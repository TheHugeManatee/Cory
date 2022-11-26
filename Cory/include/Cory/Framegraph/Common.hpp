#pragma once

#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/flextVkExt.h>

#include <Cory/Renderer/APIConversion.hpp>

#include <functional>

namespace Cory::Framegraph {

struct RenderTaskInfo;
struct RenderTaskExecutionAwaiter;
class Framegraph;
class Builder;
class TextureResourceManager;
class CommandList;

enum class CullMode { None, Front, Back, FrontAndBack };
enum class DepthTest {
    Disabled,       ///< disables depth test
    Less,           ///< VK_COMPARE_OP_LESS
    Greater,        ///< VK_COMPARE_OP_GREATER
    LessOrEqual,    ///< VK_COMPARE_OP_LESS_OR_EQUAL
    GreaterOrEqual, ///< VK_COMPARE_OP_GREATER_OR_EQUAL
    Always,         ///< VK_COMPARE_OP_ALWAYS
    Never           ///< VK_COMPARE_OP_NEVER
};
enum class DepthWrite { Enabled, Disabled };

class TransientRenderPass;
/// for this value value, framegraph will automatically fit the render area to the frame buffer
static constexpr VkRect2D RENDER_AREA_AUTO{{0, 0}, {0, 0}};
struct DynamicStates {
    VkRect2D renderArea{RENDER_AREA_AUTO};
    CullMode cullMode{CullMode::Back};
    DepthTest depthTest{DepthTest::Less};
    DepthWrite depthWrite{DepthWrite::Enabled};
};

enum class PassOutputKind { Create, Write };
enum class PixelFormat { D32, RGBA32, RGBA8 };
enum class Layout { Undefined, Attachment, ReadOnly, TransferSource, TransferDest, PresentSource };
using PipelineStages = BitField<VkPipelineStageFlagBits2>;
using ImageAspects = BitField<VkImageAspectFlagBits>;
using AccessFlags = BitField<VkAccessFlagBits2>;

using RenderTaskHandle = PrivateTypedHandle<RenderTaskInfo, Framegraph>;

enum class TextureMemoryStatus { Virtual, Allocated, External };

struct TextureInfo {
    std::string name;
    glm::u32vec3 size;
    PixelFormat format;
    int32_t sampleCount{1};
};

struct TextureState {
    Layout layout{Layout::Undefined};
    AccessFlags lastWriteAccess{VK_ACCESS_NONE};
    PipelineStages lastWriteStage{VK_PIPELINE_STAGE_NONE};
    TextureMemoryStatus status{TextureMemoryStatus::Virtual};
};

/// describes information about the intended access (read or write) for a texture resource
struct TextureAccessInfo {
    Layout layout;
    PipelineStages stage;
    AccessFlags access;
    ImageAspects imageAspect;
};

using TextureHandle = PrivateTypedHandle<TextureInfo, const TextureResourceManager>;
using MutableTextureHandle = PrivateTypedHandle<TextureInfo, TextureResourceManager>;

constexpr VkImageLayout toVkImageLayout(Layout layout)
{
    switch (layout) {
    case Layout::Undefined:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case Layout::Attachment:
        return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    case Layout::ReadOnly:
        return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
    case Layout::TransferSource:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case Layout::TransferDest:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case Layout::PresentSource:
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }
    throw std::runtime_error{"Unknown Layout"};
}

constexpr VkFormat toVkFormat(PixelFormat format)
{
    switch (format) {
    case PixelFormat::D32:
        return VK_FORMAT_D32_SFLOAT;
    case PixelFormat::RGBA32:
        return VK_FORMAT_R8G8B8A8_SRGB;
    }
    throw std::runtime_error{"Unknown format"};
}

constexpr Magnum::Vk::PixelFormat toMagnumPixelFormat(PixelFormat format)
{
    switch (format) {
    case PixelFormat::D32:
        return Magnum::Vk::PixelFormat::Depth32F;
    case PixelFormat::RGBA32:
        return Magnum::Vk::PixelFormat::RGBA8Srgb;
    }
    throw std::runtime_error{"Unknown format"};
}

constexpr bool isColorFormat(PixelFormat format)
{
    switch (format) {
    case PixelFormat::D32:
        return false;
    case PixelFormat::RGBA32:
    case PixelFormat::RGBA8:
        return true;
    }
    throw std::runtime_error("Unknown format");
}

constexpr bool isDepthFormat(PixelFormat format)
{
    switch (format) {
    case PixelFormat::D32:
        return true;
    case PixelFormat::RGBA32:
    case PixelFormat::RGBA8:
        return false;
    }
    throw std::runtime_error("Unknown format");
}

constexpr bool isStencilFormat(PixelFormat format)
{
    switch (format) {
    case PixelFormat::D32:
    case PixelFormat::RGBA32:
    case PixelFormat::RGBA8:
        return false;
    }
    throw std::runtime_error("Unknown format");
}
} // namespace Cory::Framegraph