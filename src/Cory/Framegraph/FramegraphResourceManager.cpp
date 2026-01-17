#include <Cory/Framegraph/FramegraphResourceManager.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/MappedCoherentDeviceBuffer.hpp>
#include <Cory/Renderer/VulkanUtils.hpp>

#include <KDGpu/buffer_options.h>
#include <KDGpu/texture_options.h>
#include <KDGpu/utils/formatters.h>
#include <KDGpu/vulkan/vulkan_resource_manager.h>

#include <algorithm>
#include <gsl/narrow>

namespace Cory {
namespace {
Gpu::DeviceSize alignUp(Gpu::DeviceSize value, Gpu::DeviceSize alignment)
{
    if (alignment == 0) return value;
    return (value + alignment - 1) & ~(alignment - 1);
}

Gpu::DeviceSize bufferAlignment(Context &ctx)
{
    const auto &limits = ctx.physicalDevice().limits;
    const auto minUniform = static_cast<Gpu::DeviceSize>(limits.minUniformBufferOffsetAlignment);
    const auto minStorage = static_cast<Gpu::DeviceSize>(limits.minStorageBufferOffsetAlignment);
    return std::max<Gpu::DeviceSize>(
        {static_cast<Gpu::DeviceSize>(16u), minUniform, minStorage});
}
} // namespace

struct TextureResource {
    TextureInfo info;
    TextureState state;
    Gpu::TextureHandle image;
    Gpu::TextureViewHandle view;
};

struct BufferResource {
    BufferInfo info;
    BufferState state;
    enum class Arena {
        External,
        HostMapped,
        DeviceOnly,
    };
    Arena arena{Arena::DeviceOnly};
    Gpu::BufferHandle externalBuffer;
    Gpu::DeviceSize offset{0};
};

struct FramegraphResourceManagerPrivate {
    Context *ctx_{};
    SlotMap<TextureResource> textureResources_;
    SlotMap<BufferResource> bufferResources_;
    Gpu::BufferHandle deviceBuffer;
    Gpu::DeviceSize deviceBufferSize{0};
    Gpu::BufferUsageFlags deviceBufferUsage{};
    std::unique_ptr<MappedCoherentDeviceBuffer> hostBuffer;
    Gpu::DeviceSize hostBufferSize{0};
    Gpu::BufferUsageFlags hostBufferUsage{};
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

    const auto usage = res.info.usage;

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
                            .externalMemoryHandleType = Gpu::ExternalMemoryHandleTypeFlagBits::None,
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

void FramegraphResourceManager::extendUsage(FramegraphTextureHandle handle,
                                            Gpu::TextureUsageFlags usage)
{
    data_->textureResources_[handle].info.usage |= usage;
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

    const auto arena = info.memoryUsage == Gpu::MemoryUsage::CpuToGpu
                           ? BufferResource::Arena::HostMapped
                           : BufferResource::Arena::DeviceOnly;
    auto handle = data_->bufferResources_.emplace(
        BufferResource{.info = std::move(info),
                       .state = BufferState{.lastAccess = Sync::AccessType::None,
                                            .status = BufferMemoryStatus::Virtual},
                       .arena = arena,
                       .externalBuffer = Gpu::BufferHandle{}});
    return handle;
}

FramegraphBufferHandle FramegraphResourceManager::registerExternal(BufferInfo info,
                                                                   Sync::AccessType lastWriteAccess,
                                                                   Gpu::BufferHandle resource)
{
    auto handle = data_->bufferResources_.emplace(BufferResource{
        .info = std::move(info),
        .state = BufferState{.lastAccess = lastWriteAccess, .status = BufferMemoryStatus::External},
        .arena = BufferResource::Arena::External,
        .externalBuffer = resource});
    return handle;
}

void FramegraphResourceManager::allocate(const std::vector<FramegraphBufferHandle> &handles)
{
    const auto alignment = bufferAlignment(*data_->ctx_);

    Gpu::DeviceSize deviceOffset = 0;
    Gpu::DeviceSize hostOffset = 0;
    data_->deviceBufferUsage = {};
    data_->hostBufferUsage = {};

    for (const auto &handle : handles) {
        auto &res = data_->bufferResources_[handle];
        if (res.state.status != BufferMemoryStatus::Virtual) {
            continue;
        }

        if (res.arena == BufferResource::Arena::HostMapped) {
            hostOffset = alignUp(hostOffset, alignment);
            res.offset = hostOffset;
            hostOffset += res.info.size;
            data_->hostBufferUsage |= res.info.usage;
        }
        else {
            deviceOffset = alignUp(deviceOffset, alignment);
            res.offset = deviceOffset;
            deviceOffset += res.info.size;
            data_->deviceBufferUsage |= res.info.usage;
        }
    }

    data_->deviceBufferSize = deviceOffset;
    data_->hostBufferSize = hostOffset;

    if (data_->deviceBufferSize > 0) {
        data_->deviceBufferUsage |= Gpu::BufferUsageFlagBits::ShaderDeviceAddressBit;
        CO_CORE_TRACE("Allocating framegraph device buffer ({} bytes)", data_->deviceBufferSize);
        data_->deviceBuffer = data_->ctx_->resources().createBuffer(
            data_->ctx_->device(),
            Gpu::BufferOptions{.label = "Framegraph Device Buffer",
                               .size = data_->deviceBufferSize,
                               .usage = data_->deviceBufferUsage,
                               .memoryUsage = Gpu::MemoryUsage::GpuOnly},
            nullptr);
    }

    if (data_->hostBufferSize > 0) {
        CO_CORE_TRACE("Allocating framegraph host buffer ({} bytes)", data_->hostBufferSize);
        data_->hostBuffer = std::make_unique<MappedCoherentDeviceBuffer>(
            data_->ctx_->device(),
            MappedCoherentDeviceBufferCreateInfo{
                .label = "Framegraph Host Buffer",
                .size = data_->hostBufferSize,
                .usage = static_cast<VkBufferUsageFlags>(data_->hostBufferUsage.toInt()),
            });
    }

    for (const auto &handle : handles) {
        auto &res = data_->bufferResources_[handle];
        if (res.state.status == BufferMemoryStatus::Virtual) {
            res.state.status = BufferMemoryStatus::Allocated;
        }
    }
}

void FramegraphResourceManager::extendUsage(FramegraphBufferHandle handle,
                                            Gpu::BufferUsageFlags usage)
{
    data_->bufferResources_[handle].info.usage |= usage;
}

Sync::BufferBarrier FramegraphResourceManager::synchronizeBuffer(FramegraphBufferHandle handle,
                                                                 Sync::AccessType access)
{
    auto &state = data_->bufferResources_[handle].state;
    const auto view = bufferView(handle);
    VkBuffer bufferHandle = VK_NULL_HANDLE;
    const auto &res = data_->bufferResources_[handle];
    if (res.arena == BufferResource::Arena::External) {
        auto *resource = data_->ctx_->resources().getBuffer(res.externalBuffer);
        CO_CORE_DEBUG_ASSERT(resource != nullptr, "Buffer resource is null");
        bufferHandle = resource->buffer;
    }
    else if (res.arena == BufferResource::Arena::HostMapped) {
        CO_CORE_ASSERT(data_->hostBuffer != nullptr, "Host buffer was not allocated");
        bufferHandle = data_->hostBuffer->buffer();
    }
    else {
        CO_CORE_ASSERT(data_->deviceBuffer.isValid(), "Device buffer was not allocated");
        auto *resource = data_->ctx_->resources().getBuffer(data_->deviceBuffer);
        CO_CORE_DEBUG_ASSERT(resource != nullptr, "Device buffer resource is null");
        bufferHandle = resource->buffer;
    }

    Sync::BufferBarrier barrier{.prevAccesses{state.lastAccess},
                                .nextAccesses{access},
                                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .buffer = bufferHandle,
                                .offset = view.offset,
                                .size = view.size};

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

FramegraphBufferView FramegraphResourceManager::bufferView(FramegraphBufferHandle handle) const
{
    const auto &res = data_->bufferResources_[handle];
    if (res.state.status == BufferMemoryStatus::Virtual) {
        CO_CORE_ASSERT(false, "Buffer '{}' is not allocated", res.info.name);
    }

    if (res.arena == BufferResource::Arena::External) {
        auto *resource = data_->ctx_->resources().getBuffer(res.externalBuffer);
        CO_CORE_DEBUG_ASSERT(resource != nullptr, "Buffer resource is null");
        return FramegraphBufferView{.deviceAddress = resource->bufferDeviceAddress(),
                                    .offset = 0,
                                    .size = res.info.size,
                                    .cpu = nullptr,
                                    .hostVisible = false};
    }

    if (res.arena == BufferResource::Arena::HostMapped) {
        CO_CORE_ASSERT(data_->hostBuffer != nullptr, "Host buffer was not allocated");
        const auto baseAllocation = data_->hostBuffer->allocation();
        return FramegraphBufferView{
            .deviceAddress = baseAllocation.gpu + res.offset,
            .offset = res.offset,
            .size = res.info.size,
            .cpu = baseAllocation.cpu + res.offset,
            .hostVisible = true,
        };
    }

    CO_CORE_ASSERT(data_->deviceBuffer.isValid(), "Device buffer was not allocated");
    auto *resource = data_->ctx_->resources().getBuffer(data_->deviceBuffer);
    CO_CORE_DEBUG_ASSERT(resource != nullptr, "Device buffer resource is null");
    return FramegraphBufferView{.deviceAddress = resource->bufferDeviceAddress() + res.offset,
                                .offset = res.offset,
                                .size = res.info.size,
                                .cpu = nullptr,
                                .hostVisible = false};
}
BufferDeviceAddress FramegraphResourceManager::deviceAddress(FramegraphBufferHandle handle) const
{
    const auto view = bufferView(handle);
    CO_CORE_ASSERT(view.deviceAddress != 0, "Queried Buffer device address for buffer is zero");
    return view.deviceAddress;
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

    data_->bufferResources_.clear();

    if (data_->deviceBuffer.isValid()) {
        data_->ctx_->resources().deleteBuffer(data_->deviceBuffer);
        data_->deviceBuffer = {};
    }
    data_->deviceBufferSize = 0;
    data_->deviceBufferUsage = {};

    data_->hostBuffer.reset();
    data_->hostBufferSize = 0;
    data_->hostBufferUsage = {};
}

} // namespace Cory
