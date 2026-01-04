#include <Cory/Framegraph/FramegraphResourceManager.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/VulkanUtils.hpp>

#include <KDGpu/buffer_options.h>
#include <KDGpu/texture_options.h>
#include <KDGpu/utils/formatters.h>
#include <KDGpu/vulkan/vulkan_resource_manager.h>

#include <gsl/narrow>

namespace Cory {

struct TextureResource {
    TextureInfo info;
    TextureState state;
    Gpu::TextureHandle image;
    Gpu::TextureViewHandle view;
};

struct BufferResource {
    BufferInfo info;
    BufferState state;
    Gpu::BufferHandle buffer;
};

struct FramegraphResourceManagerPrivate {
    Context *ctx_{};
    SlotMap<TextureResource> textureResources_;
    SlotMap<BufferResource> bufferResources_;
};

FramegraphResourceManager::FramegraphResourceManager(Context &ctx)
    : data_{std::make_unique<FramegraphResourceManagerPrivate>()}
{
    data_->ctx_ = &ctx;
}

FramegraphResourceManager::~FramegraphResourceManager() = default;
FramegraphResourceManager::FramegraphResourceManager(FramegraphResourceManager &&) noexcept =
    default;
FramegraphResourceManager &
FramegraphResourceManager::operator=(FramegraphResourceManager &&) noexcept = default;

FramegraphTextureHandle FramegraphResourceManager::declareTexture(TextureInfo info)
{
    CO_CORE_DEBUG("Declaring '{}' of {} ({}, {} samples)",
                  info.name,
                  info.size,
                  info.format,
                  info.sampleCount);

    auto handle = data_->textureResources_.emplace(TextureResource{
        info,
        TextureState{.lastAccess = Sync::AccessType::None, .status = TextureMemoryStatus::Virtual},
        Gpu::Texture{},
        Gpu::TextureView{}});
    return handle;
}

FramegraphTextureHandle
FramegraphResourceManager::registerExternal(TextureInfo info,
                                            Sync::AccessType lastWriteAccess,
                                            Gpu::TextureHandle resource,
                                            Gpu::TextureViewHandle resourceView)
{
    auto handle = data_->textureResources_.emplace(
        TextureResource{.info = info,
                        .state = TextureState{.lastAccess = lastWriteAccess,
                                              .status = TextureMemoryStatus::External},
                        .image = resource,
                        .view = resourceView});

    return handle;
}

void FramegraphResourceManager::allocate(FramegraphTextureHandle handle)
{
    TextureResource &res = data_->textureResources_[handle];
    Gpu::DeviceHandle deviceHandle = data_->ctx_->device();
    auto &resources = data_->ctx_->resources();
    CO_CORE_DEBUG("Allocating '{}' of {} ({})", res.info.name, res.info.size, res.info.format);

    auto extent = Gpu::Extent3D{
        .width = gsl::narrow<uint32_t>(res.info.size.x),
        .height = gsl::narrow<uint32_t>(res.info.size.y),
        .depth = gsl::narrow<uint32_t>(res.info.size.z),
    };

    // TODO: we should eventually infer the usage from the graph, not hardcode it here
    Gpu::TextureUsageFlags usage = res.info.usage;
    usage |= isDepthFormat(res.info.format) ? Gpu::TextureUsageFlagBits::DepthStencilAttachmentBit
                                            : Gpu::TextureUsageFlagBits::ColorAttachmentBit;
    usage |= Gpu::TextureUsageFlagBits::SampledBit;
    usage |= Gpu::TextureUsageFlagBits::InputAttachmentBit;

    // Create the texture (image)
    res.image = resources.createTexture(
        deviceHandle,
        Gpu::TextureOptions{.label = fmt::format("{} (IMG)", res.info.name),
                            .type = Gpu::TextureType::TextureType2D,
                            .format = res.info.format,
                            .extent = extent,
                            .mipLevels = 1,
                            .arrayLayers = 1,
                            .samples = res.info.sampleCount,
                            .usage = usage,
                            .memoryUsage = Gpu::MemoryUsage::GpuOnly,
                            .sharingMode = Gpu::SharingMode::Exclusive,
                            .queueTypeIndices = {},
                            .initialLayout = Gpu::TextureLayout::Undefined,
                            .externalMemoryHandleType =
                                KDGpu::ExternalMemoryHandleTypeFlagBits::None,
                            .drmFormatModifiers = {},
                            .createFlags = {}});

    // Create the view
    res.view = resources.createTextureView(
        deviceHandle,
        res.image,
        Gpu::TextureViewOptions{.label = fmt::format("{} (VIEW)", res.info.name),
                                .viewType = Gpu::ViewType::ViewType2D,
                                .format = res.info.format,
                                .range = {},
                                .yCbCrConversion = {}});

    res.state.status = TextureMemoryStatus::Allocated;
}

void FramegraphResourceManager::allocate(const std::vector<FramegraphTextureHandle> &handles)
{
    for (const auto &handle : handles) {
        auto &res = data_->textureResources_[handle];
        // don't allocate external resources or resources that are already allocated
        if (res.state.status != TextureMemoryStatus::Virtual) {
            continue;
        }

        allocate(handle);
    }
}

Sync::ImageBarrier FramegraphResourceManager::synchronizeTexture(FramegraphTextureHandle handle,
                                                                 Sync::AccessType access,
                                                                 ImageContents contentsMode)
{
    const auto &info = data_->textureResources_[handle].info;
    auto aspectMask = flagsForFormat(info.format);
    auto &state = data_->textureResources_[handle].state;

    auto *texture = data_->ctx_->resources().getTexture(image(handle));
    CO_CORE_DEBUG_ASSERT(texture != nullptr, "Texture resource is null");
    VkImage vkImageHandle = texture->image;
    const VkBool32 discard = (contentsMode == ImageContents::Discard) ? VK_TRUE : VK_FALSE;
    Sync::ImageBarrier barrier{.prevAccesses{state.lastAccess},
                               .nextAccesses{access},
                               .prevLayout = Sync::ImageLayout::Optimal,
                               .nextLayout = Sync::ImageLayout::Optimal,
                               .discardContents = discard,
                               // todo: probably problematic once we actually use more queues
                               .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                               .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                               .image = vkImageHandle,
                               .subresourceRange = {
                                   .aspectMask = aspectMask.toInt(),
                                   .baseMipLevel = 0,
                                   .levelCount = 1,
                                   .baseArrayLayer = 0,
                                   .layerCount = 1,
                               }};

    CO_CORE_TRACE("BARRIER '{}' written as {} ({}), read as {}",
                  info.name,
                  state.lastAccess,
                  contentsMode,
                  access);

    state.lastAccess = access;
    return barrier;
}

const TextureInfo &FramegraphResourceManager::info(FramegraphTextureHandle handle) const
{
    return data_->textureResources_[handle].info;
}

Gpu::TextureHandle FramegraphResourceManager::image(FramegraphTextureHandle handle) const
{
    return data_->textureResources_[handle].image;
}

Gpu::TextureViewHandle FramegraphResourceManager::imageView(FramegraphTextureHandle handle) const
{
    return data_->textureResources_[handle].view;
}

TextureState FramegraphResourceManager::state(FramegraphTextureHandle handle) const
{
    return data_->textureResources_[handle].state;
}

FramegraphBufferHandle FramegraphResourceManager::declareBuffer(BufferInfo info)
{
    CO_CORE_TRACE("Declaring buffer '{}' ({} bytes)", info.name, info.size);

    auto handle = data_->bufferResources_.emplace(
        BufferResource{.info = std::move(info),
                       .state = BufferState{.lastAccess = Sync::AccessType::None,
                                            .status = BufferMemoryStatus::Virtual},
                       .buffer = Gpu::Buffer{}});
    return handle;
}

FramegraphBufferHandle FramegraphResourceManager::registerExternal(BufferInfo info,
                                                                   Sync::AccessType lastWriteAccess,
                                                                   Gpu::BufferHandle resource)
{
    auto handle = data_->bufferResources_.emplace(BufferResource{
        .info = std::move(info),
        .state = BufferState{.lastAccess = lastWriteAccess, .status = BufferMemoryStatus::External},
        .buffer = resource});
    return handle;
}

void FramegraphResourceManager::allocate(FramegraphBufferHandle handle)
{
    BufferResource &res = data_->bufferResources_[handle];
    Gpu::DeviceHandle deviceHandle = data_->ctx_->device();
    auto &resources = data_->ctx_->resources();
    CO_CORE_TRACE("Allocating buffer '{}' ({} bytes)", res.info.name, res.info.size);

    res.buffer =
        resources.createBuffer(deviceHandle,
                               Gpu::BufferOptions{.label = fmt::format("{} (BUF)", res.info.name),
                                                  .size = res.info.size,
                                                  .usage = res.info.usage,
                                                  .memoryUsage = res.info.memoryUsage},
                               nullptr);

    res.state.status = BufferMemoryStatus::Allocated;
}

void FramegraphResourceManager::allocate(const std::vector<FramegraphBufferHandle> &handles)
{
    for (const auto &handle : handles) {
        auto &res = data_->bufferResources_[handle];
        if (res.state.status != BufferMemoryStatus::Virtual) {
            continue;
        }

        allocate(handle);
    }
}

Sync::BufferBarrier FramegraphResourceManager::synchronizeBuffer(FramegraphBufferHandle handle,
                                                                 Sync::AccessType access)
{
    auto &state = data_->bufferResources_[handle].state;
    auto *bufferResource = data_->ctx_->resources().getBuffer(buffer(handle));
    CO_CORE_DEBUG_ASSERT(bufferResource != nullptr, "Buffer resource is null");

    Sync::BufferBarrier barrier{.prevAccesses{state.lastAccess},
                                .nextAccesses{access},
                                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .buffer = bufferResource->buffer,
                                .offset = 0,
                                .size = data_->bufferResources_[handle].info.size};

    CO_CORE_TRACE("BARRIER buffer '{}' written as {}, read as {}",
                  data_->bufferResources_[handle].info.name,
                  state.lastAccess,
                  access);

    state.lastAccess = access;
    return barrier;
}

const BufferInfo &FramegraphResourceManager::info(FramegraphBufferHandle handle) const
{
    return data_->bufferResources_[handle].info;
}

Gpu::BufferHandle FramegraphResourceManager::buffer(FramegraphBufferHandle handle) const
{
    return data_->bufferResources_[handle].buffer;
}
std::pair<Gpu::BufferHandle, Gpu::VulkanBuffer *>
FramegraphResourceManager::bufferResource(FramegraphBufferHandle handle) const
{
    auto resourceHandle = data_->bufferResources_[handle].buffer;
    auto resource = data_->ctx_->resources().getBuffer(resourceHandle);
    CO_CORE_DEBUG_ASSERT(resource != nullptr, "Buffer resource is null");
    return {resourceHandle, resource};
}

BufferState FramegraphResourceManager::state(FramegraphBufferHandle handle) const
{
    return data_->bufferResources_[handle].state;
}

void FramegraphResourceManager::clear()
{
    for (auto &res : data_->textureResources_) {
        if (res.state.status == TextureMemoryStatus::Allocated) {
            data_->ctx_->resources().deleteTexture(res.image);
            data_->ctx_->resources().deleteTextureView(res.view);
        }
    }
    data_->textureResources_.clear();

    for (auto &res : data_->bufferResources_) {
        if (res.state.status == BufferMemoryStatus::Allocated) {
            data_->ctx_->resources().deleteBuffer(res.buffer);
        }
    }
    data_->bufferResources_.clear();
}

} // namespace Cory
