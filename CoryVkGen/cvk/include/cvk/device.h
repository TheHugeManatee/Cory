#pragma once

#include "core.h"

#include <vulkan/vulkan.h>

#include <vector>
#include <memory>

namespace cvk {

class device : public basic_vk_wrapper<VkDevice> {
  public:
    explicit device(std::shared_ptr<struct VkDevice_T> vk_resource_ptr)
        : basic_vk_wrapper{std::move(vk_resource_ptr)}
    {
    }
  private:
};

}