#include <Cory/Framegraph/Framegraph.hpp>

namespace Cory::Framegraph {

Builder Framegraph::Framegraph::declarePass(std::string_view name) { return Builder{*this, name}; }

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

} // namespace Cory::Framegraph