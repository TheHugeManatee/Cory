#include <cvk/device_builder.h>

#include "device.h"
#include "utils.h"

#include <cvk/log.h>

namespace cvk {

cvk::device device_builder::create()
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

    //    std::vector<VkQueue> queues{queue_create_infos_.size()};
    //    std::ranges::generate(queues, [&vkDevice, this, i = 0]() mutable {
    //        VkQueue q;
    //        const auto &qci = queue_create_infos_[i++];
    //        vkGetDeviceQueue(vkDevice, qci.queueFamilyIndex, 0, &q);
    //        return q;
    //    });

    auto sr = cvk::device{
        make_shared_resource(vkDevice, [=](VkDevice d) { vkDestroyDevice(d, nullptr); })};

    return sr;
}

device_builder &device_builder::add_queues(const std::set<uint32_t> &familyIndices)
{
    CVK_ASSERT(queue_create_infos_.empty(), "Multiple calls to add_queues not allowed");
    for (uint32_t familyIndex : familyIndices) {
        VkDeviceQueueCreateInfo ci{.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        ci.queueFamilyIndex = familyIndex;
        ci.queueCount = 1;
        queue_priorities_.at(queue_create_infos_.size()) = 1.0f;
        ci.pQueuePriorities = &queue_priorities_.at(queue_create_infos_.size());

        queue_create_infos_.push_back(ci);
    }
    return *this;
}

} // namespace cvk