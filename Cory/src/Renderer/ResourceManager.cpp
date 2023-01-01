#include <Cory/Renderer/ResourceManager.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/Shader.hpp>

#include <Magnum/Vk/BufferCreateInfo.h>
#include <Magnum/Vk/Device.h>
#include <Magnum/Vk/RasterizationPipelineCreateInfo.h>

namespace Cory {

namespace Vk = Magnum::Vk;

struct ResourceManagerPrivate {
    Context *ctx;
    SlotMap<Shader> shaders;
    SlotMap<Vk::Buffer> buffers;
    SlotMap<Vk::Pipeline> pipelines;
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
    return data_->shaders[shaderHandle];
}

void ResourceManager::release(ShaderHandle shaderHandle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    data_->shaders.release(shaderHandle);
}

BufferHandle
ResourceManager::createBuffer(size_t bufferSizeInBytes, BufferUsage usage, MemoryFlags flags)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->buffers.emplace(
        std::ref(data_->ctx->device()),
        Vk::BufferCreateInfo{Vk::BufferUsage{usage.underlying_bits()}, bufferSizeInBytes},
        Vk::MemoryFlag{flags.underlying_bits()});
}

Vk::Buffer &ResourceManager::operator[](BufferHandle bufferHandle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->buffers[bufferHandle];
}

void ResourceManager::release(BufferHandle bufferHandle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    data_->buffers.release(bufferHandle.handle_);
}

PipelineHandle
ResourceManager::createPipeline(std::string_view name,
                                const Vk::RasterizationPipelineCreateInfo &createInfo)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    auto handle = data_->pipelines.emplace(std::ref(data_->ctx->device()), std::ref(createInfo));

    nameVulkanObject(data_->ctx->device(), data_->pipelines[handle], name);

    return handle;
}

Vk::Pipeline &ResourceManager::operator[](PipelineHandle pipelineHandle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->pipelines[pipelineHandle];
}

void ResourceManager::release(PipelineHandle pipelineHandle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    data_->pipelines.release(pipelineHandle);
}

std::unordered_map<ResourceType, size_t> ResourceManager::resourcesInUse() const
{
    return {{ResourceType::Buffer, data_->buffers.size()},
            {ResourceType::Shader, data_->shaders.size()}};
}
} // namespace Cory
