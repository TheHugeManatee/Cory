#pragma once

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <vector>

namespace Cory {

class GraphicsContext;

enum class ShaderType {
    eVertex = VK_SHADER_STAGE_VERTEX_BIT,
    eGeometry = VK_SHADER_STAGE_GEOMETRY_BIT,
    eFragment = VK_SHADER_STAGE_FRAGMENT_BIT,
    eCompute = VK_SHADER_STAGE_COMPUTE_BIT
};

class Shader {
  public:
    Shader(GraphicsContext &ctx, const std::vector<char> &code, ShaderType type);

    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;

    Shader(Shader &&rhs)
        : m_module{std::move(rhs.m_module)}
        , m_type{rhs.m_type} {};
    Shader &operator=(Shader && rhs)
    {
        m_module = std::move(rhs.m_module);
        m_type = rhs.m_type;
    };

    auto module() { return m_module.get(); }
    auto type() { return m_type; }

    vk::PipelineShaderStageCreateInfo stageCreateInfo();

  private:
    vk::UniqueShaderModule m_module;
    ShaderType m_type;
};
} // namespace Cory