#include <cvk/instance.h>

#include <cvk/core.h>
#include <cvk/utils.h>

#include <string_view>

namespace cvk {

std::vector<const char *> instance::unsupported_extensions(const std::vector<const char *>& extensions)
{
    auto instanceExtensions =
        vk_enumerate<VkExtensionProperties>(vkEnumerateInstanceExtensionProperties, nullptr);

    std::vector<const char *> unsupportedExtensions;

    for (const auto &ext : extensions) {
        const auto match_ext = [&](const VkExtensionProperties &ep) {
            return std::string_view{ep.extensionName} == ext;
        };
        if (std::ranges::find_if(instanceExtensions, match_ext) ==
            std::ranges::end(instanceExtensions)) {
            unsupportedExtensions.push_back(ext);
        }
    }
    return unsupportedExtensions;
}

physical_device instance::device_info(VkPhysicalDevice device)
{
    physical_device dev_info;
    dev_info.device = device;
    vkGetPhysicalDeviceProperties(device, &dev_info.properties);
    vkGetPhysicalDeviceFeatures(device, &dev_info.features);

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    dev_info.queue_family_properties.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
        device, &queueFamilyCount, dev_info.queue_family_properties.data());

    dev_info.max_usable_sample_count = get_max_usable_sample_count(dev_info.properties);
    return dev_info;
}

std::vector<physical_device> instance::physical_devices()
{
    uint32_t numDevices;
    vkEnumeratePhysicalDevices(instance_ptr_.get(), &numDevices, nullptr);
    std::vector<VkPhysicalDevice> devices(numDevices);
    vkEnumeratePhysicalDevices(instance_ptr_.get(), &numDevices, devices.data());

    std::vector<physical_device> props;
    for (const auto &p : devices) {
        props.push_back(device_info(p));
    }

    return props;
}
} // namespace cvk