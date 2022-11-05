#pragma once

#include <Cory/Base/SlotMap.hpp>
#include <Cory/RenderCore/Common.hpp>

#include <filesystem>
#include <memory>

namespace Cory {

/**
 * Central resource manager that manages all vulkan-related resources.
 *
 * It it exclusively deals in handles.
 * Handle types are declared in RenderCommon.hpp to reduce compile times
 * Currently:
 *  - Shaders
 * Eventually:
 *  - Textures and Buffers
 *  - Pipelines
 *  - RenderPasses
 *  - Descriptors etc.
 */
class ResourceManager : NoCopy {
  public:
    ResourceManager();
    ~ResourceManager();

    // set up the context to be used - must be called exactly once, before any resources are created
    void setContext(Context &ctx);

    /// @see ShaderSource::ShaderSource(std::filesystem::path, ShaderType)
    [[nodiscard]] ShaderHandle createShader(std::filesystem::path filePath,
                                            ShaderType type = ShaderType::eUnknown);
    /// @see ShaderSource::ShaderSource(std::string, ShaderType, std::filesystem::path)
    [[nodiscard]] ShaderHandle
    createShader(std::string source, ShaderType type, std::filesystem::path filePath = "Unknown");
    /// dereference a shader handle to access the shader. may throw!
    Shader &operator[](ShaderHandle shaderHandle);

  private:
    std::unique_ptr<struct ResourceManagerPrivate> data_;
};

} // namespace Cory
