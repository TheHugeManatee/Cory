#pragma once

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
enum class Layout { ColorAttachment, DepthStencilAttachment, TransferSource, PresentSource };
enum class ResourceState { Clear, DontCare, Keep };
using SlotMapHandle = uint64_t;

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

class DependencyGraph {
  public:
    void recordCreates(std::string pass, TextureHandle resource)
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
    void recordReads(std::string pass, TextureHandle resource)
    {
        CO_CORE_DEBUG("Pass {}: Reads '{}' with layout {}", pass, resource.name, resource.layout);
        reads_.emplace_back(pass, resource);
    }
    void recordWrites(std::string pass, TextureHandle resource)
    {
        CO_CORE_DEBUG("Pass {}: Writes '{}' as layout {}", pass, resource.name, resource.layout);
        writes_.emplace_back(pass, resource);
    }

  private:
    using Record = std::tuple<std::string, TextureHandle>;
    std::vector<Record> creates_;
    std::vector<Record> reads_;
    std::vector<Record> writes_;
};

class TextureResourceManager {
  public:
    Texture allocate(std::string name, glm::u32vec3 size, PixelFormat format, Layout layout)
    {
        CO_CORE_DEBUG("Allocating texture '{}' of {}x{}x{} ({} {})",
                      name,
                      size.x,
                      size.y,
                      size.z,
                      format,
                      layout);
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

// a builder that allows a render pass to declare specific dependencies (inputs and outputs)
class Builder {
  public:
    Builder(std::string_view passName,
            DependencyGraph &graph,
            TextureResourceManager &resourceManager)
        : passName_{passName}
        , graph_{graph}
        , resources_{resourceManager}
    {
        CO_CORE_DEBUG("Pass {}: declaration started", passName);
    }
    ~Builder() { CO_CORE_DEBUG("Pass {}: declaration finished", passName_); }

    // declares a dependency to the named resource
    cppcoro::task<TextureHandle> read(TextureHandle &h)
    {
        graph_.recordReads(passName_, h);

        co_return TextureHandle{
            .size = h.size, .format = h.format, .layout = h.layout, .resource = h.resource};
    }

    cppcoro::task<MutableTextureHandle>
    create(std::string name, glm::u32vec3 size, PixelFormat format, Layout finalLayout)
    {
        // coroutine that will allocate and return the texture
        auto do_allocate = [](TextureResourceManager &resources,
                              std::string name,
                              glm::u32vec3 size,
                              PixelFormat format,
                              Layout finalLayout) -> cppcoro::shared_task<Texture> {
            co_return resources.allocate(name, size, format, finalLayout);
        };

        MutableTextureHandle handle{.name = name,
                                    .size = size,
                                    .format = format,
                                    .layout = finalLayout,
                                    .resource =
                                        do_allocate(resources_, name, size, format, finalLayout)};
        graph_.recordCreates(passName_, handle);
        co_return handle;
    }

    cppcoro::task<MutableTextureHandle> write(TextureHandle handle)
    {
        graph_.recordWrites(passName_, handle);
        // todo versioning - MutableTextureHandle should point to a new resource alias (version in
        // slotmap?)
        co_return MutableTextureHandle{.name = handle.name,
                                       .size = handle.size,
                                       .format = handle.format,
                                       .layout = handle.layout,
                                       .resource = handle.resource};
    }

  private:
    std::string passName_;
    DependencyGraph graph_;
    TextureResourceManager &resources_;
};

class Framegraph {
  public:
    void execute(){
        // todo
    };

    Builder declarePass(std::string_view name) { return Builder{name, graph_, resources_}; }

  private:
    void compile()
    {
        // TODO
    }

    DependencyGraph graph_;
    TextureResourceManager resources_;
};

} // namespace Cory::Framegraph
