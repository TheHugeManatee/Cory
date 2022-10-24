#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>

#include <cppcoro/shared_task.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all_ready.hpp>
#include <glm/vec3.hpp>

#include <concepts>
#include <set>
#include <string_view>
#include <unordered_map>

namespace Cory::Framegraph {

using PlaceholderT = uint64_t;
enum class PixelFormat { D32, RGBA32 };
enum class Layout {
    Undefined,
    ColorAttachment,
    DepthStencilAttachment,
    TransferSource,
    PresentSource
};
enum class ResourceState { Clear, DontCare, Keep };
using SlotMapHandle = uint64_t;
using RenderPassHandle = std::string;

struct ResourceHandle {
    SlotMapHandle handle;
};

struct Texture {
    std::string name;
    glm::u32vec3 size;
    PixelFormat format;
    Layout currentLayout;
    PlaceholderT resource; // the Vk::Image
};

struct TextureHandle {
    std::string name;
    glm::u32vec3 size;
    PixelFormat format;
    Layout layout;
    cppcoro::shared_task<Texture> resource;
};

struct MutableTextureHandle {
    std::string name;
    glm::u32vec3 size;
    PixelFormat format;
    Layout layout;
    cppcoro::shared_task<Texture> resource;

    /*implicit*/ operator TextureHandle() const
    {
        return {
            .name = name, .size = size, .format = format, .layout = layout, .resource = resource};
    }
};

struct RenderInput {
    struct RenderPassResources *resources;
    struct RenderContext *context;
};

class Framegraph;

class DependencyGraph {
  public:
    void recordCreates(RenderPassHandle pass, TextureHandle resource)
    {
        CO_CORE_DEBUG("Pass {}: Creates '{}' as {}x{}x{} ({} {})",
                      pass,
                      resource.name,
                      resource.size.x,
                      resource.size.y,
                      resource.size.z,
                      resource.format,
                      resource.layout);
        creates_.emplace_back(pass, resource);
    }
    void recordReads(RenderPassHandle pass, TextureHandle resource)
    {
        CO_CORE_DEBUG("Pass {}: Reads '{}' with layout {}", pass, resource.name, resource.layout);
        reads_.emplace_back(pass, resource);
    }
    void recordWrites(RenderPassHandle pass, TextureHandle resource)
    {
        CO_CORE_DEBUG("Pass {}: Writes '{}' as layout {}", pass, resource.name, resource.layout);
        writes_.emplace_back(pass, resource);
    }

    void summarize() {}

  private:
    using Record = std::tuple<RenderPassHandle, TextureHandle>;
    std::vector<Record> creates_;
    std::vector<Record> reads_;
    std::vector<Record> writes_;
};

class TextureResourceManager {
  public:
    Texture allocate(std::string name, glm::u32vec3 size, PixelFormat format, Layout layout)
    {
        CO_CORE_DEBUG(
            "Allocating '{}' of {}x{}x{} ({} {})", name, size.x, size.y, size.z, format, layout);
        auto handle = nextHandle_++;
        resources_.emplace(handle,
                           Texture{.name = std::move(name),
                                   .size = size,
                                   .format = format,
                                   .currentLayout = layout,
                                   .resource = {}});
        return resources_[handle];
    }

  private:
    SlotMapHandle nextHandle_;
    std::unordered_map<SlotMapHandle, Texture> resources_;
};

template <typename Callable, typename... Arguments>
TextureHandle
performWrite(MutableTextureHandle &handle, Callable &&writeOperation, Arguments... args)
{
    return TextureHandle{.name = handle.name,
                         .size = handle.size,
                         .format = handle.format,
                         .layout = handle.layout,
                         .resource = writeOperation(handle, std::forward<Arguments>(args)...)};
}

template <typename Callable, typename... Arguments>
cppcoro::shared_task<TextureHandle>
performWriteAsync(MutableTextureHandle &handle, Callable &&writeOperation, Arguments... args)
{
    co_return performWrite(
        handle, std::forward<Callable>(writeOperation), std::forward<Arguments>(args)...);
}

struct RenderPassExecutionAwaiter {
    RenderPassHandle passHandle;
    Framegraph &fg;
    constexpr bool await_ready() const noexcept { return false; }
    RenderInput await_resume() const noexcept;
    void await_suspend(cppcoro::coroutine_handle<> coroHandle) const noexcept;
};

// a builder that allows a render pass to declare specific dependencies (inputs and outputs)
class Builder {
  public:
    Builder(Framegraph &framegraph, std::string_view passName)
        : passHandle_{passName}
        , framegraph_{framegraph}
    {
        CO_CORE_TRACE("Pass {}: declaration started", passName);
    }
    ~Builder() { CO_CORE_TRACE("Pass {}: Builder destroyed", passHandle_); }

    // declares a dependency to the named resource
    cppcoro::task<TextureHandle> read(TextureHandle &h);

    cppcoro::task<MutableTextureHandle>
    create(std::string name, glm::u32vec3 size, PixelFormat format, Layout finalLayout);

    cppcoro::task<MutableTextureHandle> write(TextureHandle handle);

    RenderPassHandle passHandle() const noexcept { return passHandle_; }

    RenderPassExecutionAwaiter finishDeclaration();

  private:
    RenderPassHandle passHandle_;
    Framegraph &framegraph_;
};

class Framegraph : NoCopy, NoMove {
  public:
    friend Builder;
    friend RenderPassExecutionAwaiter;

    ~Framegraph()
    {
        // destroy all coroutines
        for (auto &[name, handle] : renderPasses_) {
            handle.destroy();
        }
    }

    void execute()
    {
        for (const auto &[name, handle] : renderPasses_) {
            CO_CORE_INFO("Executing rendering commands for {}", name);
            if (!handle.done()) { handle.resume(); }
            CO_CORE_ASSERT(
                handle.done(),
                "Render pass definition seems to have more unnecessary synchronization points!");
        }
    };

    Builder declarePass(std::string_view name);

    // to be called from Builder
    RenderInput renderInput() { return {}; }

  private:
    // to be called from Builder
    void enqueueRenderPass(RenderPassHandle passHandle, cppcoro::coroutine_handle<> coroHandle)
    {
        renderPasses_.insert({std::move(passHandle), coroHandle});
    }

    void compile()
    {
        // TODO
    }

    DependencyGraph graph_;
    TextureResourceManager resources_;

    std::unordered_map<RenderPassHandle, cppcoro::coroutine_handle<>> renderPasses_;
};

} // namespace Cory::Framegraph
