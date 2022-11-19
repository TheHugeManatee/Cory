#pragma once

#include <Cory/Renderer/Common.hpp>

#include <functional>

namespace Cory::Framegraph {

struct RenderPassInfo;
struct RenderPassExecutionAwaiter;
class Framegraph;
class Builder;
class TextureResourceManager;

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
