#pragma once

#include "buffer.h"
#include "image.h"
#include "instance.h"
#include "utils.h"

#include <glm.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

// we use the forward-declared version of the type to avoid the header file
struct VmaAllocator_T;

namespace cory {
namespace vk {

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

    [[nodiscard]] device create();

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
    graphics_context(instance inst,
                     VkPhysicalDevice physical_device,
                     VkSurfaceKHR surface = nullptr,
                     VkPhysicalDeviceFeatures *requested_features = nullptr,
                     std::vector<const char *> requested_extensions = {},
                     std::vector<const char *> requested_layers = {});

    // this one should never be copied
    graphics_context(const graphics_context &) = delete;
    graphics_context &operator=(const graphics_context &) = delete;

    // moving is fine
    graphics_context(graphics_context &&) = default;
    graphics_context &operator=(graphics_context &&) = default;

    // nothing to do here!
    ~graphics_context() = default;

    image_builder image() { return image_builder{*this}; }
    buffer_builder buffer() { return buffer_builder{*this}; }

    cory::vk::image create_image(const image_builder &builder);

    cory::vk::buffer create_buffer(const buffer_builder &buffer);

    // template <typename Functor> void immediately(Functor &&f)
    //{
    //    SingleTimeCommandBuffer cmd_buffer();
    //    f(cmd_buffer.buffer());
    //}

    [[nodiscard]] const auto &graphics_queue() const noexcept { return graphics_queue_; }
    [[nodiscard]] const auto &compute_queue() const noexcept { return compute_queue_; }
    [[nodiscard]] const auto &present_queue() const noexcept { return present_queue_; }
    [[nodiscard]] const auto &transfer_queue() const noexcept { return transfer_queue_; }

    [[nodiscard]] const auto &enabled_features() const noexcept
    {
        return physical_device_features_;
    }
    [[nodiscard]] const auto &device_info() const noexcept { return physical_device_info_; }
    [[nodiscard]] auto &instance() const noexcept { return instance_; }
    [[nodiscard]] auto physical_device() const noexcept { return physical_device_info_.device; }

  private:
  private:
    cory::vk::instance instance_;
    physical_device_info physical_device_info_;
    VkPhysicalDeviceFeatures physical_device_features_{};
    device device_;

    std::optional<uint32_t> graphics_queue_family_;
    std::optional<uint32_t> transfer_queue_family_;
    std::optional<uint32_t> compute_queue_family_;
    std::optional<uint32_t> present_queue_family_;

    VkQueue graphics_queue_{};
    VkQueue transfer_queue_{};
    VkQueue compute_queue_{};
    VkQueue present_queue_{};

    std::shared_ptr<VmaAllocator_T> vma_allocator_{};
}; // namespace vk

} // namespace vk
} // namespace cory