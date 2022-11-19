#pragma once

#include <Cory/Renderer/Common.hpp>

#include <functional>

namespace Cory::Framegraph {

struct RenderPassInfo;
class Framegraph;
struct RenderPassExecutionAwaiter;
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

class RenderPassHandle {
  public:
    RenderPassHandle() = default;
    auto operator<=>(const RenderPassHandle &rhs) const = default;

  private:
    friend class Builder;
    friend class Framegraph;
    friend struct std::hash<Cory::Framegraph::RenderPassHandle>;
    friend struct fmt::formatter<Cory::Framegraph::RenderPassHandle>;
    /* implicit */ RenderPassHandle(SlotMapHandle handle)
        : handle_{handle}
    {
    }

    operator SlotMapHandle() const noexcept { return handle_; }

    SlotMapHandle handle_{};
};
static_assert(sizeof(RenderPassHandle) == sizeof(SlotMapHandle));

template <typename T> class TransientResourceHandle {
  public:
    TransientResourceHandle() = default;
    auto operator<=>(const TransientResourceHandle &rhs) const = default;

  private:
    friend class TextureResourceManager;
    friend struct std::hash<Cory::Framegraph::TransientResourceHandle<T>>;
    friend struct fmt::formatter<Cory::Framegraph::TransientResourceHandle<T>>;
    /* implicit */ TransientResourceHandle(SlotMapHandle handle)
        : handle_{handle}
    {
    }

    operator SlotMapHandle() const noexcept { return handle_; }

    SlotMapHandle handle_{};
};
using TransientTextureHandle = TransientResourceHandle<Texture>;

} // namespace Cory::Framegraph

template <> struct std::hash<Cory::Framegraph::RenderPassHandle> {
    std::size_t operator()(const Cory::Framegraph::RenderPassHandle &s) const noexcept
    {
        return std::hash<Cory::SlotMapHandle>{}(s);
    }
};

/// make TransientResourceHandle formattable
template <>
struct fmt::formatter<Cory::Framegraph::RenderPassHandle>
    : public fmt::formatter<Cory::SlotMapHandle> {
    auto format(Cory::Framegraph::RenderPassHandle h, format_context &ctx)
    {
        return fmt::formatter<Cory::SlotMapHandle>::format(h.handle_, ctx);
    }
};

/// make TransientResourceHandle formattable
template <typename T>
struct fmt::formatter<Cory::Framegraph::TransientResourceHandle<T>>
    : public fmt::formatter<Cory::SlotMapHandle> {
    auto format(Cory::Framegraph::TransientResourceHandle<T> h, format_context &ctx)
    {
        return fmt::formatter<Cory::SlotMapHandle>::format(h.handle_, ctx);
    }
};

// partial specialization needs to be in std, not sure if that's a good idea though..
namespace std {
template <typename T> struct hash<Cory::Framegraph::TransientResourceHandle<T>> {
    std::size_t operator()(const Cory::Framegraph::TransientResourceHandle<T> &s) const noexcept
    {
        return std::hash<Cory::SlotMapHandle>{}(s);
    }
};
} // namespace std
