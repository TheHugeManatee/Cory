#include "Cory/vk/device.h"

#include "Cory/vk/instance.h"
#include "Cory/vk/utils.h"

namespace cory::vk {

cory::vk::device device_builder::create()
{
    info_.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos_.size());
    info_.pQueueCreateInfos = queue_create_infos_.data();

    info_.enabledLayerCount = static_cast<uint32_t>(enabled_layer_names_.size());
    info_.ppEnabledLayerNames = enabled_layer_names_.data();

    info_.enabledExtensionCount = static_cast<uint32_t>(enabled_extension_names_.size());
    info_.ppEnabledExtensionNames = enabled_extension_names_.data();

    info_.pEnabledFeatures = &enabled_features_;

    VkDevice vkDevice;
    vkCreateDevice(device_info_.device, &info_, nullptr, &vkDevice);

    return make_shared_resource(vkDevice, [=](VkDevice d) { vkDestroyDevice(d, nullptr); });
}

device_builder &device_builder::add_queue(VkQueueFlags flags, float priority)
{
    const auto &qfps = device_info_.queue_family_properties;

    // identify a matching queue family
    auto match = [&](const auto &qfp) -> bool { return qfp.queueFlags & flags; };
    auto matching_family = std::ranges::find_if(qfps, match);

    if (matching_family == std::ranges::end(qfps)) {
        throw std::runtime_error(
            fmt::format("Could not find a matching queue for flags: {}", flags));
    }

    VkDeviceQueueCreateInfo ci{.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    ci.queueFamilyIndex = uint32_t(std::distance(qfps.cbegin(), matching_family));
    ci.queueCount = 1;
    queue_priorities_.at(queue_create_infos_.size()) = priority;
    ci.pQueuePriorities = &queue_priorities_.at(queue_create_infos_.size());

    queue_create_infos_.push_back(ci);
    return *this;
}

} // namespace cory::vk