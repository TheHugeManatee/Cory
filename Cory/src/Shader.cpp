#include "Shader.h"

#include "Context.h"

namespace Cory {


Shader::Shader(GraphicsContext &ctx, const std::vector<char> &code, ShaderType type)
    : m_type{type}
{
    vk::ShaderModuleCreateInfo createInfo{};
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

    m_module = ctx.device->createShaderModuleUnique(createInfo);
}

vk::PipelineShaderStageCreateInfo Shader::stageCreateInfo()
{
    vk::PipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.stage = vk::ShaderStageFlagBits(m_type);
    shaderStageInfo.module = *m_module;
    // entry point -- means we can add multiple entry points in one module
    shaderStageInfo.pName = "main";

    return shaderStageInfo;
}

} // namespace Cory
