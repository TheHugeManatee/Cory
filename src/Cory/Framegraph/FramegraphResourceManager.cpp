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
#include <cstdint>
#include <gsl/narrow>
#include <span>
#include <vector>

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
    return std::max<Gpu::DeviceSize>({static_cast<Gpu::DeviceSize>(16u), minUniform, minStorage});
}
} // namespace

struct TextureResource {
    TextureInfo info;
    TextureState state;
    Gpu::TextureHandle image;
    Gpu::TextureViewHandle view;
    uint64_t lastUsedFrameNumber{0};
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
    uint64_t lastUsedFrameNumber{0};
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
    uint64_t currentFrameNumber{};
};

FramegraphResourceManager::FramegraphResourceManager(Context &ctx)
    : data_{std::make_unique<FramegraphResourceManagerPrivate>()}
{
    data_->ctx_ = &ctx;
}

FramegraphResourceManager::~FramegraphResourceManager()
{
    clearAll();
}
FramegraphResourceManager::FramegraphResourceManager(FramegraphResourceManager &&) noexcept =
    default;
FramegraphResourceManager &
FramegraphResourceManager::operator=(FramegraphResourceManager &&) noexcept = default;

void FramegraphResourceManager::setCurrentFrameNumber(uint64_t frameNumber)
{
    data_->currentFrameNumber = frameNumber;
}

uint64_t FramegraphResourceManager::currentFrameNumber() const
{
    return data_->currentFrameNumber;
}

FramegraphTextureHandle FramegraphResourceManager::declareTexture(TextureInfo info)
{
    auto handle = data_->textureResources_.emplace(
        TextureResource{.info = info,
                        .state = TextureState{.lastAccess = Sync::AccessType::None,
                                              .status = TextureMemoryStatus::Virtual},
                        .image = Gpu::Texture{},
                        .view = Gpu::TextureView{},
                        .lastUsedFrameNumber = data_->currentFrameNumber});
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
                        .view = resourceView,
                        .lastUsedFrameNumber = data_->currentFrameNumber});

    return handle;
}

void FramegraphResourceManager::allocate(FramegraphTextureHandle handle)
{
    TextureResource &res = data_->textureResources_[handle];
    Gpu::DeviceHandle deviceHandle = data_->ctx_->device();
    auto &resources = data_->ctx_->resources();

    auto extent = Gpu::Extent3D{
        .width = gsl::narrow<uint32_t>(res.info.size.x),
        .height = gsl::narrow<uint32_t>(res.info.size.y),
        .depth = gsl::narrow<uint32_t>(res.info.size.z),
    };
    const auto textureType = res.info.textureType;
    const auto viewType = textureType == Gpu::TextureType::TextureType3D
                              ? Gpu::ViewType::ViewType3D
                              : Gpu::ViewType::ViewType2D;

    const auto usage = res.info.usage;

    // Create the texture (image)
    res.image = resources.createTexture(
        deviceHandle,
        Gpu::TextureOptions{.label = fmt::format("{} (IMG)", res.info.name),
                            .type = textureType,
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
    const auto aspectMask = flagsForFormat(res.info.format);
    res.view = resources.createTextureView(
        deviceHandle,
        res.image,
        Gpu::TextureViewOptions{.label = fmt::format("{} (VIEW)", res.info.name),
                                .viewType = viewType,
                                .format = res.info.format,
                                .range =
                                    {
                                        .aspectMask = aspectMask,
                                        .baseMipLevel = 0,
                                        .levelCount = 1,
                                        .baseArrayLayer = 0,
                                        .layerCount = 1,
                                    },
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
    auto &resource = data_->textureResources_[handle];
    const auto &info = resource.info;
    auto aspectMask = flagsForFormat(info.format);
    auto &state = resource.state;

    auto *texture = data_->ctx_->resources().getTexture(image(handle));
    CO_CORE_DEBUG_ASSERT(texture != nullptr,
                         "Texture resource '{}' is null (Memory Status = {}')",
                         resource.info.name,
                         magic_enum::enum_name(state.status));
    VkImage vkImageHandle = texture->image;
    const VkBool32 discard = (contentsMode == ImageContents::Discard) ? VK_TRUE : VK_FALSE;
    Sync::ImageBarrier barrier{.prevAccesses{state.lastAccess},
                               .nextAccesses{access},
                               .prevLayout = Sync::ImageLayout::Optimal,
                               .nextLayout = Sync::ImageLayout::Optimal,
                               .discardContents = discard,
                               .srcQueueFamilyIndex = data_->ctx_->graphicsQueueFamilyIndex(),
                               .dstQueueFamilyIndex = data_->ctx_->graphicsQueueFamilyIndex(),
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
    resource.lastUsedFrameNumber = data_->currentFrameNumber;
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
                       .externalBuffer = Gpu::BufferHandle{},
                       .lastUsedFrameNumber = data_->currentFrameNumber});
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
        .externalBuffer = resource,
        .lastUsedFrameNumber = data_->currentFrameNumber});
    return handle;
}

void FramegraphResourceManager::allocate(const std::vector<FramegraphBufferHandle> &handles)
{
    const auto alignment = bufferAlignment(*data_->ctx_);

    Gpu::DeviceSize deviceOffset = 0;
    Gpu::DeviceSize hostOffset = 0;
    const auto previousDeviceBufferSize = data_->deviceBufferSize;
    const auto previousHostBufferSize = data_->hostBufferSize;
    const auto previousDeviceBufferUsage = data_->deviceBufferUsage;
    const auto previousHostBufferUsage = data_->hostBufferUsage;
    Gpu::BufferUsageFlags requiredDeviceUsage{};
    Gpu::BufferUsageFlags requiredHostUsage{};

    for (const auto &handle : handles) {
        auto &res = data_->bufferResources_[handle];
        if (res.state.status != BufferMemoryStatus::Virtual) {
            continue;
        }

        if (res.arena == BufferResource::Arena::HostMapped) {
            hostOffset = alignUp(hostOffset, alignment);
            res.offset = hostOffset;
            hostOffset += res.info.size;
            requiredHostUsage |= res.info.usage;
            res.lastUsedFrameNumber = data_->currentFrameNumber;
        }
        else {
            deviceOffset = alignUp(deviceOffset, alignment);
            res.offset = deviceOffset;
            deviceOffset += res.info.size;
            requiredDeviceUsage |= res.info.usage;
            res.lastUsedFrameNumber = data_->currentFrameNumber;
        }
    }

    const auto requiredDeviceSize = deviceOffset;
    const auto deviceUsageWithAddress =
        requiredDeviceUsage | Gpu::BufferUsageFlagBits::ShaderDeviceAddressBit;
    const bool needsDeviceBuffer =
        requiredDeviceSize > 0 &&
        (!data_->deviceBuffer.isValid() || previousDeviceBufferSize < requiredDeviceSize ||
         (previousDeviceBufferUsage & deviceUsageWithAddress) != deviceUsageWithAddress);

    if (needsDeviceBuffer) {
        CO_CORE_TRACE("Allocating framegraph device buffer ({} bytes)", requiredDeviceSize);
        if (data_->deviceBuffer.isValid()) {
            data_->ctx_->resources().deleteBuffer(data_->deviceBuffer);
        }
        data_->deviceBuffer = data_->ctx_->resources().createBuffer(
            data_->ctx_->device(),
            Gpu::BufferOptions{.label = "Framegraph Device Buffer",
                               .size = requiredDeviceSize,
                               .usage = deviceUsageWithAddress,
                               .memoryUsage = Gpu::MemoryUsage::GpuOnly},
            nullptr);
        data_->deviceBufferSize = requiredDeviceSize;
        data_->deviceBufferUsage = deviceUsageWithAddress;
    }
    else if (requiredDeviceSize == 0) {
        data_->deviceBufferSize = previousDeviceBufferSize;
        data_->deviceBufferUsage = previousDeviceBufferUsage;
    }

    const bool needsHostBuffer =
        hostOffset > 0 && (!data_->hostBuffer || previousHostBufferSize < hostOffset ||
                           (previousHostBufferUsage & requiredHostUsage) != requiredHostUsage);
    if (needsHostBuffer) {
        CO_CORE_TRACE("Allocating framegraph host buffer ({} bytes)", hostOffset);
        data_->hostBuffer = std::make_unique<MappedCoherentDeviceBuffer>(
            data_->ctx_->device(),
            MappedCoherentDeviceBufferCreateInfo{
                .label = "Framegraph Host Buffer",
                .size = hostOffset,
                .usage = static_cast<VkBufferUsageFlags>(requiredHostUsage.toInt()),
            });
        data_->hostBufferSize = hostOffset;
        data_->hostBufferUsage = requiredHostUsage;
    }
    else if (hostOffset == 0) {
        data_->hostBufferSize = previousHostBufferSize;
        data_->hostBufferUsage = previousHostBufferUsage;
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
    auto &resource = data_->bufferResources_[handle];
    auto &state = resource.state;
    const auto view = bufferView(handle);
    VkBuffer bufferHandle = VK_NULL_HANDLE;
    if (resource.arena == BufferResource::Arena::External) {
        auto *bufferResource = data_->ctx_->resources().getBuffer(resource.externalBuffer);
        CO_CORE_DEBUG_ASSERT(bufferResource != nullptr, "Buffer resource is null");
        bufferHandle = bufferResource->buffer;
    }
    else if (resource.arena == BufferResource::Arena::HostMapped) {
        CO_CORE_ASSERT(data_->hostBuffer != nullptr, "Host buffer was not allocated");
        bufferHandle = data_->hostBuffer->buffer();
    }
    else {
        CO_CORE_ASSERT(data_->deviceBuffer.isValid(), "Device buffer was not allocated");
        auto *bufferResource = data_->ctx_->resources().getBuffer(data_->deviceBuffer);
        CO_CORE_DEBUG_ASSERT(bufferResource != nullptr, "Device buffer resource is null");
        bufferHandle = bufferResource->buffer;
    }

    Sync::BufferBarrier barrier{.prevAccesses{state.lastAccess},
                                .nextAccesses{access},
                                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .buffer = bufferHandle,
                                .offset = view.offset,
                                .size = view.size};

    CO_CORE_TRACE("BARRIER buffer '{}' written as {}, read as {}",
                  resource.info.name,
                  state.lastAccess,
                  access);

    state.lastAccess = access;
    resource.lastUsedFrameNumber = data_->currentFrameNumber;
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
        auto hostRange =
            std::span{baseAllocation.cpu, gsl::narrow_cast<size_t>(baseAllocation.size)};
        auto hostSubRange = hostRange.subspan(gsl::narrow_cast<size_t>(res.offset));
        return FramegraphBufferView{
            .deviceAddress = baseAllocation.gpu + res.offset,
            .offset = res.offset,
            .size = res.info.size,
            .cpu = hostSubRange.data(),
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

void FramegraphResourceManager::clearFrame(uint64_t frameNumber)
{
    if (!data_) {
        return;
    }
    std::vector<FramegraphTextureHandle> textureHandlesToRelease;
    for (const auto &handle : data_->textureResources_.handles()) {
        const auto &resource = data_->textureResources_[handle];
        if (resource.lastUsedFrameNumber != frameNumber) {
            continue;
        }
        if (resource.state.status == TextureMemoryStatus::Allocated) {
            data_->ctx_->resources().deleteTexture(resource.image);
            data_->ctx_->resources().deleteTextureView(resource.view);
        }
        textureHandlesToRelease.push_back(handle);
    }
    for (const auto &handle : textureHandlesToRelease) {
        data_->textureResources_.release(handle);
    }

    std::vector<FramegraphBufferHandle> bufferHandlesToRelease;
    for (const auto &handle : data_->bufferResources_.handles()) {
        const auto &resource = data_->bufferResources_[handle];
        if (resource.lastUsedFrameNumber != frameNumber) {
            continue;
        }
        bufferHandlesToRelease.push_back(handle);
    }
    for (const auto &handle : bufferHandlesToRelease) {
        data_->bufferResources_.release(handle);
    }
}

void FramegraphResourceManager::clearAll()
{
    if (!data_) {
        return;
    }
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

    data_->currentFrameNumber = 0;
}

} // namespace Cory
