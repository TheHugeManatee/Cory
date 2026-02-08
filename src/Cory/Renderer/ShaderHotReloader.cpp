#include <Cory/Renderer/ShaderHotReloader.hpp>

#include <Cory/Base/FileWatchManager.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include <utility>

namespace Cory {

ShaderHotReloader::~ShaderHotReloader()
{
    shutdown();
}

void ShaderHotReloader::initialize(Context &ctx)
{
    shutdown();
    ctx_ = &ctx;
}

ShaderHandle ShaderHotReloader::addShader(ShaderRegistration registration)
{
    CO_CORE_ASSERT(ctx_ != nullptr,
                   "ShaderHotReloader::addShader requires initialize(ctx) to be called first.");
    CO_CORE_ASSERT(registration.shaderHandle != nullptr,
                   "ShaderHotReloader::addShader requires a non-null shaderHandle target.");

    auto &shader = shaders_.emplace_back(ReloadableShader{
        .path = std::move(registration.path),
        .stage = registration.stage,
        .label = std::move(registration.label),
        .shaderHandle = registration.shaderHandle,
    });

    *shader.shaderHandle = ctx_->shaders().createShader(shader.path, shader.stage);
    const auto shaderIndex = shaders_.size() - 1;
    shader.watchTask = watchShaderFile(shaderIndex);
    return *shader.shaderHandle;
}

void ShaderHotReloader::shutdown()
{
    if (!ctx_) {
        shaders_.clear();
        return;
    }

    auto &fileWatchManager = ctx_->fileWatchManager();
    auto &shaderManager = ctx_->shaders();
    for (auto &shader : shaders_) {
        fileWatchManager.unwatch(shader.watchHandle);
        if (shader.shaderHandle && *shader.shaderHandle) {
            shaderManager.release(*shader.shaderHandle);
        }
    }

    shaders_.clear();
    ctx_ = nullptr;
}

void ShaderHotReloader::processPendingReloads(uint64_t currentFrameNumber)
{
    if (!ctx_) {
        return;
    }

    for (auto &shader : shaders_) {
        if (!shader.reloadPending) {
            continue;
        }
        shader.reloadPending = false;
        reloadShaderFromDisk(shader, currentFrameNumber);
    }
}

EagerJob ShaderHotReloader::watchShaderFile(size_t shaderIndex)
{
    auto &shader = shaders_[shaderIndex];
    auto &fileWatchManager = ctx_->fileWatchManager();

    auto watchHandle = fileWatchManager.watch(FileWatch{.path = shader.path.string()});
    if (!watchHandle) {
        CO_CORE_ERROR("ShaderHotReloader: failed to watch shader '{}'", shader.path.string());
        co_return;
    }

    shader.watchHandle = watchHandle;
    for (auto event = FileWatchEventType::Unknown; event != FileWatchEventType::WatchEnded;
         event = co_await fileWatchManager.nextEvent(watchHandle)) {
        if (event == FileWatchEventType::Modified) {
            CO_CORE_INFO("ShaderHotReloader: shader '{}' modified on disk", shader.label);
            shader.reloadPending = true;
        }
    }
}

bool ShaderHotReloader::reloadShaderFromDisk(ReloadableShader &shader, uint64_t currentFrameNumber)
{
    auto replacement = ctx_->shaders().createShader(shader.path, shader.stage);
    const auto &compiledShader = ctx_->shaders()[replacement];
    if (!compiledShader.valid()) {
        CO_CORE_ERROR("ShaderHotReloader: failed to reload shader '{}': {}",
                      shader.label,
                      compiledShader.error());
        ctx_->shaders().release(replacement);
        return false;
    }

    ctx_->shaders().release(*shader.shaderHandle, currentFrameNumber);
    *shader.shaderHandle = replacement;
    CO_CORE_INFO("ShaderHotReloader: reloaded shader '{}'", shader.label);
    return true;
}

} // namespace Cory
