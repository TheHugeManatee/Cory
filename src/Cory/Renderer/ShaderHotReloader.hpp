#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Coro/Coro.hpp>
#include <Cory/Renderer/Common.hpp>

#include <cstddef>
#include <deque>
#include <filesystem>
#include <string>

namespace Cory {

class Context;

/// Utility that compiles shaders, watches files on disk, and hot-reloads shaders in-place.
///
/// Typical usage:
/// 1. `initialize(ctx)` once.
/// 2. Register one or more shaders with `addShader(...)`.
/// 3. Call `processPendingReloads(frameNumber)` each frame on the render thread.
/// 4. Let destruction (or `shutdown()`) release watches and managed shader handles.
class ShaderHotReloader : NoCopy {
  public:
    /// Registration payload for one reloadable shader.
    /// `shaderHandle` must point to storage owned by the caller (e.g. a system member).
    struct ShaderRegistration {
        std::filesystem::path path;
        Gpu::ShaderStageFlagBits stage{SHADER_TYPE_UNKNOWN};
        std::string label;
        ShaderHandle *shaderHandle{nullptr};
    };

    ShaderHotReloader() = default;
    ~ShaderHotReloader();

    ShaderHotReloader(ShaderHotReloader &&) = default;
    ShaderHotReloader &operator=(ShaderHotReloader &&) = default;

    /// Attach to a context and reset previous state.
    void initialize(Context &ctx);

    /// Add one shader to hot-reload management.
    ///
    /// The shader is compiled immediately and a file watch is started right away.
    /// Returns the freshly created shader handle.
    ShaderHandle addShader(ShaderRegistration registration);

    /// Stop watches and release all managed shader handles.
    void shutdown();

    /// Process pending file modifications and recompile changed shaders.
    /// Call this from the render thread once per frame.
    void processPendingReloads(uint64_t currentFrameNumber);

  private:
    struct ReloadableShader {
        std::filesystem::path path;
        Gpu::ShaderStageFlagBits stage{SHADER_TYPE_UNKNOWN};
        std::string label;
        ShaderHandle *shaderHandle{nullptr};
        FileWatchHandle watchHandle{};
        EagerJob watchTask{};
        bool reloadPending{false};
    };

    EagerJob watchShaderFile(size_t shaderIndex);
    bool reloadShaderFromDisk(ReloadableShader &shader, uint64_t currentFrameNumber);

    Context *ctx_{nullptr};
    std::deque<ReloadableShader> shaders_;
};

} // namespace Cory
