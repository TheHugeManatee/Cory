#include <Cory/Renderer/FrameCapture.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/SingleShotCommandRecorder.hpp>

#include <KDGpu/buffer_options.h>
#include <KDGpu/command_recorder.h>
#include <KDGpu/texture.h>
#include <gsl/narrow>

#include <algorithm>
#include <cstring>
#include <system_error>

namespace Cory {
namespace {

constexpr size_t kRgbaBytesPerPixel = 4;

[[nodiscard]] Gpu::TextureMemoryBarrierOptions makeReadbackBarrier(const Texture &texture,
                                                                   Gpu::TextureLayout currentLayout)
{
    switch (currentLayout) {
    case Gpu::TextureLayout::TransferSrcOptimal:
        return Gpu::TextureMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::TopOfPipeBit,
            .srcMask = Gpu::AccessFlagBit::None,
            .dstStages = Gpu::PipelineStageFlagBit::TransferBit,
            .dstMask = Gpu::AccessFlagBit::TransferReadBit,
            .oldLayout = Gpu::TextureLayout::TransferSrcOptimal,
            .newLayout = Gpu::TextureLayout::TransferSrcOptimal,
            .texture = texture.handle(),
            .range = {.aspectMask = Gpu::TextureAspectFlagBits::ColorBit,
                      .baseMipLevel = 0,
                      .levelCount = 1,
                      .baseArrayLayer = 0,
                      .layerCount = 1},
        };
    case Gpu::TextureLayout::ColorAttachmentOptimal:
        return Gpu::TextureMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::ColorAttachmentOutputBit,
            .srcMask = Gpu::AccessFlagBit::ColorAttachmentWriteBit,
            .dstStages = Gpu::PipelineStageFlagBit::TransferBit,
            .dstMask = Gpu::AccessFlagBit::TransferReadBit,
            .oldLayout = Gpu::TextureLayout::ColorAttachmentOptimal,
            .newLayout = Gpu::TextureLayout::TransferSrcOptimal,
            .texture = texture.handle(),
            .range = {.aspectMask = Gpu::TextureAspectFlagBits::ColorBit,
                      .baseMipLevel = 0,
                      .levelCount = 1,
                      .baseArrayLayer = 0,
                      .layerCount = 1},
        };
    case Gpu::TextureLayout::PresentSrc:
        return Gpu::TextureMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::BottomOfPipeBit,
            .srcMask = Gpu::AccessFlagBit::None,
            .dstStages = Gpu::PipelineStageFlagBit::TransferBit,
            .dstMask = Gpu::AccessFlagBit::TransferReadBit,
            .oldLayout = Gpu::TextureLayout::PresentSrc,
            .newLayout = Gpu::TextureLayout::TransferSrcOptimal,
            .texture = texture.handle(),
            .range = {.aspectMask = Gpu::TextureAspectFlagBits::ColorBit,
                      .baseMipLevel = 0,
                      .levelCount = 1,
                      .baseArrayLayer = 0,
                      .layerCount = 1},
        };
    case Gpu::TextureLayout::TransferDstOptimal:
        return Gpu::TextureMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::TransferBit,
            .srcMask = Gpu::AccessFlagBit::TransferWriteBit,
            .dstStages = Gpu::PipelineStageFlagBit::TransferBit,
            .dstMask = Gpu::AccessFlagBit::TransferReadBit,
            .oldLayout = Gpu::TextureLayout::TransferDstOptimal,
            .newLayout = Gpu::TextureLayout::TransferSrcOptimal,
            .texture = texture.handle(),
            .range = {.aspectMask = Gpu::TextureAspectFlagBits::ColorBit,
                      .baseMipLevel = 0,
                      .levelCount = 1,
                      .baseArrayLayer = 0,
                      .layerCount = 1},
        };
    default:
        CO_CORE_ASSERT(false,
                       "readbackTextureRgba8 only supports TransferSrcOptimal, TransferDstOptimal, ColorAttachmentOptimal, or PresentSrc layouts");
        return Gpu::TextureMemoryBarrierOptions{};
    }
}

[[nodiscard]] IO::BmpImageRgba8 readbackBufferToBmp(Gpu::Buffer &buffer,
                                                    glm::u32vec2 size,
                                                    Gpu::Format format)
{
    CO_CORE_ASSERT(format == Gpu::Format::B8G8R8A8_UNORM || format == Gpu::Format::R8G8B8A8_UNORM,
                   "readbackTextureRgba8 currently supports only BGRA8/RGBA8 UNORM textures");

    const auto byteSize = static_cast<size_t>(size.x) * static_cast<size_t>(size.y) * kRgbaBytesPerPixel;
    buffer.invalidate();
    const auto *mapped = static_cast<const std::byte *>(buffer.map());
    CO_CORE_ASSERT(mapped != nullptr, "Failed to map readback buffer");

    IO::BmpImageRgba8 image{.width = size.x, .height = size.y};
    image.pixelsRgba8.resize(byteSize);
    if (format == Gpu::Format::R8G8B8A8_UNORM) {
        std::memcpy(image.pixelsRgba8.data(), mapped, byteSize);
    }
    else {
        for (size_t i = 0; i < byteSize; i += 4) {
            image.pixelsRgba8[i + 0] = mapped[i + 2];
            image.pixelsRgba8[i + 1] = mapped[i + 1];
            image.pixelsRgba8[i + 2] = mapped[i + 0];
            image.pixelsRgba8[i + 3] = mapped[i + 3];
        }
    }
    buffer.unmap();
    return image;
}

} // namespace

IO::BmpImageRgba8 readbackTextureRgba8(Context &ctx,
                                       const Texture &texture,
                                       glm::u32vec2 size,
                                       Gpu::Format format,
                                       Gpu::TextureLayout currentLayout)
{
    const auto byteSize = static_cast<Gpu::DeviceSize>(size.x) *
                          static_cast<Gpu::DeviceSize>(size.y) *
                          static_cast<Gpu::DeviceSize>(kRgbaBytesPerPixel);
    auto readback = ctx.device().createBuffer(
        Gpu::BufferOptions{.label = "TextureReadback",
                           .size = byteSize,
                           .usage = Gpu::BufferUsageFlagBits::TransferDstBit,
                           .memoryUsage = Gpu::MemoryUsage::CpuOnly});

    {
        SingleShotCommandRecorder recorder{ctx};
        if (currentLayout != Gpu::TextureLayout::TransferSrcOptimal) {
            recorder->textureMemoryBarrier(makeReadbackBarrier(texture, currentLayout));
        }
        recorder->copyTextureToBuffer(Gpu::TextureToBufferCopy{
            .srcTexture = texture.handle(),
            .srcTextureLayout = Gpu::TextureLayout::TransferSrcOptimal,
            .dstBuffer = readback.handle(),
            .regions = {{
                .textureSubResource = {.aspectMask = Gpu::TextureAspectFlagBits::ColorBit},
                .textureExtent = {size.x, size.y, 1},
            }},
        });
    }

    return readbackBufferToBmp(readback, size, format);
}

void writeCapturedFrameBmp(Context &ctx, const CapturedFrame &frame, const std::filesystem::path &path)
{
    CO_CORE_ASSERT(frame.texture != nullptr, "CapturedFrame does not contain a texture");
    const auto image = readbackTextureRgba8(ctx,
                                            *frame.texture,
                                            frame.extent,
                                            frame.format,
                                            frame.layout);
    const auto result = IO::writeBmpRgba8(path, image);
    CO_CORE_ASSERT(result.has_value(), "{}", result.error());
}

} // namespace Cory
