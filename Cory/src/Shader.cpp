#include "Shader.h"

#include "Context.h"

namespace Cory {


Shader::Shader(graphics_context &ctx, const std::vector<char> &code)
{
    vk::ShaderModuleCreateInfo createInfo{};
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

    m_module = ctx.device->createShaderModuleUnique(createInfo);
}

} // namespace Cory
