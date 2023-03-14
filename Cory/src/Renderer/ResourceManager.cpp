#include <Cory/Renderer/ResourceManager.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/Shader.hpp>

#include <Magnum/Vk/BufferCreateInfo.h>
#include <Magnum/Vk/DescriptorSetLayoutCreateInfo.h>
#include <Magnum/Vk/Device.h>
#include <Magnum/Vk/ImageCreateInfo.h>
#include <Magnum/Vk/ImageViewCreateInfo.h>
#include <Magnum/Vk/RasterizationPipelineCreateInfo.h>
#include <Magnum/Vk/SamplerCreateInfo.h>

namespace Cory {

namespace Vk = Magnum::Vk;

struct ResourceManagerPrivate {
    Context *ctx;
    SlotMap<Vk::Buffer> buffers;
    SlotMap<Shader> shaders;
    SlotMap<Vk::Pipeline> pipelines;
    SlotMap<Vk::Image> images;
    SlotMap<Vk::ImageView> imageViews;
    SlotMap<Vk::Sampler> samplers;
    SlotMap<Vk::DescriptorSetLayout> descriptorSetLayouts;
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

std::unordered_map<ResourceType, size_t> ResourceManager::resourcesInUse() const
{
    return {
        {ResourceType::Buffer, data_->buffers.size()},
        {ResourceType::Shader, data_->shaders.size()},
        {ResourceType::Pipeline, data_->pipelines.size()},
        {ResourceType::Sampler, data_->samplers.size()},
    };
}

// SHADERS
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

// BUFFERS
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

// PIPELINES
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

// IMAGES
ImageHandle ResourceManager::createImage(std::string_view name,
                                         const Vk::ImageCreateInfo &createInfo,
                                         Magnum::Vk::MemoryFlags memoryFlags)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    auto handle =
        data_->images.emplace(std::ref(data_->ctx->device()), std::ref(createInfo), memoryFlags);

    nameVulkanObject(data_->ctx->device(), data_->images[handle], name);

    return handle;
}
ImageHandle ResourceManager::wrapImage(std::string_view name, Magnum::Vk::Image &resource)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    auto handle =
        data_->images.emplace(Vk::Image::wrap(data_->ctx->device(), resource, resource.format()));

    nameVulkanObject(data_->ctx->device(), data_->images[handle], name);

    return handle;
}
Magnum::Vk::Image &ResourceManager::operator[](ImageHandle handle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->images[handle];
}
void ResourceManager::release(ImageHandle handle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    data_->images.release(handle);
}

// IMAGE VIEWS
ImageViewHandle ResourceManager::createImageView(std::string_view name,
                                                 const Vk::ImageViewCreateInfo &createInfo)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    auto handle = data_->imageViews.emplace(std::ref(data_->ctx->device()), std::ref(createInfo));

    nameVulkanObject(data_->ctx->device(), data_->imageViews[handle], name);

    return handle;
}
ImageViewHandle ResourceManager::wrapImageView(std::string_view name,
                                               Magnum::Vk::ImageView &resource)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    auto handle = data_->imageViews.emplace(Vk::ImageView::wrap(data_->ctx->device(), resource));

    nameVulkanObject(data_->ctx->device(), data_->imageViews[handle], name);

    return handle;
}
Magnum::Vk::ImageView &ResourceManager::operator[](ImageViewHandle handle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->imageViews[handle];
}
void ResourceManager::release(ImageViewHandle handle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    data_->imageViews.release(handle);
}

// SAMPLERS
SamplerHandle ResourceManager::createSampler(std::string_view name,
                                             const Vk::SamplerCreateInfo &createInfo)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    auto handle = data_->samplers.emplace(std::ref(data_->ctx->device()), std::ref(createInfo));

    nameVulkanObject(data_->ctx->device(), data_->samplers[handle], name);

    return handle;
}
Magnum::Vk::Sampler &ResourceManager::operator[](SamplerHandle handle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->samplers[handle];
}
void ResourceManager::release(SamplerHandle handle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    data_->samplers.release(handle);
}

// DESCRIPTOR SET LAYOUTS
DescriptorSetLayoutHandle
ResourceManager::createDescriptorLayout(std::string_view name,
                                        const Vk::DescriptorSetLayoutCreateInfo &createInfo)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    auto handle =
        data_->descriptorSetLayouts.emplace(std::ref(data_->ctx->device()), std::ref(createInfo));

    nameVulkanObject(data_->ctx->device(), data_->descriptorSetLayouts[handle], name);

    return handle;
}
Magnum::Vk::DescriptorSetLayout &ResourceManager::operator[](DescriptorSetLayoutHandle handle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->descriptorSetLayouts[handle];
}
void ResourceManager::release(DescriptorSetLayoutHandle handle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    data_->descriptorSetLayouts.release(handle);
}
} // namespace Cory
