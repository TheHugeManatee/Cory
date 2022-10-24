#include <Cory/Framegraph/Framegraph.hpp>

namespace Cory::Framegraph {

Builder Framegraph::Framegraph::declarePass(std::string_view name)
{
    //
    return Builder{*this, name};
}

Framegraph::~Framegraph()
{
    // destroy all coroutines
    for (auto &[name, handle] : renderPasses_) {
        handle.destroy();
    }
}

void Framegraph::execute()
{
    compile();

    for (const auto &[name, handle] : renderPasses_) {
        CO_CORE_INFO("Executing rendering commands for {}", name);
        if (!handle.done()) { handle.resume(); }
        CO_CORE_ASSERT(
            handle.done(),
            "Render pass definition seems to have more unnecessary synchronization points!");
    }
}
TextureHandle Framegraph::declareInput(std::string_view name)
{
    TextureHandle handle{
        .name = std::string{name},
        // todo fill these with more parameters?
        //  also create an external resource in the resource manager?
    };
    graph_.recordCreates("[ExternalInput]", handle);
    return handle;
}

void Framegraph::declareOutput(TextureHandle handle)
{
    graph_.recordReads("[ExternalOut]", handle);
    // todo
}

cppcoro::task<TextureHandle> Builder::read(TextureHandle &h)
{
    framegraph_.graph_.recordReads(passHandle_, h);

    co_return TextureHandle{
        .size = h.size, .format = h.format, .layout = h.layout, .resource = h.resource};
}

cppcoro::task<MutableTextureHandle>
Builder::create(std::string name, glm::u32vec3 size, PixelFormat format, Layout finalLayout)
{
    // coroutine that will allocate and return the texture
    auto do_allocate = [](TextureResourceManager &resources,
                          std::string name,
                          glm::u32vec3 size,
                          PixelFormat format,
                          Layout finalLayout) -> cppcoro::shared_task<Texture> {
        co_return resources.allocate(name, size, format, finalLayout);
    };

    MutableTextureHandle handle{
        .name = name,
        .size = size,
        .format = format,
        .layout = finalLayout,
        .resource = do_allocate(framegraph_.resources_, name, size, format, finalLayout)};
    framegraph_.graph_.recordCreates(passHandle_, handle);
    co_return handle;
}

cppcoro::task<MutableTextureHandle> Builder::write(TextureHandle handle)
{
    framegraph_.graph_.recordWrites(passHandle_, handle);
    // todo versioning - MutableTextureHandle should point to a new resource alias (version in
    // slotmap?)
    co_return MutableTextureHandle{.name = handle.name,
                                   .size = handle.size,
                                   .format = handle.format,
                                   .layout = handle.layout,
                                   .resource = handle.resource};
}

RenderPassExecutionAwaiter Builder::finishDeclaration()
{
    return RenderPassExecutionAwaiter{passHandle_, framegraph_};
}

RenderInput RenderPassExecutionAwaiter::await_resume() const noexcept { return fg.renderInput(); }

void RenderPassExecutionAwaiter::await_suspend(
    cppcoro::coroutine_handle<> coroHandle) const noexcept
{
    fg.enqueueRenderPass(passHandle, coroHandle);
}

void DependencyGraph::dump()
{
    std::string out{"digraph G {\n"};

    std::set<RenderPassHandle> passes;
    std::set<std::string> textures;

    for (const auto &create : creates_) {
        fmt::format_to(std::back_inserter(out),
                       "  \"{}\" -> \"{}\" [style=dashed,color=darkgreen,label=\"create\"]\n",
                       create.pass,
                       create.texture.name);
        passes.insert(create.pass);
        textures.insert(create.texture.name);
    }
    for (const auto &read : reads_) {
        fmt::format_to(std::back_inserter(out),
                       "  \"{}\" -> \"{}\" [label=\"read\"]\n",
                       read.texture.name,
                       read.pass);
        passes.insert(read.pass);
        textures.insert(read.texture.name);
    }
    for (const auto &write : writes_) {
        fmt::format_to(std::back_inserter(out),
                       "  \"{}\" -> \"{}\" [label=\"write\"]\n",
                       write.pass,
                       write.texture.name);
        passes.insert(write.pass);
        textures.insert(write.texture.name);
    }
    for (const auto &pass : passes) {
        fmt::format_to(std::back_inserter(out), "  \"{}\" [shape=ellipse]\n", pass);
    }
    for (const auto &texture : textures) {
        fmt::format_to(std::back_inserter(out), "  \"{}\" [shape=rectangle]\n", texture);
    }

    out += "}\n";
    CO_CORE_INFO(out);
}
} // namespace Cory::Framegraph