#pragma once

#include <Cory/Base/SlotMap.hpp>

#include <cppcoro/task.hpp>
#include <cppcoro/when_all_ready.hpp>
#include <glm/vec3.hpp>

#include <concepts>
#include <string_view>

namespace Cory::Framegraph {

using PlaceholderT = uint64_t;

struct ResourceHandle {
    SlotMapHandle handle;
};

struct Texture {
    glm::u32vec3 size;
    PlaceholderT format;
    PlaceholderT currentLayout;
    PlaceholderT resource; // the Vk::Image
};

struct TextureHandle {
    glm::u32vec3 size;
    PlaceholderT format;
    PlaceholderT currentLayout;

    SlotMapHandle handle;
};
struct MutableTextureHandle {
    glm::u32vec3 size;
    PlaceholderT format;
    PlaceholderT currentLayout;

    SlotMapHandle handle;

    /*implicit*/ operator TextureHandle() const
    {
        return {.size = size, .format = format, .currentLayout = currentLayout, .handle = handle};
    }
};

// base template
template <typename ResourceHandle> struct ResourcePtrHelper {};
template <typename ResourceHandle>
    requires std::is_same_v<ResourceHandle, TextureHandle>
struct ResourcePtrHelper<ResourceHandle> {
    using Type = Texture &;
};
template <typename ResourceHandle>
    requires std::is_same_v<ResourceHandle, MutableTextureHandle>
struct ResourcePtrHelper<ResourceHandle> {
    using Type = const Texture &;
};
template <typename ResourceHandle>
using ResourcePtr = typename ResourcePtrHelper<ResourceHandle>::Type;

class Builder {
  public:
    cppcoro::task<TextureHandle> read(std::string_view name /**/, TextureHandle handle)
    {
        const auto &t = resources_[handle.handle];
        co_return TextureHandle{
            .size = t.size, .format = t.format, .currentLayout = t.format, .handle = handle.handle};
    }
    cppcoro::task<MutableTextureHandle>
    create(std::string_view name, glm::u32vec3 size, PlaceholderT format, PlaceholderT finalLayout)
    {
        // todo actually create a resource
        auto handle = resources_.insert(
            {.size = size, .format = format, .currentLayout = finalLayout, .resource = {}});
        co_return MutableTextureHandle{
            .size = size, .format = format, .currentLayout = finalLayout, .handle = handle};
    }
    cppcoro::task<MutableTextureHandle> write(std::string_view /**/, ResourceHandle handle)
    {
        auto &t = resources_[handle.handle];

        auto updated_handle = resources_.update(handle.handle,
                                                {.size = t.size,
                                                 .format = t.format,
                                                 .currentLayout = t.currentLayout,
                                                 .resource = t.resource});
        co_return MutableTextureHandle{.size = t.size,
                                       .format = t.format,
                                       .currentLayout = t.currentLayout,
                                       .handle = updated_handle};
    }

    template <typename... Handles> auto acquireResources(Handles... handles)
    {
        return cppcoro::when_all_ready(acquireResource(handles)...);
    }

  private:
    cppcoro::task<const Texture &> acquireResource(const TextureHandle &h)
    {
        co_return resources_[h.handle];
    }
    cppcoro::task<Texture &> acquireResource(const MutableTextureHandle &h)
    {
        co_return resources_[h.handle];
    }

    SlotMap<Texture> resources_;
};

class Framegraph {
  public:
    template <typename Functor>
        requires(std::invocable<Functor, Builder &>)
    cppcoro::task<void> build(Functor &&functor)
    {
        co_await functor(builder_);
        compile();
        co_return;
    }
    void execute();

    Builder &declarePass(std::string_view name);

  private:
    void compile();

    Builder builder_;
};

} // namespace Cory::Framegraph
