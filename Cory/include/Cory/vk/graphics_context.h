#pragma once

#include "resource.h"
#include "utils.h"

#include <glm.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <string_view>

namespace cory {
namespace vk {

class image_builder;
class buffer_builder;

class graphics_context {
  public:
    graphics_context()
    {
        // TODO
    }

    // image_builder image() { return image_builder{*this}; }
    // buffer_builder buffer() { return buffer_builder{*this}; }

    cory::vk::image create_image(const image_builder &builder);

    cory::vk::buffer create_buffer(const buffer_builder &buffer);

    // template <typename Functor> void immediately(Functor &&f)
    //{
    //    SingleTimeCommandBuffer cmd_buffer();
    //    f(cmd_buffer.buffer());
    //}

 

  private:
    VkDevice *device_;
    struct VmaAllocator_T *vma_allocator_;
};

} // namespace vk
} // namespace cory