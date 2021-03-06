#include "vk/graphics_context.h"

#include "vk/enum_utils.h"

#include "Log.h"

#include <vk_mem_alloc.h>

#include <bit>
#include <iterator>
#include <set>

namespace cory {
namespace vk {

graphics_context::graphics_context(cory::vk::instance inst,
                                   VkPhysicalDevice physical_device,
                                   surface surface_khr /*= nullptr*/,
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
                physical_device, qfi, surface_.get(), &supports_present);
            if (supports_present) { present_queue_family_ = qfi; }
        }
        if (!present_queue_family_) {
            CO_CORE_ERROR("graphics_context: surface was supplied but could not find a present "
                          "queue to support it.");
        }
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
    std::vector<queue_builder> queues;
    for (uint32_t qfi : queue_families_to_instantiate)
        queues.emplace_back(queue_builder().queue_family_index(qfi).queue_priorities({1.0f}));

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

    // create the VMA allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
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

cory::vk::image graphics_context::create_image(const image_builder &builder)
{
    VkImage vkImage;
    VmaAllocation allocation;
    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = static_cast<VmaMemoryUsage>(builder.memory_usage_);
    VmaAllocationInfo allocInfo;

    // VK_CHECKED_CALL(
    vmaCreateImage(
        vma_allocator_.get(), &builder.info_, &allocCreateInfo, &vkImage, &allocation, &allocInfo);
    //   "Could not allocate image device memory from memory allocator");


    auto image_sptr = make_shared_resource(
        vkImage, [=](auto *p) { vmaDestroyImage(vma_allocator_.get(), p, allocation); });

    static_assert(std::is_same<decltype(image_sptr.get()), VkImage>::value);

    return cory::vk::image(*this,
                           std::move(image_sptr),
                           builder.name_,
                           builder.info_.imageType,
                           glm::uvec3{builder.info_.extent.width,
                                      builder.info_.extent.height,
                                      builder.info_.extent.depth},
                           builder.info_.format);
}

cory::vk::buffer graphics_context::create_buffer(const buffer_builder &builder)
{
    return cory::vk::buffer(*this, {}, builder.name_);
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