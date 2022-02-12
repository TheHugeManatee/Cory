#pragma once

#include "physical_device.h"

#include <vulkan/vulkan.h>

#include <memory>
#include <utility>
#include <vector>

namespace cvk {

class instance {
  public:
    explicit instance(std::shared_ptr<struct VkInstance_T> instance_ptr)
        : instance_ptr_{std::move(instance_ptr)}
    {
    }

    /**
     * @brief list info about all physical devices
     * @return
     */
    std::vector<physical_device> physical_devices();

    VkInstance get() { return instance_ptr_.get(); }

    /// figure out if any of the given extensions are unsupported
    static std::vector<const char *>
    unsupported_extensions(const std::vector<const char *> &extensions);

  private:
    physical_device device_info(VkPhysicalDevice device);

    std::shared_ptr<struct VkInstance_T> instance_ptr_;
};

} // namespace cvk