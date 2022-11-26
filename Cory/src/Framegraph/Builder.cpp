#include <Cory/Framegraph/Builder.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/Framegraph/Framegraph.hpp>

namespace Vk = Magnum::Vk;

namespace Cory::Framegraph {

Builder::Builder(Framegraph &framegraph, std::string_view passName)
    : info_{}
    , framegraph_{framegraph}
{
    info_.name = passName;
    CO_CORE_TRACE("Pass {}: declaration started", info_.name);
}
Builder::~Builder() { CO_CORE_TRACE("Pass {}: Builder destroyed", info_.name); }

RenderTaskExecutionAwaiter Builder::finishDeclaration()
{
    RenderTaskHandle passHandle = framegraph_.finishPassDeclaration(std::move(info_));
    return RenderTaskExecutionAwaiter{passHandle, framegraph_};
}

TextureHandle Builder::create(std::string name,
                              glm::u32vec3 size,
                              PixelFormat format,
                              Layout initialLayout,
                              PipelineStages writeStage,
                              AccessFlags writeAccess)
{
    TextureInfo info{.name = std::move(name), .size = size, .format = format};
    TextureAccessInfo accessInfo{.layout = initialLayout,
                                 .access = writeAccess,
                                 .stage = writeStage,
                                 .imageAspect = VK_IMAGE_ASPECT_COLOR_BIT};

    auto handle = framegraph_.resources_.declareTexture(info);
    info_.outputs.push_back(
        {.handle = handle, .kind = PassOutputKind::Create, .accessInfo = accessInfo});
    return handle;
}

TextureInfo Builder::read(TextureHandle &handle, TextureAccessInfo readAccess)
{
    info_.inputs.push_back({.handle = handle, .accessInfo = readAccess});
    return framegraph_.resources_.info(handle);
}

std::pair<TextureHandle, TextureInfo> Builder::write(TextureHandle handle,
                                                     TextureAccessInfo writeAccess)
{
    CO_CORE_ASSERT(framegraph_.resources_.state(handle).status != TextureMemoryStatus::External,
                   "External textures cannot be written to!");

    info_.outputs.push_back({
        .handle = handle,
        .kind = PassOutputKind::Write,
        .accessInfo = writeAccess,
    });
    // TODO versioning of the handles?
    return {handle, framegraph_.resources_.info(handle)};
}

std::pair<TextureHandle, TextureInfo> Builder::readWrite(TextureHandle handle,
                                                         TextureAccessInfo readAccess,
                                                         TextureAccessInfo writeAccess)
{
    CO_CORE_ASSERT(framegraph_.resources_.state(handle).status != TextureMemoryStatus::External,
                   "External textures cannot be written to!");

    info_.inputs.push_back({.handle = handle, .accessInfo = readAccess});
    info_.outputs.push_back({
        .handle = handle,
        .kind = PassOutputKind::Write,
        .accessInfo = writeAccess,
    });
    // TODO versioning of the handles?
    return {handle, framegraph_.resources_.info(handle)};
}

TransientRenderPassBuilder Builder::declareRenderPass(std::string_view name)
{
    return TransientRenderPassBuilder(
        *framegraph_.ctx_, name.empty() ? info_.name : name, framegraph_.resources_);
}

} // namespace Cory::Framegraph
