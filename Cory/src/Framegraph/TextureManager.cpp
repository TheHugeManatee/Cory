#include <Cory/Framegraph/TextureManager.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Context.hpp>

#include <Magnum/Vk/CommandBuffer.h>
#include <Magnum/Vk/Device.h>
#include <Magnum/Vk/ImageCreateInfo.h>
#include <Magnum/Vk/ImageViewCreateInfo.h>
#include <gsl/narrow>
#include <utility>

namespace Vk = Magnum::Vk;

namespace Cory::Framegraph {

struct TextureResource {
    TextureInfo info;
    TextureState state;
    Vk::Image image{Corrade::NoCreate};
    Vk::ImageView view{Corrade::NoCreate};
};

struct TextureManagerPrivate {
    Context *ctx_{};
    SlotMap<TextureResource> textureResources_;
};

TextureResourceManager::TextureResourceManager(Context &ctx)
    : data_{std::make_unique<TextureManagerPrivate>()}
{
    data_->ctx_ = &ctx;
}

TextureResourceManager::~TextureResourceManager() = default;

TextureHandle TextureResourceManager::declareTexture(TextureInfo info)
{
    CO_CORE_DEBUG("Declaring '{}' of {} ({}, {} samples)",
                  info.name,
                  info.size,
                  info.format,
                  info.sampleCount);
    auto handle = data_->textureResources_.emplace(TextureResource{
        info,
        TextureState{.lastAccess = Sync::AccessType::None, .status = TextureMemoryStatus::Virtual},
        Vk::Image{Corrade::NoCreate},
        Vk::ImageView{Corrade::NoCreate}});
    return handle;
}

TextureHandle TextureResourceManager::registerExternal(TextureInfo info,
                                                       Sync::AccessType lastWriteAccess,
                                                       Magnum::Vk::Image &resource,
                                                       Magnum::Vk::ImageView &resourceView)
{
    auto handle = data_->textureResources_.emplace(TextureResource{
        std::move(info),
        TextureState{.lastAccess = lastWriteAccess, .status = TextureMemoryStatus::External},
        Vk::Image::wrap(data_->ctx_->device(), resource, resource.format()),
        Vk::ImageView::wrap(data_->ctx_->device(), resourceView)});

    return handle;
}

void TextureResourceManager::allocate(TextureHandle handle)
{
    TextureResource &res = data_->textureResources_[handle];
    CO_CORE_DEBUG("Allocating '{}' of {} ({})", res.info.name, res.info.size, res.info.format);
    // TODO allocate from a big buffer instead of individual allocations

    {
        const auto size = Magnum::Vector2i{gsl::narrow<int32_t>(res.info.size.x),
                                           gsl::narrow<int32_t>(res.info.size.y)};
        static const int32_t levels = 1;
        static const Magnum::Vk::ImageLayout initialLayout{Magnum::Vk::ImageLayout::Undefined};

        auto usage = isDepthFormat(res.info.format) ? Vk::ImageUsage::DepthStencilAttachment
                                                    : Vk::ImageUsage::ColorAttachment;

        const Vk::ImageCreateInfo2D createInfo{
            usage, res.info.format, size, levels, res.info.sampleCount, initialLayout};

        // todo eventually want to externalize these memory flags
        res.image = Vk::Image{data_->ctx_->device(), createInfo, Vk::MemoryFlag::DeviceLocal};

        nameVulkanObject(data_->ctx_->device(), res.image, res.info.name);
    }

    {
        const Vk::ImageViewCreateInfo2D createInfo{res.image};
        res.view = Vk::ImageView{data_->ctx_->device(), createInfo};
        nameVulkanObject(data_->ctx_->device(), res.image, res.info.name);
    }
    res.state.status = TextureMemoryStatus::Allocated;
}

void TextureResourceManager::allocate(const std::vector<TextureHandle> &handles)
{
    for (const auto &handle : handles) {
        auto &res = data_->textureResources_[handle];
        // don't allocate external resources or resources that are already allocated
        if (res.state.status != TextureMemoryStatus::Virtual) { continue; }

        allocate(handle);
    }
}

Sync::ImageBarrier TextureResourceManager::synchronizeTexture(Magnum::Vk::CommandBuffer &cmdBuffer,
                                                              TextureHandle handle,
                                                              Sync::AccessType readAccess,
                                                              ImageContents contentsMode)
{
    const auto &info = data_->textureResources_[handle].info;
    auto aspectMask = VkImageAspectFlags(imageAspectsFor(info.format));
    auto &state = data_->textureResources_[handle].state;

    const VkBool32 discard = (contentsMode == ImageContents::Discard) ? VK_TRUE : VK_FALSE;
    Sync::ImageBarrier barrier{.prevAccesses{},
                               .nextAccesses{},
                               .prevLayout = Sync::ImageLayout::Optimal,
                               .nextLayout = Sync::ImageLayout::Optimal,
                               .discardContents = discard,
                               // todo: we should get family somewhere else and not from the context
                               .srcQueueFamilyIndex = data_->ctx_->graphicsQueueFamily(),
                               .dstQueueFamilyIndex = data_->ctx_->graphicsQueueFamily(),
                               .image = image(handle),
                               .subresourceRange = {
                                   .aspectMask = aspectMask,
                                   .baseMipLevel = 0,
                                   .levelCount = 1,
                                   .baseArrayLayer = 0,
                                   .layerCount = 1,
                               }};

    CO_CORE_TRACE("BARRIER synchronizing data written to '{}' as to be read as {}",
                  info.name,
                  state.lastAccess,
                  readAccess);

    //    const VkImageMemoryBarrier2 imageMemoryBarrier{
    //        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    //        .srcStageMask = state.lastWriteStage.bits(),
    //        .srcAccessMask = state.lastAccess.bits(),
    //        .dstStageMask = readAccessInfo.stage.bits(),
    //        .dstAccessMask = readAccessInfo.access.bits(),
    //        .oldLayout = toVkImageLayout(state.layout),
    //        .newLayout = toVkImageLayout(readAccessInfo.layout),
    //        // todo: we should get family somewhere else and not from the context
    //        .srcQueueFamilyIndex = data_->ctx_->graphicsQueueFamily(),
    //        .dstQueueFamilyIndex = data_->ctx_->graphicsQueueFamily(),
    //        .image = image(handle),
    //        .subresourceRange = {
    //            .aspectMask = aspectMask,
    //            .baseMipLevel = 0,
    //            .levelCount = 1,
    //            .baseArrayLayer = 0,
    //            .layerCount = 1,
    //        }};
    //    const VkDependencyInfo dependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    //                                          .pNext = nullptr,
    //                                          .dependencyFlags = {}, // ?
    //                                          .memoryBarrierCount = 0,
    //                                          .pMemoryBarriers = nullptr,
    //                                          .bufferMemoryBarrierCount = 0,
    //                                          .pBufferMemoryBarriers = nullptr,
    //                                          .imageMemoryBarrierCount = 1,
    //                                          .pImageMemoryBarriers = &imageMemoryBarrier};
    //    data_->ctx_->device()->CmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
    state.lastAccess = readAccess;
    return barrier;
}

const TextureInfo &TextureResourceManager::info(TextureHandle handle) const
{
    return data_->textureResources_[handle].info;
}

Magnum::Vk::Image &TextureResourceManager::image(TextureHandle handle)
{
    return data_->textureResources_[handle].image;
}

Magnum::Vk::ImageView &TextureResourceManager::imageView(TextureHandle handle)
{
    return data_->textureResources_[handle].view;
}

TextureState TextureResourceManager::state(TextureHandle handle) const
{
    return data_->textureResources_[handle].state;
}

void TextureResourceManager::clear() { data_->textureResources_.clear(); }

} // namespace Cory::Framegraph