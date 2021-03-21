#include "Cory/vk/device.h"


#include "Cory/vk/utils.h"

namespace cory::vk {

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

}