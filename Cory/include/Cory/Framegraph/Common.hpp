#pragma once

#include <Cory/Renderer/Common.hpp>

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

/// for this value value, framegraph will automatically fit the render area to the frame buffer
static constexpr VkRect2D RENDER_AREA_AUTO{{0, 0}, {0, 0}};
struct DynamicStates {
    VkRect2D renderArea{RENDER_AREA_AUTO};
    CullMode cullMode{CullMode::Back};
    DepthTest depthTest{DepthTest::Less};
    DepthWrite depthWrite{DepthWrite::Enabled};
};

enum class PassOutputKind { Create, Write };
enum class PixelFormat { D32, RGBA32 };
enum class Layout { Undefined, Color, DepthStencil, TransferSource, PresentSource };
using PipelineStages = BitField<VkPipelineStageFlagBits>;
using ImageAspects = BitField<VkImageAspectFlagBits>;
using AccessFlags = BitField<VkAccessFlagBits>;

using RenderTaskHandle = PrivateTypedHandle<RenderTaskInfo, Framegraph>;
using TransientTextureHandle = PrivateTypedHandle<Magnum::Vk::Image, TextureResourceManager>;

enum class TextureMemoryStatus { Virtual, Allocated, External };

struct TextureInfo {
    std::string name;
    glm::u32vec3 size;
    PixelFormat format;
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
    AccessFlags access;
    PipelineStages stage;
    ImageAspects imageAspect;
};

using TextureHandle = PrivateTypedHandle<TextureInfo, TextureResourceManager>;
using MutableTextureHandle = PrivateTypedHandle<TextureInfo, TextureResourceManager>;
} // namespace Cory::Framegraph
