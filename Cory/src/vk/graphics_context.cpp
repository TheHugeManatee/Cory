#include "vk/graphics_context.h"

#include "Log.h"

#include "utils/container.h"
#include "vk/enum_utils.h"

#include <fmt/ranges.h>
#include <vk_mem_alloc.h>

#include <bit>
#include <iterator>
#include <set>

namespace cory {
namespace vk {

graphics_context::graphics_context(cory::vk::instance inst,
                                   VkPhysicalDevice physical_device,
                                   cory::vk::surface surface_khr /*= {} */,
                                   VkPhysicalDeviceFeatures *requested_features /*= nullptr*/,
                                   std::vector<const char *> requested_extensions /*= {}*/,
                                   std::vector<const char *> requested_layers /*= {}*/)
    : instance_{inst}
    , physical_device_info_{inst.device_info(physical_device)}
    , surface_{surface_khr}
{
    // if not explicitly supplied, enable all features. :)
    if (requested_features) { physical_device_features_ = *requested_features; }
    else {
        physical_device_features_ = physical_device_info_.features;
    }

    // if we got a surface, make sure we enable the required extension
    if (surface_.has_value()) { requested_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME); }

    // figure out which queue families we want to use for what
    const std::set<uint32_t> queue_families_to_instantiate = configure_queue_families();

    // pre-create the builders for each queue
    const std::vector<queue_builder> queues = [&]() {
        std::vector<queue_builder> q;
        for (uint32_t qfi : queue_families_to_instantiate)
            q.emplace_back(queue_builder().queue_family_index(qfi).queue_priorities({1.0f}));
        return q;
    }();

    // create the device with from the physical device and the extensions
    device_ = device_builder(physical_device)
                  .queue_create_infos(queues)
                  .enabled_features(physical_device_features_)
                  .enabled_extension_names(requested_extensions)
                  .enabled_layer_names(requested_layers)
                  .create();

    // get references to the actual queues so we can reference them later
    auto get_queue = [&](auto &family, auto &queue) {
        VkQueue vk_queue{};
        if (family.has_value()) {
            vkGetDeviceQueue(device_.get(), *family, 0, &vk_queue);
            queue.set_queue(vk_queue, *family);
        }
    };
    get_queue(graphics_queue_family_, graphics_queue_);
    get_queue(compute_queue_family_, compute_queue_);
    get_queue(transfer_queue_family_, transfer_queue_);
    get_queue(present_queue_family_, present_queue_);

    init_allocator();

    if (surface_.has_value()) { init_swapchain(); }

    // determine best depth format
    default_depth_stencil_format_ =
        find_supported_format(physical_device_info_.device,
                              {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                              VK_IMAGE_TILING_OPTIMAL,
                              VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    if (surface_.has_value())
        default_color_format_ = swapchain_->format();
    else {
        // FIXME: this is the next-best thing that was supported on my system, need to figure
        // out the best way to determine a supported format, i.e. by querying the device's supported
        // formats and picking the "best" RGB format
        default_color_format_ = VK_FORMAT_B8G8R8A8_SRGB;
    }

    max_msaa_samples_ = get_max_usable_sample_count(physical_device_info_.properties);
}

void graphics_context::init_allocator()
{
    // create the VMA allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    // TODO: the vulkan version should come from the instance, as the instance/device might not
    // actually support 1.2
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocatorInfo.physicalDevice = physical_device_info_.device;
    allocatorInfo.device = device_.get();
    allocatorInfo.instance = instance_.get();

    VmaAllocator vmaAllocator;
    VK_CHECKED_CALL(vmaCreateAllocator(&allocatorInfo, &vmaAllocator),
                    "Could not create VMA allocator object");
    // wrap the allocator with a shared_ptr with a custom deallocator
    vma_allocator_ =
        make_shared_resource(vmaAllocator, [](VmaAllocator alloc) { vmaDestroyAllocator(alloc); });
}

std::set<uint32_t> graphics_context::configure_queue_families()
{
    // find the default queue families for graphics, transfer and compute
    // we use a scoring function that ranks eligible families by their amount of
    // specialization: the more other bits they have (i.e. the more other features the
    // family supports) the less attractive they are (because they might overlap with other
    // computations).
    // No idea if this makes sense at all, but I finally found a use for std::popcount! :)
    auto scoring_func = [](VkQueueFlagBits queue_flags) {
        return [=](const VkQueueFamilyProperties &qfp) {
            if (qfp.queueFlags & queue_flags) { return 32 - std::popcount(qfp.queueFlags); }
            return 0;
        };
    };
    const auto &qfi_props = physical_device_info_.queue_family_properties;
    graphics_queue_family_ = find_best_queue_family(qfi_props, scoring_func(VK_QUEUE_GRAPHICS_BIT));
    transfer_queue_family_ = find_best_queue_family(qfi_props, scoring_func(VK_QUEUE_TRANSFER_BIT));
    compute_queue_family_ = find_best_queue_family(qfi_props, scoring_func(VK_QUEUE_COMPUTE_BIT));

    // if we were passed a surface, try to initialize a present queue for it. no fancy
    // selection logic yet, just pick whatever works.
    if (surface_.has_value()) {
        VkBool32 supports_present;
        for (int qfi = 0; qfi < qfi_props.size(); ++qfi) {
            vkGetPhysicalDeviceSurfaceSupportKHR(
                physical_device_info_.device, qfi, surface_.get(), &supports_present);
            if (supports_present) { present_queue_family_ = qfi; }
        }

        CO_CORE_ASSERT(present_queue_family_.has_value(),
                       "graphics_context: surface was supplied but could not find a present "
                       "queue to support it.");
    }

    // log this out, you never know when this might be interesting..
    CO_CORE_TRACE(R"(Instantiating queue families:)");
    CO_CORE_TRACE(
        "    graphics: {} - {}",
        *graphics_queue_family_,
        flag_bits_to_string<VkQueueFlagBits>(
            physical_device_info_.queue_family_properties[*graphics_queue_family_].queueFlags));
    CO_CORE_TRACE(
        "    transfer  {} - {}",
        *transfer_queue_family_,
        flag_bits_to_string<VkQueueFlagBits>(
            physical_device_info_.queue_family_properties[*transfer_queue_family_].queueFlags));
    CO_CORE_TRACE(
        "    compute:  {} - {}",
        *compute_queue_family_,
        flag_bits_to_string<VkQueueFlagBits>(
            physical_device_info_.queue_family_properties[*compute_queue_family_].queueFlags));

    if (surface_.has_value()) {
        CO_CORE_TRACE(
            "    present:  {} - {}",
            *present_queue_family_,
            flag_bits_to_string<VkQueueFlagBits>(
                physical_device_info_.queue_family_properties[*present_queue_family_].queueFlags));
    }

    // figure out which queues we need to create - we always expect graphics and transfer queues
    // (i guess there might be compute-only devices but we don't care about those weirdos)
    std::set<uint32_t> queue_families_to_instantiate;
    queue_families_to_instantiate.insert(*graphics_queue_family_);
    queue_families_to_instantiate.insert(*transfer_queue_family_);

    if (compute_queue_family_) queue_families_to_instantiate.insert(*compute_queue_family_);
    if (present_queue_family_) queue_families_to_instantiate.insert(*present_queue_family_);
    return queue_families_to_instantiate;
}

void graphics_context::init_swapchain()
{
    // get the capabilities of the swapchain
    const auto swapchain_support =
        cory::vk::query_swap_chain_support(physical_device_info_.device, surface_.get());

    CO_CORE_INFO("swapchain supported present modes: {}", swapchain_support.presentModes);
    for (const auto &surfaceFmt : swapchain_support.formats)
        CO_CORE_DEBUG(
            "swapchain supported format: {}, {}", surfaceFmt.format, surfaceFmt.colorSpace);
    // CO_APP_INFO("Swap chain supports formats: {}", fmt::join(formats, ","));

    // get the present mode
    auto present_mode = utils::find_or(
        swapchain_support.presentModes, VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR);

    // get the surface format - BGRA8 with nonlinear sRGB is the preferred option
    auto surface_format = utils::find_or(
        swapchain_support.formats,
        [](const auto &fmt) {
            return fmt.format == VK_FORMAT_B8G8R8A8_SRGB &&
                   fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        },
        swapchain_support.formats[0]);

    // determine the swapchain extent
    glm::uvec2 swapchain_extent;
    if (swapchain_support.capabilities.currentExtent.width != UINT32_MAX) {
        swapchain_extent = glm::uvec2{swapchain_support.capabilities.currentExtent.width,
                                      swapchain_support.capabilities.currentExtent.height};
    }
    else {
        swapchain_extent = glm::uvec2{800, 600};

        // NOTE: this is where we would ideally query the window manager for the window size.
        //       however, the current API does not allow passing such a size, so we have to fall
        //       back to a fixed window size, which might not look great and might not work at
        //       all on some platforms

        CO_CORE_WARN("Surface did not supply a preferred swapchain extent. Falling back to a "
                     "default resolution of {}x{}. This can lead to unexpected results.",
                     swapchain_extent.x,
                     swapchain_extent.y);
    }

    auto swch_builder = swapchain_builder(*this)
                            .surface(surface_.get())
                            .min_image_count(3) // triple buffering works well for most applications
                            .present_mode(present_mode)
                            .image_format(surface_format.format)
                            .image_color_space(surface_format.colorSpace)
                            .image_extent(swapchain_extent)
                            .pre_transform(swapchain_support.capabilities.currentTransform);

    // if the swap and present queues are different, the swap chain images have to
    // be shareable
    if (graphics_queue_family_ == present_queue_family_) {
        swch_builder.image_sharing_mode(VK_SHARING_MODE_CONCURRENT)
            .queue_family_indices({graphics_queue_family_.value(), present_queue_family_.value()});
    }
    else {
        // otherwise, exclusive has better performance
        swch_builder.image_sharing_mode(VK_SHARING_MODE_EXCLUSIVE).queue_family_indices({});
    }

    swapchain_.emplace(swch_builder.create());
}

cory::vk::fence graphics_context::fence(VkFenceCreateFlags flags /*= {}*/)
{
    // fixme: we probably want a pool here!
    VkFenceCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = flags};

    VkFence created_fence;
    VK_CHECKED_CALL(vkCreateFence(device_.get(), &create_info, nullptr, &created_fence),
                    "failed to create a fence object");

    auto vk_resource_ptr = make_shared_resource(
        created_fence, [dev = device_.get()](VkFence f) { vkDestroyFence(dev, f, nullptr); });

    return {*this, vk_resource_ptr};
}

cory::vk::semaphore graphics_context::semaphore(VkSemaphoreCreateFlags flags /*= {}*/)
{
    VkSemaphoreCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                      .flags = flags};

    VkSemaphore created_semaphore;
    VK_CHECKED_CALL(vkCreateSemaphore(device_.get(), &create_info, nullptr, &created_semaphore),
                    "failed to create a semaphore object");
    return make_shared_resource(created_semaphore, [dev = device_.get()](VkSemaphore f) {
        vkDestroySemaphore(dev, f, nullptr);
    });
}

cory::vk::queue &graphics_context::queue(cory::vk::queue_type requested_type) noexcept
{
    switch (requested_type) {
    case cory::vk::queue_type::graphics:
        return graphics_queue_;
    case cory::vk::queue_type::compute:
        return compute_queue_;
    case cory::vk::queue_type::transfer:
        return transfer_queue_;
    case cory::vk::queue_type::present:
        return present_queue_;
        // no default case so compiler will warn about missing cases
    }
}

} // namespace vk
} // namespace cory