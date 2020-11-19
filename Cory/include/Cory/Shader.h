#pragma once

#include <vulkan/vulkan.hpp>

#include <vector>
#include <cstdint>

namespace Cory {

    class GraphicsContext;

class Shader {
  public:
    Shader(GraphicsContext& ctx, const std::vector<char> &code);

    auto module() { return m_module.get(); }

  private:
      vk::UniqueShaderModule m_module;
};
}