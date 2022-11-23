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

TextureHandle TextureResourceManager::declareTexture(TextureInfo info, Layout initialLayout)
{
    CO_CORE_DEBUG("Creating '{}' of {}x{}x{} ({} {})",
                  info.name,
                  info.size.x,
                  info.size.y,
                  info.size.z,
                  info.format,
                  initialLayout);
    auto handle = data_->textureResources_.emplace(
        TextureResource{info,
                        TextureState{.layout = initialLayout,
                                     .lastWriteAccess = VK_ACCESS_NONE,
                                     .lastWriteStage = VK_PIPELINE_STAGE_NONE},
                        Vk::Image{Corrade::NoCreate},
                        Vk::ImageView{Corrade::NoCreate}});
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
}

void TextureResourceManager::readBarrier(Magnum::Vk::CommandBuffer &cmdBuffer,
                                         TextureHandle handle,
                                         TextureAccessInfo readAccessInfo)
{
    auto &state = data_->textureResources_[handle].state;

    const VkImageMemoryBarrier2 imageMemoryBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = state.lastWriteStage,
        .srcAccessMask = state.lastWriteAccess,
        .dstStageMask = readAccessInfo.stage,
        .dstAccessMask = readAccessInfo.stage,
        .oldLayout = state.layout,
        .newLayout = readAccessInfo.layout,
        .srcQueueFamilyIndex = data_->ctx_->graphicsQueueFamily(),
        .dstQueueFamilyIndex = data_->ctx_->graphicsQueueFamily(),
        .image = image(handle),
        .subresourceRange = {
            .aspectMask = readAccessInfo.imageAspect.underlying_bits(),
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

} // namespace Cory::Framegraph