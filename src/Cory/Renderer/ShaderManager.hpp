#pragma once

#include <Cory/Base/SlotMap.hpp>
#include <Cory/Renderer/Common.hpp>

#include <filesystem>
#include <memory>
#include <source_location>

namespace Cory {

/**
 * Central shader manager.
 */
class ShaderManager : NoCopy {
  public:
    ShaderManager();
    ~ShaderManager();

    ShaderManager(ShaderManager &&) = default;
    ShaderManager &operator=(ShaderManager &&) = default;

    // set up the context to be used - must be called exactly once, before any resources are created
    void setContext(Context &ctx);

    /// query the number of resources in use
    size_t shadersInUse() const;

    /// @see ShaderSource::ShaderSource(std::filesystem::path, ShaderType)
    [[nodiscard]] ShaderHandle
    createShader(std::filesystem::path filePath,
                 Gpu::ShaderStageFlagBits type = SHADER_TYPE_UNKNOWN,
                 std::source_location loc = std::source_location::current());
    /// @see ShaderSource::ShaderSource(std::string, ShaderType, std::filesystem::path)
    [[nodiscard]] ShaderHandle
    createShader(std::string source,
                 Gpu::ShaderStageFlagBits type,
                 std::filesystem::path filePath = "Unknown",
                 std::source_location loc = std::source_location::current());
    /// dereference a shader handle to access the shader. may throw!
    [[nodiscard]] Shader &operator[](ShaderHandle shaderHandle);
    void release(ShaderHandle shaderHandle);

  private:
    std::unique_ptr<struct ResourceManagerPrivate> data_;
};

} // namespace Cory
