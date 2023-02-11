#include <Cory/Renderer/VulkanUtils.hpp>

#include <Cory/Renderer/Semaphore.hpp>

#include <Magnum/Vk/Buffer.h>
#include <Magnum/Vk/CommandBuffer.h>
#include <Magnum/Vk/DescriptorSet.h>
#include <Magnum/Vk/DescriptorSetLayout.h>
#include <Magnum/Vk/Device.h>
#include <Magnum/Vk/Fence.h>
#include <Magnum/Vk/Image.h>
#include <Magnum/Vk/ImageView.h>
#include <Magnum/Vk/Instance.h>
#include <Magnum/Vk/Pipeline.h>
#include <Magnum/Vk/Queue.h>
#include <Magnum/Vk/Sampler.h>
#include <Magnum/Vk/Shader.h>

#include <type_traits>

namespace Cory {

template <typename DeviceHandle, typename VulkanObjectHandle>
void nameRawVulkanObject(DeviceHandle &device, VulkanObjectHandle handle, std::string_view name)
{
    VkDebugUtilsObjectNameInfoEXT objectNameInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = nullptr,
        .objectType = VK_OBJECT_TYPE_UNKNOWN,
        .objectHandle = (uint64_t)(handle),
        .pObjectName = name.data(),
    };

    if constexpr (std::is_same_v<VulkanObjectHandle, VkInstance>) {
        objectNameInfo.objectType = VK_OBJECT_TYPE_INSTANCE;
    }
    if constexpr (std::is_same_v<VulkanObjectHandle, VkDebugUtilsMessengerEXT>) {
        objectNameInfo.objectType = VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT;
    }
    if constexpr (std::is_same_v<VulkanObjectHandle, VkDevice>) {
        objectNameInfo.objectType = VK_OBJECT_TYPE_DEVICE;
    }
    if constexpr (std::is_same_v<VulkanObjectHandle, VkBuffer>) {
        objectNameInfo.objectType = VK_OBJECT_TYPE_BUFFER;
    }
    if constexpr (std::is_same_v<VulkanObjectHandle, VkSurfaceKHR>) {
        objectNameInfo.objectType = VK_OBJECT_TYPE_SURFACE_KHR;
    }
    if constexpr (std::is_same_v<VulkanObjectHandle, VkSwapchainKHR>) {
        objectNameInfo.objectType = VK_OBJECT_TYPE_SWAPCHAIN_KHR;
    }
    if constexpr (std::is_same_v<VulkanObjectHandle, VkQueue>) {
        objectNameInfo.objectType = VK_OBJECT_TYPE_QUEUE;
    }
    if constexpr (std::is_same_v<VulkanObjectHandle, VkSemaphore>) {
        objectNameInfo.objectType = VK_OBJECT_TYPE_SEMAPHORE;
    }
    if constexpr (std::is_same_v<VulkanObjectHandle, VkFence>) {
        objectNameInfo.objectType = VK_OBJECT_TYPE_FENCE;
    }
    if constexpr (std::is_same_v<VulkanObjectHandle, VkCommandBuffer>) {
        objectNameInfo.objectType = VK_OBJECT_TYPE_COMMAND_BUFFER;
    }
    if constexpr (std::is_same_v<VulkanObjectHandle, VkImage>) {
        objectNameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
    }
    if constexpr (std::is_same_v<VulkanObjectHandle, VkImageView>) {
        objectNameInfo.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
    }
    if constexpr (std::is_same_v<VulkanObjectHandle, VkShaderModule>) {
        objectNameInfo.objectType = VK_OBJECT_TYPE_SHADER_MODULE;
    }
    if constexpr (std::is_same_v<VulkanObjectHandle, VkPipeline>) {
        objectNameInfo.objectType = VK_OBJECT_TYPE_PIPELINE;
    }
    if constexpr (std::is_same_v<VulkanObjectHandle, VkSampler>) {
        objectNameInfo.objectType = VK_OBJECT_TYPE_SAMPLER;
    }
    if constexpr (std::is_same_v<VulkanObjectHandle, VkDescriptorSetLayout>) {
        objectNameInfo.objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT;
    }
    if constexpr (std::is_same_v<VulkanObjectHandle, VkDescriptorSet>) {
        objectNameInfo.objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET;
    }

    device->SetDebugUtilsObjectNameEXT(device, &objectNameInfo);
}

template <typename DeviceHandle, typename MagnumVulkanObjectHandle>
void nameVulkanObject(DeviceHandle &device, MagnumVulkanObjectHandle &handle, std::string_view name)
{
    nameRawVulkanObject(device, handle.handle(), name);
}

// explicitly instantiate for known types
#define INSTANTIATE(type)                                                                          \
    template void nameRawVulkanObject<Magnum::Vk::Device, type>(                                   \
        Magnum::Vk::Device & device, type handle, std::string_view name)

#define INSTANTIATE_WRAPPED(type)                                                                  \
    template void nameVulkanObject<Magnum::Vk::Device, type>(                                      \
        Magnum::Vk::Device & device, type & handle, std::string_view name)

INSTANTIATE(VkDebugUtilsMessengerEXT);
INSTANTIATE_WRAPPED(Magnum::Vk::Instance);
INSTANTIATE_WRAPPED(Magnum::Vk::Device);
INSTANTIATE_WRAPPED(Magnum::Vk::Buffer);
INSTANTIATE_WRAPPED(BasicVkObjectWrapper<VkSurfaceKHR>);
INSTANTIATE(VkSwapchainKHR);
INSTANTIATE_WRAPPED(Magnum::Vk::Queue);
INSTANTIATE_WRAPPED(BasicVkObjectWrapper<VkSemaphore>);
INSTANTIATE_WRAPPED(Magnum::Vk::Fence);
INSTANTIATE_WRAPPED(Magnum::Vk::CommandBuffer);
INSTANTIATE(VkImage);
INSTANTIATE_WRAPPED(Magnum::Vk::Image);
INSTANTIATE_WRAPPED(Magnum::Vk::Pipeline);
INSTANTIATE_WRAPPED(Magnum::Vk::ImageView);
INSTANTIATE_WRAPPED(Magnum::Vk::Shader);
INSTANTIATE_WRAPPED(Magnum::Vk::Sampler);
INSTANTIATE_WRAPPED(Magnum::Vk::DescriptorSetLayout);
INSTANTIATE_WRAPPED(Magnum::Vk::DescriptorSet);

} // namespace Cory