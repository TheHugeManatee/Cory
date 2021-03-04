#include "vk/graphics_context.h"

#include "vk/enum_utils.h"

#include "Log.h"

#include <vk_mem_alloc.h>

#include <bit>
#include <iterator>

namespace cory {
namespace vk {

graphics_context::graphics_context(cory::vk::instance inst,
                                   VkPhysicalDevice physical_device,
                                   VkSurfaceKHR surface /*= nullptr*/,
                                   VkPhysicalDeviceFeatures *requested_features /*= nullptr*/,
                                   std::vector<const char *> requested_extensions /*= {}*/,
                                   std::vector<const char *> requested_layers /*= {}*/)
    : instance_{inst}
    , physical_device_info_{inst.device_info(physical_device)}
{
    if (requested_features) physical_device_features_ = *requested_features;

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

    if (surface) {
        // just pick the last queue that can present
        for (int qfi = 0; qfi < qfi_props.size(); ++qfi) {
            VkBool32 supports_present;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, qfi, surface, &supports_present);
            if (supports_present) { present_queue_family_ = qfi; }
        }
        if (!present_queue_family_) {
            CO_CORE_ERROR("graphics_context: surface was supplied but could not find a present "
                          "queue to support it.");
        }
    }

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

    // figure out which queues we need to create
    std::vector<queue_builder> queues{
        queue_builder().queue_family_index(*graphics_queue_family_).queue_priorities({1.0f}),
        queue_builder().queue_family_index(*transfer_queue_family_).queue_priorities({1.0f})};
    if (compute_queue_family_.has_value())
        queues.emplace_back(
            queue_builder().queue_family_index(*compute_queue_family_).queue_priorities({1.0f}));

    // create the device with from the physical device and the extensions
    device_ = device_builder(physical_device)
                  .queue_create_infos(queues)
                  .enabled_features(physical_device_features_)
                  .enabled_extension_names(requested_extensions)
                  .enabled_layer_names(requested_layers)
                  .create();

    // query the actual queues to keep a reference
    auto get_queue = [&](auto &family, auto &queue) {
        if (family.has_value()) vkGetDeviceQueue(device_.get(), *family, 0, &queue);
    };
    get_queue(graphics_queue_family_, graphics_queue_);
    get_queue(compute_queue_family_, compute_queue_);
    get_queue(transfer_queue_family_, transfer_queue_);

    // create the allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocatorInfo.physicalDevice = physical_device_info_.device;
    allocatorInfo.device = device_.get();
    allocatorInfo.instance = instance_.get();

    vmaCreateAllocator(&allocatorInfo, &vma_allocator_);
}

graphics_context::~graphics_context() { vmaDestroyAllocator(vma_allocator_); }

cory::vk::image graphics_context::create_image(const image_builder &builder)
{
    VkImage vkImage;
    VmaAllocation allocation;
    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = static_cast<VmaMemoryUsage>(builder.memory_usage_);
    VmaAllocationInfo allocInfo;

    VK_CHECKED_CALL(
        vmaCreateImage(
            vma_allocator_, &builder.info_, &allocCreateInfo, &vkImage, &allocation, &allocInfo),
        "Could not allocate image device memory from memory allocator");

    // create a shared pointer to VkImage_T (because VkImage_T* == VkImage)
    // with a custom deallocation function that destroys the image in the VMA
    // this way we get reference-counted
    std::shared_ptr<VkImage_T> image_sptr(
        vkImage, [=](auto *p) { vmaDestroyImage(vma_allocator_, p, allocation); });

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

    VkDevice device;
    vkCreateDevice(physical_device_, &info_, nullptr, &device);

    std::shared_ptr<struct VkDevice_T> device_ptr{device,
                                                  [=](VkDevice d) { vkDestroyDevice(d, nullptr); }};

    return device_ptr;
}

} // namespace vk
} // namespace cory