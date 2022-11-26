#include <Cory/Framegraph/TextureManager.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Context.hpp>

#include <Magnum/Vk/CommandBuffer.h>
#include <Magnum/Vk/Device.h>
#include <Magnum/Vk/ImageCreateInfo.h>
#include <Magnum/Vk/ImageViewCreateInfo.h>

namespace Vk = Magnum::Vk;

namespace Cory::Framegraph {

struct TextureResource {
    TextureInfo info;
    TextureState state;
    Vk::Image image{Corrade::NoCreate};
    Vk::ImageView view{Corrade::NoCreate};
};

struct TextureManagerPrivate {
    Context *ctx_;
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
    CO_CORE_DEBUG("Declaring '{}' of {}x{}x{} ({}, {} samples)",
                  info.name,
                  info.size.x,
                  info.size.y,
                  info.size.z,
                  info.format,
                  info.sampleCount);
    auto handle = data_->textureResources_.emplace(
        TextureResource{info,
                        TextureState{.layout = Layout::Undefined,
                                     .lastWriteAccess = VK_ACCESS_NONE,
                                     .lastWriteStage = VK_PIPELINE_STAGE_NONE,
                                     .status = TextureMemoryStatus::Virtual},
                        Vk::Image{Corrade::NoCreate},
                        Vk::ImageView{Corrade::NoCreate}});
    return handle;
}

TextureHandle TextureResourceManager::registerExternal(TextureInfo info,
                                                       Layout layout,
                                                       AccessFlags lastWriteAccess,
                                                       PipelineStages lastWriteStage,
                                                       Magnum::Vk::Image &resource)
{
    auto handle = data_->textureResources_.emplace(
        TextureResource{info,
                        TextureState{.layout = layout,
                                     .lastWriteAccess = lastWriteAccess,
                                     .lastWriteStage = lastWriteStage,
                                     .status = TextureMemoryStatus::External},
                        Vk::Image::wrap(data_->ctx_->device(), resource, resource.format()),
                        Vk::ImageView{Corrade::NoCreate}});

    // data_->textureResources_[handle].image.;
    return handle;
}

void TextureResourceManager::allocate(TextureHandle handle)
{
    TextureResource &r = data_->textureResources_[handle];
    CO_CORE_DEBUG("Allocating '{}' of {}x{}x{} ({})",
                  r.info.name,
                  r.info.size.x,
                  r.info.size.y,
                  r.info.size.z,
                  r.info.format);
    // TODO allocate from a big buffer instead of individual allocations
    // r.image = Vk::Image{data_->ctx_->device(), Vk::ImageCreateInfo{/*...*/}};
    // r.view = Vk::ImageView{data_->ctx_->device(), Vk::ImageCreateInfo{/*...*/}};
    r.state.status = TextureMemoryStatus::Allocated;
}

void TextureResourceManager::allocate(const std::vector<TextureHandle> &handles)
{
    for (const auto &handle : handles) {
        auto &r = data_->textureResources_[handle];
        // don't allocate external resources or resources that are already allocated
        if (r.state.status != TextureMemoryStatus::Virtual) { continue; }

        allocate(handle);
    }
}

void TextureResourceManager::readBarrier(Magnum::Vk::CommandBuffer &cmdBuffer,
                                         TextureHandle handle,
                                         TextureAccessInfo readAccessInfo)
{
    auto &state = data_->textureResources_[handle].state;

    const VkImageAspectFlags aspectMask = readAccessInfo.imageAspect.bits();
    const VkImageMemoryBarrier2 imageMemoryBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = state.lastWriteStage.bits(),
        .srcAccessMask = state.lastWriteAccess.bits(),
        .dstStageMask = readAccessInfo.stage.bits(),
        .dstAccessMask = readAccessInfo.stage.bits(),
        .oldLayout = toVkImageLayout(state.layout),
        .newLayout = toVkImageLayout(readAccessInfo.layout),
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
    const VkDependencyInfo dependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                          .pNext = nullptr,
                                          .dependencyFlags = {}, // ?
                                          .memoryBarrierCount = 0,
                                          .pMemoryBarriers = nullptr,
                                          .bufferMemoryBarrierCount = 0,
                                          .pBufferMemoryBarriers = nullptr,
                                          .imageMemoryBarrierCount = 1,
                                          .pImageMemoryBarriers = &imageMemoryBarrier};
    data_->ctx_->device()->CmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
    state.layout = readAccessInfo.layout;
}

void TextureResourceManager::recordWrite(Magnum::Vk::CommandBuffer &cmdBuffer,
                                         TextureHandle handle,
                                         TextureAccessInfo writeAccessInfo)
{
    auto &state = data_->textureResources_[handle].state;
    state.layout = writeAccessInfo.layout;
    state.lastWriteStage = writeAccessInfo.stage;
    state.lastWriteAccess = writeAccessInfo.access;
}

void TextureResourceManager::readWriteBarrier(Magnum::Vk::CommandBuffer &cmdBuffer,
                                              TextureHandle handle,
                                              TextureAccessInfo readAccessInfo,
                                              TextureAccessInfo writeAccessInfo)
{
    auto &state = data_->textureResources_[handle].state;

    const VkImageAspectFlags aspectMask = writeAccessInfo.imageAspect.bits();
    const VkImageMemoryBarrier2 imageMemoryBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = state.lastWriteStage.bits(),
        .srcAccessMask = state.lastWriteAccess.bits(),
        .dstStageMask = writeAccessInfo.stage.bits(),
        .dstAccessMask = writeAccessInfo.stage.bits(),
        .oldLayout = toVkImageLayout(state.layout),
        .newLayout = toVkImageLayout(writeAccessInfo.layout),
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
    const VkDependencyInfo dependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                          .pNext = nullptr,
                                          .dependencyFlags = {}, // ?
                                          .memoryBarrierCount = 0,
                                          .pMemoryBarriers = nullptr,
                                          .bufferMemoryBarrierCount = 0,
                                          .pBufferMemoryBarriers = nullptr,
                                          .imageMemoryBarrierCount = 1,
                                          .pImageMemoryBarriers = &imageMemoryBarrier};
    data_->ctx_->device()->CmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
    state.layout = writeAccessInfo.layout;
    state.lastWriteStage = writeAccessInfo.stage;
    state.lastWriteAccess = writeAccessInfo.access;
}

const TextureInfo &TextureResourceManager::info(TextureHandle handle)
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

} // namespace Cory::Framegraph