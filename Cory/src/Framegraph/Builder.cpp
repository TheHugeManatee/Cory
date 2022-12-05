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
                                       Sync::AccessType writeAccess)
{
    const TextureInfo info{.name = std::move(name), .size = size, .format = format};

    auto handle = TransientTextureHandle{.texture = framegraph_.resources_.declareTexture(info),
                                         .version = 0};

    info_.outputs.push_back(RenderTaskInfo::Dependency{
        .kind = TaskDependencyKindBits::CreateWrite,
        .handle = handle,
        .access = writeAccess,
    });
    return handle;
}

TextureInfo Builder::read(TransientTextureHandle &handle, Sync::AccessType readAccess)
{
    info_.inputs.push_back(RenderTaskInfo::Dependency{
        .kind = TaskDependencyKindBits::Read, .handle = handle, .access = readAccess});
    return framegraph_.resources_.info(handle.texture);
}

std::pair<TransientTextureHandle, TextureInfo> Builder::write(TransientTextureHandle handle,
                                                              Sync::AccessType writeAccess)
{
    // increase the version of the texture handle to record the modification
    auto outputHandle =
        TransientTextureHandle{.texture = handle.texture, .version = handle.version + 1};
    info_.outputs.push_back({
        .kind = TaskDependencyKindBits::Write,
        .handle = outputHandle,
        .access = writeAccess,
    });

    return {outputHandle, framegraph_.resources_.info(handle.texture)};
}

std::pair<TransientTextureHandle, TextureInfo> Builder::readWrite(TransientTextureHandle handle,
                                                                  Sync::AccessType readWriteAccess)
{
    info_.inputs.push_back({
        .kind = TaskDependencyKindBits::Read,
        .handle = handle,
        .access = readWriteAccess,
    });

    // increase the version of the texture handle to record the modification
    auto outputHandle =
        TransientTextureHandle{.texture = handle.texture, .version = handle.version + 1};

    info_.outputs.push_back({
        .kind = TaskDependencyKindBits::Write,
        .handle = outputHandle,
        .access = readWriteAccess,
    });

    return {outputHandle, framegraph_.resources_.info(handle.texture)};
}

TransientRenderPassBuilder Builder::declareRenderPass(std::string_view name)
{
    return TransientRenderPassBuilder{
        *framegraph_.ctx_, name.empty() ? info_.name : name, framegraph_.resources_};
}

} // namespace Cory::Framegraph
