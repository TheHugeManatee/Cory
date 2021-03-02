#pragma once

#include "instance.h"
#include "resource.h"
#include "utils.h"

#include <glm.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <iterator>
#include <memory>
#include <string_view>

namespace cory {
namespace vk {

class image_builder;
class buffer_builder;

class device {
  public:
    device() = default;
    device(std::shared_ptr<struct VkDevice_T> device_ptr)
        : device_ptr_{device_ptr}
    {
    }

    VkDevice get() { return device_ptr_.get(); }

  private:
    std::shared_ptr<struct VkDevice_T> device_ptr_;
};

class queue_builder {
  public:
    friend class device_builder;

    [[nodiscard]] queue_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    [[nodiscard]] queue_builder &flags(VkDeviceQueueCreateFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    [[nodiscard]] queue_builder &queue_family_index(uint32_t queueFamilyIndex) noexcept
    {
        info_.queueFamilyIndex = queueFamilyIndex;
        return *this;
    }

    [[nodiscard]] queue_builder &queue_priorities(std::vector<float> queuePriorities) noexcept
    {
        queue_priorities_ = queuePriorities;
        return *this;
    }

  private:
    [[nodiscard]] auto create_info()
    {
        info_.queueCount = static_cast<uint32_t>(queue_priorities_.size());
        info_.pQueuePriorities = queue_priorities_.data();
        return info_;
    }
    VkDeviceQueueCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    };
    std::vector<float> queue_priorities_;
};

class device_builder {
  public:
    device_builder(VkPhysicalDevice physical_device)
        : physical_device_{physical_device}
    {
    }

    [[nodiscard]] device_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    [[nodiscard]] device_builder &flags(VkDeviceCreateFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    [[nodiscard]] device_builder &
    queue_create_infos(std::vector<queue_builder> queueCreateInfos) noexcept
    {
        queue_builders_ = queueCreateInfos;
        return *this;
    }

    [[nodiscard]] device_builder &
    enabled_layer_names(std::vector<const char *> enabledLayerNames) noexcept
    {
        enabled_layer_names_ = enabledLayerNames;
        return *this;
    }

    [[nodiscard]] device_builder &
    enabled_extension_names(std::vector<const char *> enabledExtensionNames) noexcept
    {
        enabled_extension_names_ = enabledExtensionNames;
        return *this;
    }

    [[nodiscard]] device_builder &
    enabled_features(VkPhysicalDeviceFeatures enabledFeatures) noexcept
    {
        enabled_features_ = enabledFeatures;
        return *this;
    }

    [[nodiscard]] device create()
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

        std::shared_ptr<struct VkDevice_T> device_ptr{
            device, [=](VkDevice d) { vkDestroyDevice(d, nullptr); }};

        return device_ptr;
    }

  private:
    VkPhysicalDevice physical_device_;
    VkDeviceCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    };
    std::vector<queue_builder> queue_builders_;
    std::vector<const char *> enabled_extension_names_;
    std::vector<const char *> enabled_layer_names_;
    VkPhysicalDeviceFeatures enabled_features_;
};

class graphics_context {
  public:
    graphics_context(instance inst, VkPhysicalDevice physical_device)
        : instance_{inst}
        , physical_device_info_{inst.device_info(physical_device)}
    {
        // TODO
        device_ = device_builder(physical_device).create();

        // device_builder()
        //    .queue(queue_builder().queue_family_index(1).queue_priorities({1.0f}))
        //    .enabled_features();
    }

    // this one should never be copied
    graphics_context(const graphics_context &) = delete;
    graphics_context &operator=(const graphics_context &) = delete;

    // moving is fine
    graphics_context(graphics_context &&) = default;
    graphics_context &operator=(graphics_context &&) = default;

    ~graphics_context() {}

    // image_builder image() { return image_builder{*this}; }
    // buffer_builder buffer() { return buffer_builder{*this}; }

    cory::vk::image create_image(const image_builder &builder);

    cory::vk::buffer create_buffer(const buffer_builder &buffer);

    // template <typename Functor> void immediately(Functor &&f)
    //{
    //    SingleTimeCommandBuffer cmd_buffer();
    //    f(cmd_buffer.buffer());
    //}

    const auto &enabled_features() const { return physical_device_features_; }
    const auto &device_info() const { return physical_device_info_; }
    auto &instance() const { return instance_; }
    auto physical_device() const { return physical_device_info_.device; }

  private:
    cory::vk::instance instance_;
    physical_device_info physical_device_info_;
    VkPhysicalDeviceFeatures physical_device_features_;
    device device_;

    // we use the forward-declared version of the type to avoid the header file
    struct VmaAllocator_T *vma_allocator_;
};

} // namespace vk
} // namespace cory