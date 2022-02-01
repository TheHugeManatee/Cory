#pragma once

#include "buffer.h"
#include "command_buffer.h"
#include "command_pool.h"
#include "device.h"
#include "fence.h"
#include "image.h"
#include "../../../../CoryVkGen/cvk/include/cvk/instance.h"
#include "misc.h"
#include "queue.h"
#include "swapchain.h"
#include "utils.h"

#include <glm.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string_view>

// we use the forward-declared VulkanMemoryAllocator type
struct VmaAllocator_T;

namespace cory::vk {

class graphics_context {
  public:
    // == lifetime ==
    graphics_context(instance inst,
                     VkPhysicalDevice physical_device,
                     surface surface_khr = {},
                     VkPhysicalDeviceFeatures *requested_features = nullptr,
                     std::vector<const char *> requested_extensions = {},
                     std::vector<const char *> requested_layers = {});

    // nothing to do here!
    ~graphics_context() = default;

    // === basic copy&move interface ===
    // this one should never be copied
    graphics_context(const graphics_context &) = delete;
    graphics_context &operator=(const graphics_context &) = delete;

    // moving is fine
    graphics_context(graphics_context &&) = default;
    graphics_context &operator=(graphics_context &&) = default;

    // ===  resource creation ===
    image_builder build_image() { return {*this}; }
    buffer_builder build_buffer() { return {*this}; }
    image_view_builder build_image_view(const cory::vk::image &img) { return {*this, img}; }

    /// creates a new fence
    cory::vk::fence fence(VkFenceCreateFlags flags = {});
    /// creates a new semaphore
    cory::vk::semaphore semaphore(VkSemaphoreCreateFlags = {});

    // === command buffer submission ===

    template <typename Functor>
    executable_command_buffer record(Functor f, cory::vk::queue &target_queue)
    {
        return graphics_queue_.record(std::forward<Functor>(f));
    }

    // template <typename PooledObject> [[nodiscard]] constexpr auto &pool() noexcept
    //{
    //    if constexpr (std::is_same_v<PooledObject, command_pool>) { return command_pool_pool_ ;}
    //}

    // === access to the queues ===
    [[nodiscard]] cory::vk::queue &graphics_queue() noexcept { return graphics_queue_; }
    [[nodiscard]] cory::vk::queue &compute_queue() noexcept { return compute_queue_; }
    [[nodiscard]] cory::vk::queue &present_queue() noexcept { return present_queue_; }
    [[nodiscard]] cory::vk::queue &transfer_queue() noexcept { return transfer_queue_; }

    inline cory::vk::queue &queue(cory::vk::queue_type requested_type) noexcept;

    // === direct access to the basic vulkan entities ===
    [[nodiscard]] const auto &enabled_features() const noexcept
    {
        return physical_device_features_;
    }
    [[nodiscard]] const auto &device_info() const noexcept { return physical_device_info_; }
    [[nodiscard]] auto &instance() const noexcept { return instance_; }
    [[nodiscard]] auto physical_device() const noexcept { return physical_device_info_.device; }
    [[nodiscard]] auto device() noexcept -> VkDevice { return device_.get(); };
    [[nodiscard]] auto allocator() noexcept { return vma_allocator_.get(); }
    [[nodiscard]] auto &surface() const noexcept { return surface_; }
    [[nodiscard]] auto &swapchain() noexcept { return swapchain_; }

    [[nodiscard]] auto max_msaa_samples() const noexcept { return max_msaa_samples_; }
    [[nodiscard]] auto default_color_format() const noexcept { return default_color_format_; }
    [[nodiscard]] auto default_depth_stencil_format() const noexcept
    {
        return default_depth_stencil_format_;
    }

  private:
    std::set<uint32_t> configure_queue_families();
    void init_allocator();
    void init_swapchain();

  private:
    cory::vk::instance instance_;
    physical_device_info physical_device_info_;
    VkPhysicalDeviceFeatures physical_device_features_{};
    cory::vk::device device_;
    cory::vk::surface surface_;

    std::optional<uint32_t> graphics_queue_family_;
    std::optional<uint32_t> transfer_queue_family_;
    std::optional<uint32_t> compute_queue_family_;
    std::optional<uint32_t> present_queue_family_;

    cory::vk::queue graphics_queue_{*this, "graphics"};
    cory::vk::queue transfer_queue_{*this, "transfer"};
    cory::vk::queue compute_queue_{*this, "compute"};
    cory::vk::queue present_queue_{*this, "present"};

    std::shared_ptr<VmaAllocator_T> vma_allocator_{};

    std::optional<cory::vk::swapchain> swapchain_;

    VkSampleCountFlagBits max_msaa_samples_;
    VkFormat default_color_format_;
    VkFormat default_depth_stencil_format_;

    // pools
    // command_pool_pool command_pool_pool_;
};
} // namespace cory::vk

// =====================================================================
