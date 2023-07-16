#include <Cory/Framegraph/RenderTaskBuilder.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/Framegraph/TextureManager.hpp>

#include <Magnum/Vk/Image.h>

namespace Cory {

// <editor-fold desc="RenderTaskExecutionAwaiter">
RenderInput RenderTaskExecutionAwaiter::await_resume() const noexcept
{
    return fg.renderInput(passHandle);
}

void RenderTaskExecutionAwaiter::await_suspend(
    cppcoro::coroutine_handle<> coroHandle) const noexcept
{
    fg.enqueueRenderPass(passHandle, coroHandle);
}
// </editor-fold>

// <editor-fold desc="RenderTaskBuilder">
RenderTaskBuilder::RenderTaskBuilder(Context &ctx, Framegraph &framegraph, std::string_view taskName)
    : ctx_{ctx}
    , info_{}
    , framegraph_{framegraph}
{
    info_.name = taskName;
    CO_CORE_TRACE("Pass {}: declaration started", info_.name);
}
RenderTaskBuilder::~RenderTaskBuilder() {}

RenderTaskExecutionAwaiter RenderTaskBuilder::finishDeclaration()
{
    const RenderTaskHandle passHandle = framegraph_.finishTaskDeclaration(std::move(info_));
    return RenderTaskExecutionAwaiter{passHandle, framegraph_};
}

TransientTextureHandle RenderTaskBuilder::create(std::string name,
                                       glm::u32vec3 size,
                                       PixelFormat format,
                                       Sync::AccessType writeAccess)
{
    const TextureInfo info{.name = std::move(name), .size = size, .format = format};

    auto handle = TransientTextureHandle{framegraph_.resources().declareTexture(info)};

    info_.dependencies.push_back(RenderTaskInfo::Dependency{
        .kind = TaskDependencyKindBits::CreateWrite,
        .handle = handle,
        .access = writeAccess,
    });
    return handle;
}

TextureInfo RenderTaskBuilder::read(TransientTextureHandle &handle, Sync::AccessType readAccess)
{
    info_.dependencies.push_back(RenderTaskInfo::Dependency{
        .kind = TaskDependencyKindBits::Read, .handle = handle, .access = readAccess});
    return framegraph_.resources().info(handle.texture());
}

std::pair<TransientTextureHandle, TextureInfo>
RenderTaskBuilder::write(TransientTextureHandle handle,
                                                              Sync::AccessType writeAccess)
{
    // increase the version of the texture handle to record the modification
    auto outputHandle = handle + 1;
    info_.dependencies.push_back({
        .kind = TaskDependencyKindBits::Write,
        .handle = outputHandle,
        .access = writeAccess,
    });

    return {outputHandle, framegraph_.resources().info(outputHandle.texture())};
}

std::pair<TransientTextureHandle, TextureInfo>
RenderTaskBuilder::readWrite(TransientTextureHandle handle,
                                                                  Sync::AccessType readWriteAccess)
{
    info_.dependencies.push_back({
        .kind = TaskDependencyKindBits::Read,
        .handle = handle,
        .access = readWriteAccess,
    });

    // increase the version of the texture handle to record the modification
    auto outputHandle = handle + 1;

    info_.dependencies.push_back({
        .kind = TaskDependencyKindBits::ReadWrite,
        .handle = outputHandle,
        .access = readWriteAccess,
    });

    return {outputHandle, framegraph_.resources().info(handle.texture())};
}

TransientRenderPassBuilder RenderTaskBuilder::declareRenderPass(std::string_view name)
{
    return TransientRenderPassBuilder{
        ctx_, name.empty() ? info_.name : name, framegraph_.resources()};
}
// </editor-fold>
} // namespace Cory
