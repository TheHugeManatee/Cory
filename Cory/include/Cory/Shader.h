#pragma once

#include <vulkan/vulkan.hpp>

#include <vector>
#include <cstdint>

namespace Cory {

    class graphics_context;

class Shader {
  public:
    Shader(graphics_context& ctx, const std::vector<char> &code);

    auto module() { return m_module.get(); }

  private:
      vk::UniqueShaderModule m_module;
};
}