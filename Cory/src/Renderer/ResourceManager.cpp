#include <Cory/Renderer/ResourceManager.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/Shader.hpp>

namespace Cory {

struct ResourceManagerPrivate {
    Context *ctx;
    SlotMap<Shader> shaders;
};

ResourceManager::ResourceManager()
    : data_{std::make_unique<ResourceManagerPrivate>()}
{
}

ResourceManager::~ResourceManager() = default;

void ResourceManager::setContext(Context &ctx)
{
    CO_CORE_ASSERT(data_->ctx == nullptr, "Context already initialized!");
    data_->ctx = &ctx;
}

ShaderHandle ResourceManager::createShader(std::filesystem::path filePath, ShaderType type)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->shaders.emplace(std::ref(*data_->ctx), ShaderSource{std::move(filePath), type});
}

ShaderHandle
ResourceManager::createShader(std::string source, ShaderType type, std::filesystem::path filePath)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->shaders.emplace(std::ref(*data_->ctx),
                                  ShaderSource{std::move(source), type, std::move(filePath)});
}

Shader &ResourceManager::operator[](ShaderHandle shaderHandle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->shaders[shaderHandle.handle_];
}

} // namespace Cory