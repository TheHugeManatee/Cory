#pragma once

#include "command_buffer.h"

#include <vulkan/vulkan.h>

#include "Cory/utils/algorithms.h"

#include <map>
#include <memory>
#include <string_view>

namespace cory {
namespace vk {

class graphics_context;
class command_pool_builder;

// template <typename Derived, typename StoredVkType> class pooled_resource {
//  public:
//    using ResourcePtrType = std::shared_ptr<std::remove_pointer_t<StoredVkType>>;
//
//    pooled_resource(uint64_t resource_key, ResourcePtrType resource_ptr)
//        : resource_key_{resource_key}
//        , resource_ptr_{resource_ptr}
//    {
//    }
//    [[nodiscard]] auto key() const noexcept { return resource_key_; }
//    [[nodiscard]] auto get() const noexcept { return resource_ptr_.get(); }
//
//  private:
//    uint64_t resource_key_{};
//    ResourcePtrType resource_ptr_{};
//};

class command_pool /*: public pooled_resource<command_pool, VkCommandPool>*/ {
  public:
    using ResourcePtrType = std::shared_ptr<VkCommandPool_T>;

    explicit command_pool(graphics_context &ctx, ResourcePtrType command_pool_ptr)
        : ctx_{ctx}
        , command_pool_ptr_{command_pool_ptr}
    {
    }

    void reset(VkCommandPoolResetFlags flags = {});

    [[nodiscard]] VkCommandPool get() const noexcept { return command_pool_ptr_.get(); }

    cory::vk::command_buffer allocate_buffer();

  private:
    graphics_context &ctx_;
    ResourcePtrType command_pool_ptr_;
};

class command_pool_builder {
  public:
    friend class command_pool;
    friend class command_pool_pool;

    command_pool_builder(graphics_context &ctx)
        : ctx_{ctx}
    {
    }

    command_pool_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    command_pool_builder &flags(VkCommandPoolCreateFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    command_pool_builder &queue_family_index(uint32_t queueFamilyIndex) noexcept
    {
        info_.queueFamilyIndex = queueFamilyIndex;
        return *this;
    }

    command_pool_builder &name(std::string_view name) noexcept
    {
        name_ = name;
        return *this;
    }
    [[nodiscard]] command_pool create();

  private:
    graphics_context &ctx_;
    VkCommandPoolCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    };
    std::string_view name_;
};

// class command_pool_pool {
//  public:
//
//    [[nodiscard]] std::uint64_t hash(const command_pool_builder &builder) const noexcept
//    {
//        return builder.info_.flags ^ builder.info_.queueFamilyIndex;
//    }
//
//    [[nodiscard]] command_pool get(const command_pool_builder &builder);
//
//    // recycle a finished command_pool back into the pool. will reset the pool.
//    void recycle(command_pool &&pool)
//    {
//        pool.reset();
//        available_elements_.insert({pool.key(), pool});
//    }
//
//    void clear() { available_elements_.clear(); }
//
//    size_t available() { return available_elements_.size(); }
//
//  private:
//    std::multimap<std::uint64_t, command_pool> available_elements_;
//};

} // namespace vk
} // namespace cory
