#include <Cory/Framegraph/Builder.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/Framegraph/Framegraph.hpp>

#include <Magnum/Vk/Image.h>

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
    const RenderTaskHandle passHandle = framegraph_.finishPassDeclaration(std::move(info_));
    return RenderTaskExecutionAwaiter{passHandle, framegraph_};
}

TransientTextureHandle Builder::create(std::string name,
                                       glm::u32vec3 size,
                                       PixelFormat format,
                                       Layout initialLayout,
                                       PipelineStages writeStage,
                                       AccessFlags writeAccess)
{
    const TextureInfo info{.name = std::move(name), .size = size, .format = format};

    // by default, access all aspects for the format
    auto aspects = VkImageAspectFlags(Vk::imageAspectsFor(format));
    const TextureAccessInfo accessInfo{.layout = initialLayout,
                                       .stage = writeStage,
                                       .access = writeAccess,
                                       .imageAspect = aspects};

    auto handle = TransientTextureHandle{.texture = framegraph_.resources_.declareTexture(info),
                                         .version = 0};

    info_.outputs.push_back(
        {.handle = handle, .kind = TaskOutputKind::Create, .accessInfo = accessInfo});
    return handle;
}

TextureInfo Builder::read(TransientTextureHandle &handle, TextureAccessInfo readAccess)
{
    info_.inputs.push_back({.handle = handle, .accessInfo = readAccess});
    return framegraph_.resources_.info(handle.texture);
}

std::pair<TransientTextureHandle, TextureInfo> Builder::write(TransientTextureHandle handle,
                                                              TextureAccessInfo writeAccess)
{
    // increase the version of the texture handle to record the modification
    auto outputHandle =
        TransientTextureHandle{.texture = handle.texture, .version = handle.version + 1};
    info_.outputs.push_back({
        .handle = outputHandle,
        .kind = TaskOutputKind::Write,
        .accessInfo = writeAccess,
    });

    return {outputHandle, framegraph_.resources_.info(handle.texture)};
}

std::pair<TransientTextureHandle, TextureInfo> Builder::readWrite(TransientTextureHandle handle,
                                                                  TextureAccessInfo readAccess,
                                                                  TextureAccessInfo writeAccess)
{
    info_.inputs.push_back({.handle = handle, .accessInfo = readAccess});
    auto outputHandle =
        TransientTextureHandle{.texture = handle.texture, .version = handle.version + 1};
    info_.outputs.push_back({
        .handle = outputHandle,
        .kind = TaskOutputKind::Write,
        .accessInfo = writeAccess,
    });

    // increase the version of the texture handle to record the modification
    return {outputHandle, framegraph_.resources_.info(handle.texture)};
}

TransientRenderPassBuilder Builder::declareRenderPass(std::string_view name)
{
    return TransientRenderPassBuilder{
        *framegraph_.ctx_, name.empty() ? info_.name : name, framegraph_.resources_};
}

} // namespace Cory::Framegraph
