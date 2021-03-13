#pragma once

#include <vulkan/vulkan.h>

namespace cory {
namespace vk {

class graphics_context;
class command_pool_builder;

class command_pool {
  public:
    explicit command_pool(command_pool_builder &builder) {}

  private:
};

class command_pool_builder {
  public:
    friend class command_pool;

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
    [[nodiscard]] command_pool create() { return command_pool(*this); }

  private:
    VkCommandPoolCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    };
    std::string_view name_;
};

} // namespace vk
} // namespace cory