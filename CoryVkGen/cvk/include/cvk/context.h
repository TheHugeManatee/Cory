#pragma once

#include "core.h"
#include "device.h"
#include "fence.h"
#include "instance.h"
#include "physical_device.h"
#include "semaphore.h"
#include "utils.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string_view>

// forward-declare VulkanMemoryAllocator type
struct VmaAllocator_T;

namespace cvk {

/// wrapper for a VkSurface
using surface = basic_vk_wrapper<VkSurfaceKHR>;

class queue;
class swapchain;

struct context_queues {
    ~context_queues();
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> transfer_family;
    std::optional<uint32_t> compute_family;
    std::optional<uint32_t> present_family;

    cvk::queue *graphics{};
    cvk::queue *transfer{};
    cvk::queue *compute{};
    cvk::queue *present{};

    std::vector<std::unique_ptr<cvk::queue>> storage;
};

class context {
  public:
    // == lifetime ==
    context(instance inst,
            surface surface_khr = surface{},
            VkPhysicalDeviceFeatures *requested_features = nullptr,
            std::vector<const char *> requested_extensions = {},
            std::vector<const char *> requested_layers = {});

    ~context();

    // === basic copy&move interface ===
    context(const context &) = delete;
    context &operator=(const context &) = delete;
    context(context &&) = default;
    context &operator=(context &&) = default;

    /// creates a new fence
    fence create_fence(VkFenceCreateFlags flags = {});
    /// creates a new semaphore
    semaphore create_semaphore(VkSemaphoreCreateFlags = {});

    // === command buffer submission ===

    //    template <typename Functor>
    //    executable_command_buffer record(Functor f, cory::vk::queue &target_queue)
    //    {
    //        return graphics_queue_.record(std::forward<Functor>(f));
    //    }

    // template <typename PooledObject> [[nodiscard]] constexpr auto &pool() noexcept
    //{
    //    if constexpr (std::is_same_v<PooledObject, command_pool>) { return command_pool_pool_ ;}
    //}

    // === access to the queues ===
    [[nodiscard]] queue &graphics_queue() noexcept { return *queues_.graphics; }
    [[nodiscard]] queue &compute_queue() noexcept { return *queues_.compute; }
    [[nodiscard]] queue &present_queue() noexcept { return *queues_.present; }
    [[nodiscard]] queue &transfer_queue() noexcept { return *queues_.transfer; }

    // inline cvk::queue &queue(cvk::queue_type requested_type) noexcept;

    // === direct access to the basic vulkan entities ===
    [[nodiscard]] const auto &enabled_features() const noexcept
    {
        return physical_device_features_;
    }
    //[[nodiscard]] const auto &device_info() const noexcept { return physical_device_; }
    //[[nodiscard]] auto &instance() const noexcept { return instance_; }
    [[nodiscard]] auto vk_physical_device() const noexcept { return physical_device_.device; }
    [[nodiscard]] auto vk_surface() noexcept -> VkSurfaceKHR { return surface_.get(); };
    [[nodiscard]] auto vk_device() noexcept -> VkDevice { return device_.get(); };
    [[nodiscard]] auto vk_allocator() noexcept { return vma_allocator_.get(); }

    //    [[nodiscard]] auto max_msaa_samples() const noexcept { return max_msaa_samples_; }
    //    [[nodiscard]] auto default_color_format() const noexcept { return default_color_format_; }
    //    [[nodiscard]] auto default_depth_stencil_format() const noexcept
    //    {
    //        return default_depth_stencil_format_;
    //    }

    // query which formats and color spaces are supported by the device+surface combination
    swap_chain_support query_swap_chain_support();

  private:
    // figures out which queues to create and returns the respective family indices
    std::set<uint32_t> configure_queue_families();
    cvk::physical_device pick_device();
    cvk::device create_device(std::vector<const char *> requested_extensions,
                              std::vector<const char *> requested_layers);
    void setup_queues(const std::set<uint32_t> &queue_families, VkDevice device);
    std::shared_ptr<VmaAllocator_T> init_allocator();
    void init_swapchain();

  private:
    cvk::instance instance_;
    physical_device physical_device_;
    VkPhysicalDeviceFeatures physical_device_features_{};
    cvk::surface surface_;
    context_queues queues_;
    cvk::device device_;

    std::shared_ptr<VmaAllocator_T> vma_allocator_{};

    std::unique_ptr<swapchain> swapchain_;

    //    VkSampleCountFlagBits max_msaa_samples_;
    //    VkFormat default_color_format_;
    //    VkFormat default_depth_stencil_format_;

    // pools
    // command_pool_pool command_pool_pool_;
};
} // namespace cvk

// =====================================================================
