#pragma once

#include <Cory/Renderer/Common.hpp>

#include <functional>

namespace Cory::Framegraph {

struct RenderPassInfo;
struct RenderPassExecutionAwaiter;
class Framegraph;
class Builder;
class TextureResourceManager;
class Commands;

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

enum class PassOutputKind {
    Create,
    Write,
};
enum class PixelFormat { D32, RGBA32 };
enum class Layout { Undefined, Color, DepthStencil, TransferSource, PresentSource };
enum class ResourceState { Clear, DontCare, Keep };
struct Texture;
struct TextureHandle;
struct MutableTextureHandle;

using RenderPassHandle = PrivateTypedHandle<RenderPassInfo, Framegraph>;
using TransientTextureHandle = PrivateTypedHandle<Texture, TextureResourceManager>;

} // namespace Cory::Framegraph
