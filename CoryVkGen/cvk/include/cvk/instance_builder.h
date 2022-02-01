#pragma once

#include "instance.h"

namespace cvk {
class instance_builder {
  public:
    friend class graphics_context;

    [[nodiscard]] instance_builder &next(const void *pNext)
    {
        info_.pNext = pNext;
        return *this;
    }

    [[nodiscard]] instance_builder &flags(VkInstanceCreateFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    [[nodiscard]] instance_builder &application_info(VkApplicationInfo applicationInfo) noexcept
    {
        application_info_ = applicationInfo;
        return *this;
    }

    [[nodiscard]] instance_builder &enabled_layers(std::vector<const char *> enabledLayers) noexcept
    {
        enabled_layers_ = std::move(enabledLayers);
        return *this;
    }

    [[nodiscard]] instance_builder &
    enabled_extensions(std::vector<const char *> enabledExtensions) noexcept
    {
        enabled_extensions_ = std::move(enabledExtensions);
        return *this;
    }

    [[nodiscard]] instance create();

  private:
    VkInstanceCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    };
    std::vector<const char *> enabled_extensions_;
    std::vector<const char *> enabled_layers_;
    VkApplicationInfo application_info_;
};
} // namespace cvk