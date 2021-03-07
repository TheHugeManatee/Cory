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
                                   cory::vk::surface surface_khr /*= nullptr*/,
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
    if (surface_) { requested_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME); }

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
        if (family.has_value()) vkGetDeviceQueue(device_.get(), *family, 0, &queue);
    };
    get_queue(graphics_queue_family_, graphics_queue_);
    get_queue(compute_queue_family_, compute_queue_);
    get_queue(transfer_queue_family_, transfer_queue_);
    get_queue(present_queue_family_, present_queue_);

    init_allocator();

    if (surface_) { init_swapchain(); }
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
    if (surface_) {
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
    CO_APP_TRACE(R"(Instantiating queue families:)");
    CO_APP_TRACE(
        "    graphics: {} - {}",
        *graphics_queue_family_,
        flag_bits_to_string<VkQueueFlagBits>(
            physical_device_info_.queue_family_properties[*graphics_queue_family_].queueFlags));
    CO_APP_TRACE(
        "    transfer  {} - {}",
        *transfer_queue_family_,
        flag_bits_to_string<VkQueueFlagBits>(
            physical_device_info_.queue_family_properties[*transfer_queue_family_].queueFlags));
    CO_APP_TRACE(
        "    compute:  {} - {}",
        *compute_queue_family_,
        flag_bits_to_string<VkQueueFlagBits>(
            physical_device_info_.queue_family_properties[*compute_queue_family_].queueFlags));

    if (surface_) {
        CO_APP_TRACE(
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

    CO_APP_INFO("swapchain supported present modes: {}", swapchain_support.presentModes);
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

cory::vk::device device_builder::create()
{
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::transform(queue_builders_.begin(),
                   queue_builders_.end(),
                   std::back_inserter(queueCreateInfos),
                   [](queue_builder &builder) { return builder.create_info(); });

    info_.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    info_.pQueueCreateInfos = queueCreateInfos.data();

    info_.enabledLayerCount = static_cast<uint32_t>(enabled_layer_names_.size());
    info_.ppEnabledLayerNames = enabled_layer_names_.data();

    info_.enabledExtensionCount = static_cast<uint32_t>(enabled_extension_names_.size());
    info_.ppEnabledExtensionNames = enabled_extension_names_.data();

    info_.pEnabledFeatures = &enabled_features_;

    VkDevice vkDevice;
    vkCreateDevice(physical_device_, &info_, nullptr, &vkDevice);

    return make_shared_resource(vkDevice, [=](VkDevice d) { vkDestroyDevice(d, nullptr); });
}

} // namespace vk
} // namespace cory