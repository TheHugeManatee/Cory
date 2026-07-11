#pragma once

#include <Cory/IO/Bmp.hpp>
#include <Cory/Renderer/Gpu.hpp>

#include <filesystem>
#include <glm/vec2.hpp>

#include <cstdint>

namespace Cory {

class Context;

struct CapturedFrame {
    const Texture *texture{};
    glm::u32vec2 extent{};
    Gpu::Format format{};
    Gpu::TextureLayout layout{Gpu::TextureLayout::TransferSrcOptimal};
};

/**
 * @brief Read back a GPU texture into a tightly packed RGBA8 bitmap image.
 *
 * The source texture must currently be in either TransferSrcOptimal or a supported render-attachment
 * layout such as ColorAttachmentOptimal / PresentSrc.
 */
[[nodiscard]] IO::BmpImageRgba8
readbackTextureRgba8(Context &ctx,
                     const Texture &texture,
                     glm::u32vec2 size,
                     Gpu::Format format,
                     Gpu::TextureLayout currentLayout = Gpu::TextureLayout::TransferSrcOptimal);

void writeCapturedFrameBmp(Context &ctx, const CapturedFrame &frame, const std::filesystem::path &path);

} // namespace Cory
