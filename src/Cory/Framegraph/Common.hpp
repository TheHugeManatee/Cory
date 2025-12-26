#pragma once

#include <Cory/Renderer/Common.hpp>

#include <functional>

namespace Cory {

struct RenderTaskInfo;
struct RenderTaskExecutionAwaiter;
class Framegraph;
class RenderTaskBuilder;
class FramegraphResourceManager;
class FramegraphVisualizer;

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
static constexpr Gpu::Rect2D RENDER_AREA_AUTO{{0, 0}, {0, 0}};
struct DynamicStates {
    Gpu::Rect2D renderArea{RENDER_AREA_AUTO};
    CullMode cullMode{CullMode::Back};
    DepthTest depthTest{DepthTest::Less};
    DepthWrite depthWrite{DepthWrite::Enabled};
};

enum class TaskDependencyKindBits {
    Create = 1 << 0,
    Read = 1 << 1,
    Write = 1 << 2,
    ReadWrite = Read | Write,
    CreateWrite = Create | Write
};
using TaskDependencyKind = BitField<TaskDependencyKindBits>;
enum class ImageContents { Retain, Discard };

using RenderTaskHandle = PrivateTypedHandle<RenderTaskInfo, Framegraph>;

enum class TextureMemoryStatus { Virtual, Allocated, External };

struct TextureInfo {
    std::string name;
    glm::u32vec3 size;
    Gpu::Format format;
    Gpu::TextureUsageFlags usage;
    Gpu::SampleCountFlagBits sampleCount{Gpu::SampleCountFlagBits::Samples1Bit};
};

struct TextureState {
    Sync::AccessType lastAccess{Sync::AccessType::None};
    TextureMemoryStatus status{TextureMemoryStatus::Virtual};
};

enum class BufferMemoryStatus { Virtual, Allocated, External };

struct BufferInfo {
    std::string name;
    Gpu::DeviceSize size{};
    Gpu::BufferUsageFlags usage{};
    Gpu::MemoryUsage memoryUsage{Gpu::MemoryUsage::GpuOnly};
};

struct BufferState {
    Sync::AccessType lastAccess{Sync::AccessType::None};
    BufferMemoryStatus status{BufferMemoryStatus::Virtual};
};

using FramegraphTextureHandle = PrivateTypedHandle<TextureInfo, const FramegraphResourceManager>;
using MutableFramegraphTextureHandle = PrivateTypedHandle<TextureInfo, FramegraphResourceManager>;
using FramegraphBufferHandle = PrivateTypedHandle<BufferInfo, const FramegraphResourceManager>;
using MutableFramegraphBufferHandle = PrivateTypedHandle<BufferInfo, FramegraphResourceManager>;
class TransientTextureHandle {
  public:
    TransientTextureHandle() = default;
    /* implicit */ TransientTextureHandle(NullHandle_t null)
        : texture_{null}
    {
    }
    TransientTextureHandle(FramegraphTextureHandle texture)
        : texture_{texture}
        , version_{0} {};

    TransientTextureHandle operator+(uint32_t inc)
    {
        return TransientTextureHandle{texture_, version_ + inc};
    }

    // implicit conversion to the handle it wraps
    operator FramegraphTextureHandle() const { return texture_; }

    FramegraphTextureHandle texture() const { return texture_; }
    uint32_t version() const { return version_; }

    auto operator<=>(const TransientTextureHandle &) const = default;
    explicit operator bool() const { return texture_.valid() && version_ != 0xFFFFFFFF; }

  private:
    TransientTextureHandle(FramegraphTextureHandle texture, uint32_t version)
        : texture_{texture}
        , version_{version} {};
    FramegraphTextureHandle texture_{};
    uint32_t version_{0xFFFFFFFF};
};

} // namespace Cory

/// make TransientTextureHandle hashable
template <> struct std::hash<Cory::TransientTextureHandle> {
    std::size_t operator()(const Cory::TransientTextureHandle &s) const noexcept
    {
        return Cory::hashCompose(s.version(), s.texture());
    }
};
