#include <Cory/Framegraph/TextureManager.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Context.hpp>
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

struct TextureManagerPrivate {
    Context *ctx_{};
    SlotMap<TextureResource> textureResources_;
};

TextureManager::TextureManager(Context &ctx)
    : data_{std::make_unique<TextureManagerPrivate>()}
{
    data_->ctx_ = &ctx;
}

TextureManager::~TextureManager() = default;
TextureManager::TextureManager(TextureManager &&) noexcept = default;
TextureManager &TextureManager::operator=(TextureManager &&) noexcept = default;

FramegraphTextureHandle TextureManager::declareTexture(TextureInfo info)
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

FramegraphTextureHandle TextureManager::registerExternal(TextureInfo info,
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

void TextureManager::allocate(FramegraphTextureHandle handle)
{
    TextureResource &res = data_->textureResources_[handle];
    Gpu::DeviceHandle deviceHandle = data_->ctx_->device();
    auto &resources = data_->ctx_->resources();
    CO_CORE_DEBUG("Allocating '{}' of {} ({})", res.info.name, res.info.size, res.info.format);

    // {
    //     const auto size = Magnum::Vector2i{gsl::narrow<int32_t>(res.info.size.x),
    //                                        gsl::narrow<int32_t>(res.info.size.y)};
    //     static const int32_t levels = 1;
    //     static const Magnum::Vk::ImageLayout initialLayout{Magnum::Vk::ImageLayout::Undefined};
    //
    //     Vk::ImageUsages usage{};
    //     usage |= isDepthFormat(res.info.format) ? Vk::ImageUsage::DepthStencilAttachment
    //                                             : Vk::ImageUsage::ColorAttachment;
    //     usage |= Vk::ImageUsage::Sampled;
    //     usage |= Vk::ImageUsage::InputAttachment;
    //
    //     const Vk::ImageCreateInfo2D createInfo{
    //         usage, res.info.format, size, levels, res.info.sampleCount, initialLayout};
    //
    //     // todo eventually want to externalize these memory flags
    //     res.image = resources.createImage(
    //         fmt::format("{} (IMG)", res.info.name), createInfo, Vk::MemoryFlag::DeviceLocal);
    // }
    //
    // {
    //     const Vk::ImageViewCreateInfo2D createInfo{resources[res.image]};
    //     res.view = resources.createImageView(fmt::format("{} (VIEW)", res.info.name),
    //     createInfo);
    // }

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

void TextureManager::allocate(const std::vector<FramegraphTextureHandle> &handles)
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

Sync::ImageBarrier TextureManager::synchronizeTexture(FramegraphTextureHandle handle,
                                                      Sync::AccessType access,
                                                      ImageContents contentsMode)
{
    const auto &info = data_->textureResources_[handle].info;
    auto aspectMask = flagsForFormat(info.format);
    auto &state = data_->textureResources_[handle].state;

    VkImage vkImageHandle = data_->ctx_->resources().getTexture(image(handle))->image;
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

const TextureInfo &TextureManager::info(FramegraphTextureHandle handle) const
{
    return data_->textureResources_[handle].info;
}

Gpu::TextureHandle TextureManager::image(FramegraphTextureHandle handle) const
{
    return data_->textureResources_[handle].image;
}

Gpu::TextureViewHandle TextureManager::imageView(FramegraphTextureHandle handle) const
{
    return data_->textureResources_[handle].view;
}

TextureState TextureManager::state(FramegraphTextureHandle handle) const
{
    return data_->textureResources_[handle].state;
}

void TextureManager::clear()
{
    for (auto &res : data_->textureResources_) {
        if (res.state.status == TextureMemoryStatus::Allocated) {
            data_->ctx_->resources().deleteTexture(res.image);
            data_->ctx_->resources().deleteTextureView(res.view);
        }
    }
    data_->textureResources_.clear();
}

} // namespace Cory