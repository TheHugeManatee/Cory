#include <Cory/Framegraph/Builder.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/Framegraph/TextureManager.hpp>

namespace Cory::Framegraph {

Builder::Builder(Framegraph &framegraph, std::string_view passName)
    : info_{}
    , framegraph_{framegraph}
{
    info_.name = passName;
    CO_CORE_TRACE("Pass {}: declaration started", info_.name);
}
Builder::~Builder() { CO_CORE_TRACE("Pass {}: Builder destroyed", info_.name); }

cppcoro::task<TextureHandle> Builder::read(TextureHandle &handle)
{
    info_.inputs.push_back(handle);

    co_return TextureHandle{.size = handle.size,
                            .format = handle.format,
                            .layout = handle.layout,
                            .rsrcHandle = handle.rsrcHandle};
}

cppcoro::task<MutableTextureHandle>
Builder::create(std::string name, glm::u32vec3 size, PixelFormat format, Layout finalLayout)
{
    MutableTextureHandle handle{
        .name = name,
        .size = size,
        .format = format,
        .layout = finalLayout,
        .rsrcHandle = framegraph_.resources_.createTexture(name, size, format, finalLayout)};
    info_.outputs.push_back({handle, PassOutputKind::Create});
    co_return handle;
}

cppcoro::task<MutableTextureHandle> Builder::write(TextureHandle handle)
{
    // framegraph_.graph_.recordWrites(passHandle_, handle);
    info_.outputs.push_back({handle, PassOutputKind::Write});
    // todo versioning - MutableTextureHandle should point to a new resource alias (version in
    // slotmap?)
    co_return MutableTextureHandle{.name = handle.name,
                                   .size = handle.size,
                                   .format = handle.format,
                                   .layout = handle.layout,
                                   .rsrcHandle = handle.rsrcHandle};
}

RenderPassExecutionAwaiter Builder::finishDeclaration()
{
    RenderPassHandle passHandle = framegraph_.finishPassDeclaration(std::move(info_));
    return RenderPassExecutionAwaiter{passHandle, framegraph_};
}

} // namespace Cory::Framegraph
