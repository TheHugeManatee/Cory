#pragma once

#include <Cory/Base/SlotMap.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Base/ValOptional.hpp>
#include <Cory/Renderer/Shader.hpp>

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
    /// Create a shader from a fully configured ShaderSource.
    [[nodiscard]] ShaderHandle
    createShader(ShaderSource source,
                 std::source_location loc = std::source_location::current());
    /// @see ShaderSource::ShaderSource(std::string, ShaderType, std::filesystem::path)
    [[nodiscard]] ShaderHandle
    createShader(std::string source,
                 Gpu::ShaderStageFlagBits type,
                 std::filesystem::path filePath = "Unknown",
                 std::source_location loc = std::source_location::current());
    /// dereference a shader handle to access the shader. may throw!
    [[nodiscard]] Shader &operator[](ShaderHandle shaderHandle);

    /// @brief Release a shader
    /// @param shaderHandle   the shader to release
    /// @param lastUsedFrame optional frame number when the shader was last used
    ///
    /// If @a lastUsedFrame is not provided, the shader will be released immediately. Otherwise it
    /// will be released when clearDeferredReleases has been called with @a lastUsedFrame +
    /// MAX_FRAMES_IN_FLIGHT
    void release(ShaderHandle shaderHandle, ValOptional<uint64_t> lastUsedFrame = {});

    /// Release any shaders that are no longer in use
    void clearDeferredReleases(uint64_t currentFrame = UINT64_MAX);

  private:
    std::unique_ptr<struct ResourceManagerPrivate> data_;
};

} // namespace Cory
