#include <Cory/Renderer/ResourceManager.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/Shader.hpp>

#include <Magnum/Vk/BufferCreateInfo.h>
#include <Magnum/Vk/Device.h>

namespace Cory {

struct ResourceManagerPrivate {
    Context *ctx;
    SlotMap<Shader> shaders;
    SlotMap<Magnum::Vk::Buffer> buffers;
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

void ResourceManager::release(ShaderHandle shaderHandle)
{
    data_->shaders.release(shaderHandle.handle_);
}

BufferHandle
ResourceManager::createBuffer(size_t bufferSizeInBytes, BufferUsage usage, MemoryFlags flags)
{
    namespace Vk = Magnum::Vk;
    return data_->buffers.emplace(
        std::ref(data_->ctx->device()),
        Vk::BufferCreateInfo{Vk::BufferUsage{usage.underlying_bits()}, bufferSizeInBytes},
        Vk::MemoryFlag{flags.underlying_bits()});
}

Magnum::Vk::Buffer &ResourceManager::operator[](BufferHandle bufferHandle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->buffers[bufferHandle.handle_];
}

void ResourceManager::release(BufferHandle bufferHandle)
{
    data_->buffers.release(bufferHandle.handle_);
}

} // namespace Cory