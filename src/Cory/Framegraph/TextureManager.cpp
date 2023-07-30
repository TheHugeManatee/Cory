#include <Cory/Framegraph/TextureManager.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/ResourceManager.hpp>

#include <Magnum/Vk/CommandBuffer.h>
#include <Magnum/Vk/Device.h>
#include <Magnum/Vk/ImageCreateInfo.h>
#include <Magnum/Vk/ImageViewCreateInfo.h>
#include <gsl/narrow>

namespace Vk = Magnum::Vk;

namespace Cory {

struct TextureResource {
    TextureInfo info;
    TextureState state;
    ImageHandle image;
    ImageViewHandle view;
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
TextureManager::TextureManager(TextureManager &&) = default;
TextureManager &TextureManager::operator=(TextureManager &&) = default;

TextureHandle TextureManager::declareTexture(TextureInfo info)
{
    CO_CORE_DEBUG("Declaring '{}' of {} ({}, {} samples)",
                  info.name,
                  info.size,
                  info.format,
                  info.sampleCount);

    auto handle = data_->textureResources_.emplace(TextureResource{
        info,
        TextureState{.lastAccess = Sync::AccessType::None, .status = TextureMemoryStatus::Virtual},
        NullHandle,
        NullHandle});
    return handle;
}

TextureHandle TextureManager::registerExternal(TextureInfo info,
                                               Sync::AccessType lastWriteAccess,
                                               Magnum::Vk::Image &resource,
                                               Magnum::Vk::ImageView &resourceView)
{
    auto &resources = data_->ctx_->resources();
    auto handle = data_->textureResources_.emplace(
        TextureResource{.info = info,
                        .state = TextureState{.lastAccess = lastWriteAccess,
                                              .status = TextureMemoryStatus::External},
                        .image = resources.wrapImage(info.name, resource),
                        .view = resources.wrapImageView(info.name, resourceView)});

    return handle;
}

void TextureManager::allocate(TextureHandle handle)
{
    TextureResource &res = data_->textureResources_[handle];
    auto &resources = data_->ctx_->resources();
    CO_CORE_DEBUG("Allocating '{}' of {} ({})", res.info.name, res.info.size, res.info.format);

    // TODO allocate from a big buffer instead of individual allocations
    {
        const auto size = Magnum::Vector2i{gsl::narrow<int32_t>(res.info.size.x),
                                           gsl::narrow<int32_t>(res.info.size.y)};
        static const int32_t levels = 1;
        static const Magnum::Vk::ImageLayout initialLayout{Magnum::Vk::ImageLayout::Undefined};

        Vk::ImageUsages usage{};
        usage |= isDepthFormat(res.info.format) ? Vk::ImageUsage::DepthStencilAttachment
                                                : Vk::ImageUsage::ColorAttachment;
        usage |= Vk::ImageUsage::Sampled;
        usage |= Vk::ImageUsage::InputAttachment;

        const Vk::ImageCreateInfo2D createInfo{
            usage, res.info.format, size, levels, res.info.sampleCount, initialLayout};

        // todo eventually want to externalize these memory flags
        res.image = resources.createImage(
            fmt::format("{} (IMG)", res.info.name), createInfo, Vk::MemoryFlag::DeviceLocal);
    }

    {
        const Vk::ImageViewCreateInfo2D createInfo{resources[res.image]};
        res.view = resources.createImageView(fmt::format("{} (VIEW)", res.info.name), createInfo);
    }
    res.state.status = TextureMemoryStatus::Allocated;
}

void TextureManager::allocate(const std::vector<TextureHandle> &handles)
{
    for (const auto &handle : handles) {
        auto &res = data_->textureResources_[handle];
        // don't allocate external resources or resources that are already allocated
        if (res.state.status != TextureMemoryStatus::Virtual) { continue; }

        allocate(handle);
    }
}

Sync::ImageBarrier TextureManager::synchronizeTexture(TextureHandle handle,
                                                      Sync::AccessType access,
                                                      ImageContents contentsMode)
{
    const auto &info = data_->textureResources_[handle].info;
    auto aspectMask = VkImageAspectFlags(imageAspectsFor(info.format));
    auto &state = data_->textureResources_[handle].state;

    const VkBool32 discard = (contentsMode == ImageContents::Discard) ? VK_TRUE : VK_FALSE;
    Sync::ImageBarrier barrier{.prevAccesses{state.lastAccess},
                               .nextAccesses{access},
                               .prevLayout = Sync::ImageLayout::Optimal,
                               .nextLayout = Sync::ImageLayout::Optimal,
                               .discardContents = discard,
                               // todo: probably problematic once we actually use more queues
                               .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                               .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                               .image = data_->ctx_->resources()[image(handle)],
                               .subresourceRange = {
                                   .aspectMask = aspectMask,
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

const TextureInfo &TextureManager::info(TextureHandle handle) const
{
    return data_->textureResources_[handle].info;
}

ImageHandle TextureManager::image(TextureHandle handle) const
{
    return data_->textureResources_[handle].image;
}

ImageViewHandle TextureManager::imageView(TextureHandle handle) const
{
    return data_->textureResources_[handle].view;
}

TextureState TextureManager::state(TextureHandle handle) const
{
    return data_->textureResources_[handle].state;
}

void TextureManager::clear()
{
    for (auto &res : data_->textureResources_) {
        if (res.state.status == TextureMemoryStatus::Allocated) {
            data_->ctx_->resources().release(res.image);
            data_->ctx_->resources().release(res.view);
        }
    }
    data_->textureResources_.clear();
}

} // namespace Cory