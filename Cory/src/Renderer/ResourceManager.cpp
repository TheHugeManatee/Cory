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

#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/view/take.hpp>

namespace Cory {

namespace Vk = Magnum::Vk;

template <typename T> struct ResourceStorage {
    const std::string name;
    const std::source_location loc;
    T resource;
};

struct ResourceManagerPrivate {
    Context *ctx;
    SlotMap<ResourceStorage<Vk::Buffer>> buffers;
    SlotMap<ResourceStorage<Shader>> shaders;
    SlotMap<ResourceStorage<Vk::Pipeline>> pipelines;
    SlotMap<ResourceStorage<Vk::Image>> images;
    SlotMap<ResourceStorage<Vk::ImageView>> imageViews;
    SlotMap<ResourceStorage<Vk::Sampler>> samplers;
    SlotMap<ResourceStorage<Vk::DescriptorSetLayout>> descriptorSetLayouts;
};

ResourceManager::ResourceManager()
    : data_{std::make_unique<ResourceManagerPrivate>()}
{
}

ResourceManager::~ResourceManager()
{
    auto check_empty = [](std::string_view name, const auto &slotMap) {
        if (!slotMap.empty()) {
            CO_CORE_WARN(
                "ResourceManager: There are still {} {} elements in use!", slotMap.size(), name);

            size_t counter{0};
            for (const auto &storage : slotMap) {
                CO_CORE_WARN("  - {}, allocated here: \n    {}:{}",
                             storage.name,
                             storage.loc.file_name(),
                             storage.loc.line();
                if (++counter == 10) {
                    CO_CORE_WARN("  - {} more...", slotMap.size() - counter);
                    break;
                }
            }
        }
    };

    check_empty("buffer", data_->buffers);
    check_empty("shader", data_->shaders);
    check_empty("pipeline", data_->pipelines);
    check_empty("image", data_->images);
    check_empty("imageView", data_->imageViews);
    check_empty("sampler", data_->samplers);
    check_empty("descriptorSetLayout", data_->descriptorSetLayouts);
}

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
ShaderHandle ResourceManager::createShader(std::filesystem::path filePath,
                                           ShaderType type,
                                           std::source_location loc)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->shaders.emplace(ResourceStorage<Shader>{
        .name = filePath.string(),
        .loc = std::move(loc),
        .resource = {std::ref(*data_->ctx), ShaderSource{std::move(filePath), type}}});
}
ShaderHandle ResourceManager::createShader(std::string source,
                                           ShaderType type,
                                           std::filesystem::path filePath,
                                           std::source_location loc)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->shaders.emplace(ResourceStorage<Shader>{
        .name = filePath.string(),
        .loc = std::move(loc),
        .resource = {std::ref(*data_->ctx),
                     ShaderSource{std::move(source), type, std::move(filePath)}}});
}
Shader &ResourceManager::operator[](ShaderHandle shaderHandle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->shaders[shaderHandle].resource;
}
void ResourceManager::release(ShaderHandle shaderHandle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    data_->shaders.release(shaderHandle);
}

// BUFFERS
BufferHandle ResourceManager::createBuffer(std::string_view name,
                                           size_t bufferSizeInBytes,
                                           BufferUsage usage,
                                           MemoryFlags flags,
                                           std::source_location loc)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    auto handle = data_->buffers.emplace(ResourceStorage<Vk::Buffer>{
        .name{name},
        .loc = std::move(loc),
        .resource{std::ref(data_->ctx->device()),
                  Vk::BufferCreateInfo{Vk::BufferUsage{usage.underlying_bits()}, bufferSizeInBytes},
                  Vk::MemoryFlag{flags.underlying_bits()}}});

    nameVulkanObject(data_->ctx->device(), data_->buffers[handle].resource, name);

    return handle;
}
Vk::Buffer &ResourceManager::operator[](BufferHandle bufferHandle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->buffers[bufferHandle].resource;
}
void ResourceManager::release(BufferHandle bufferHandle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    data_->buffers.release(bufferHandle.handle_);
}

// PIPELINES
PipelineHandle
ResourceManager::createPipeline(std::string_view name,
                                const Vk::RasterizationPipelineCreateInfo &createInfo,
                                std::source_location loc)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    auto handle = data_->pipelines.emplace(ResourceStorage<Vk::Pipeline>{
        .name{name},
        .loc = std::move(loc),
        .resource{std::ref(data_->ctx->device()), std::ref(createInfo)}});

    nameVulkanObject(data_->ctx->device(), data_->pipelines[handle].resource, name);

    return handle;
}
Vk::Pipeline &ResourceManager::operator[](PipelineHandle pipelineHandle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->pipelines[pipelineHandle].resource;
}
void ResourceManager::release(PipelineHandle pipelineHandle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    data_->pipelines.release(pipelineHandle);
}

// IMAGES
ImageHandle ResourceManager::createImage(std::string_view name,
                                         const Vk::ImageCreateInfo &createInfo,
                                         Magnum::Vk::MemoryFlags memoryFlags,
                                         std::source_location loc)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    auto handle = data_->images.emplace(ResourceStorage<Vk::Image>{
        .name{name},
        .loc = std::move(loc),
        .resource{std::ref(data_->ctx->device()), std::ref(createInfo), memoryFlags}});

    nameVulkanObject(data_->ctx->device(), data_->images[handle].resource, name);

    return handle;
}
ImageHandle ResourceManager::wrapImage(std::string_view name,
                                       Magnum::Vk::Image &resource,
                                       std::source_location loc)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    auto handle = data_->images.emplace(ResourceStorage<Vk::Image>{
        .name{name},
        .loc = std::move(loc),
        .resource{Vk::Image::wrap(data_->ctx->device(), resource, resource.format())}});

    nameVulkanObject(data_->ctx->device(), data_->images[handle].resource, name);

    return handle;
}
Magnum::Vk::Image &ResourceManager::operator[](ImageHandle handle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->images[handle].resource;
}
void ResourceManager::release(ImageHandle handle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    data_->images.release(handle);
}

// IMAGE VIEWS
ImageViewHandle ResourceManager::createImageView(std::string_view name,
                                                 const Vk::ImageViewCreateInfo &createInfo,
                                                 std::source_location loc)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    auto handle = data_->imageViews.emplace(ResourceStorage<Vk::ImageView>{
        .name{name},
        .loc = std::move(loc),
        .resource{std::ref(data_->ctx->device()), std::ref(createInfo)}});

    nameVulkanObject(data_->ctx->device(), data_->imageViews[handle].resource, name);

    return handle;
}
ImageViewHandle ResourceManager::wrapImageView(std::string_view name,
                                               Magnum::Vk::ImageView &resource,
                                               std::source_location loc)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    auto handle = data_->imageViews.emplace(ResourceStorage<Vk::ImageView>{
        .name{name},
        .loc = std::move(loc),
        .resource{Vk::ImageView::wrap(data_->ctx->device(), resource)}});

    nameVulkanObject(data_->ctx->device(), data_->imageViews[handle].resource, name);

    return handle;
}
Magnum::Vk::ImageView &ResourceManager::operator[](ImageViewHandle handle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->imageViews[handle].resource;
}
void ResourceManager::release(ImageViewHandle handle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    data_->imageViews.release(handle);
}

// SAMPLERS
SamplerHandle ResourceManager::createSampler(std::string_view name,
                                             const Vk::SamplerCreateInfo &createInfo,
                                             std::source_location loc)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    auto handle = data_->samplers.emplace(ResourceStorage<Vk::Sampler>{
        .name{name},
        .loc = std::move(loc),
        .resource{std::ref(data_->ctx->device()), std::ref(createInfo)}});

    nameVulkanObject(data_->ctx->device(), data_->samplers[handle].resource, name);

    return handle;
}
Magnum::Vk::Sampler &ResourceManager::operator[](SamplerHandle handle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->samplers[handle].resource;
}
void ResourceManager::release(SamplerHandle handle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    data_->samplers.release(handle);
}

// DESCRIPTOR SET LAYOUTS
DescriptorSetLayoutHandle
ResourceManager::createDescriptorLayout(std::string_view name,
                                        const Vk::DescriptorSetLayoutCreateInfo &createInfo,
                                        std::source_location loc)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    auto handle = data_->descriptorSetLayouts.emplace(ResourceStorage<Vk::DescriptorSetLayout>{
        .name{name},
        .loc = std::move(loc),
        .resource{std::ref(data_->ctx->device()), std::ref(createInfo)}});

    nameVulkanObject(data_->ctx->device(), data_->descriptorSetLayouts[handle].resource, name);

    return handle;
}
Magnum::Vk::DescriptorSetLayout &ResourceManager::operator[](DescriptorSetLayoutHandle handle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->descriptorSetLayouts[handle].resource;
}
void ResourceManager::release(DescriptorSetLayoutHandle handle)
{
    CO_CORE_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    data_->descriptorSetLayouts.release(handle);
}
} // namespace Cory
