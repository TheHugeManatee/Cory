#include <Cory/Renderer/ShaderManager.hpp>

#include "FrameResourceLifetimeHelper.hpp"

#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/Shader.hpp>

#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/view/take.hpp>

namespace Cory {

template <typename T> struct ResourceStorage {
    const std::string name;
    const std::source_location loc;
    T resource;
};

struct ResourceManagerPrivate {
    Context *ctx;
    SlotMap<ResourceStorage<Shader>> shaders;
    FrameResourceLifetimeHelper<ShaderHandle> deferredReleaseTracker;
};

ShaderManager::ShaderManager()
    : data_{std::make_unique<ResourceManagerPrivate>()}
{
}

ShaderManager::~ShaderManager()
{
    if (!data_->shaders.empty()) {
        CO_CORE_WARN("ResourceManager: There are still {} Shaders in use!", data_->shaders.size());

        size_t counter{0};
        for (const auto &storage : data_->shaders) {
            CO_CORE_WARN("  - {}, allocated here: \n    {}:{}",
                         storage.name,
                         storage.loc.file_name(),
                         storage.loc.line());
            if (++counter == 10) {
                CO_CORE_WARN("  - {} more...", data_->shaders.size() - counter);
                break;
            }
        }
    }
}

void ShaderManager::setContext(Context &ctx)
{
    CO_CORE_DEBUG_ASSERT(data_->ctx == nullptr, "Context already initialized!");
    data_->ctx = &ctx;
}

size_t ShaderManager::shadersInUse() const
{
    return data_->shaders.size();
}

ShaderHandle ShaderManager::createShader(std::filesystem::path filePath,
                                         Gpu::ShaderStageFlagBits type,
                                         std::source_location loc)
{
    CO_CORE_DEBUG_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->shaders.emplace(ResourceStorage<Shader>{
        .name = filePath.string(),
        .loc = std::move(loc),
        .resource = {std::ref(*data_->ctx), ShaderSource{std::move(filePath), type}, "main", {}}});
}

ShaderHandle ShaderManager::createShader(ShaderSource source,
                                         std::vector<Gpu::PushConstantRange> pushConstantRanges,
                                         std::source_location loc)
{
    CO_CORE_DEBUG_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->shaders.emplace(ResourceStorage<Shader>{
        .name = source.filePath().string(),
        .loc = std::move(loc),
        .resource = {std::ref(*data_->ctx), std::move(source), "main", std::move(pushConstantRanges)}});
}

ShaderHandle ShaderManager::createShader(std::string source,
                                         Gpu::ShaderStageFlagBits type,
                                         std::filesystem::path filePath,
                                         std::source_location loc)
{
    CO_CORE_DEBUG_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->shaders.emplace(ResourceStorage<Shader>{
        .name = filePath.string(),
        .loc = std::move(loc),
        .resource = {std::ref(*data_->ctx),
                     ShaderSource{std::move(source), type, std::move(filePath)},
                     "main",
                     {}}});
}

Shader &ShaderManager::operator[](ShaderHandle shaderHandle)
{
    CO_CORE_DEBUG_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->shaders[shaderHandle].resource;
}

void ShaderManager::release(ShaderHandle shaderHandle, ValOptional<uint64_t> lastUsedFrame)
{
    CO_CORE_DEBUG_ASSERT(data_->ctx != nullptr, "Context was not initialized!");

    if (lastUsedFrame) {
        data_->deferredReleaseTracker.scheduleForRelease(shaderHandle, *lastUsedFrame);
        return;
    }
    // Otherwise release directly
    data_->shaders.release(shaderHandle);
}

void ShaderManager::clearDeferredReleases(uint64_t currentFrame)
{
    for (auto shaderHandle :
         data_->deferredReleaseTracker.collectReleasableResources(currentFrame)) {
        const auto &shader = data_->shaders[shaderHandle];
        CO_CORE_DEBUG("Frame {}: Deferred release of shader {} allocated at {}:{}",
                      currentFrame,
                      shader.name,
                      shader.loc.file_name(),
                      shader.loc.line());

        data_->shaders.release(shaderHandle);
    }
}

} // namespace Cory
