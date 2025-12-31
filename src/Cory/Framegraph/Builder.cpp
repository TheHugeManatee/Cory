#include <Cory/Framegraph/RenderTaskBuilder.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/Framegraph/FramegraphResourceManager.hpp>

namespace Cory {

RenderTaskBuilder::RenderTaskBuilder(Context &ctx,
                                     Framegraph &framegraph,
                                     std::string_view taskName)
    : ctx_{ctx}
    , info_{}
    , framegraph_{framegraph}
{
    info_.name = taskName;
    CO_CORE_TRACE("Pass {}: declaration started", info_.name);
}
RenderTaskBuilder::~RenderTaskBuilder() {}

TransientTextureHandle RenderTaskBuilder::create(std::string name,
                                                 glm::u32vec3 size,
                                                 TextureFormat format,
                                                 Sync::AccessType writeAccess)
{
    const TextureInfo info{.name = std::move(name), .size = size, .format = format};

    auto handle = TransientTextureHandle{framegraph_.resources().declareTexture(info)};

    info_.textureDependencies.push_back(RenderTaskInfo::TextureDependency{
        .kind = TaskDependencyKindBits::CreateWrite,
        .handle = handle,
        .access = writeAccess,
    });
    return handle;
}

TransientBufferHandle RenderTaskBuilder::create(std::string name,
                                                Gpu::DeviceSize size,
                                                Gpu::BufferUsageFlags usage,
                                                Sync::AccessType writeAccess,
                                                Gpu::MemoryUsage memoryUsage)
{
    const BufferInfo info{.name = std::move(name),
                          .size = size,
                          .usage = usage,
                          .memoryUsage = memoryUsage};

    auto handle = TransientBufferHandle{framegraph_.resources().declareBuffer(info)};

    info_.bufferDependencies.push_back(RenderTaskInfo::BufferDependency{
        .kind = TaskDependencyKindBits::CreateWrite,
        .handle = handle,
        .access = writeAccess,
    });
    return handle;
}

TextureInfo RenderTaskBuilder::read(TransientTextureHandle &handle, Sync::AccessType readAccess)
{
    info_.textureDependencies.push_back(RenderTaskInfo::TextureDependency{
        .kind = TaskDependencyKindBits::Read, .handle = handle, .access = readAccess});
    return framegraph_.resources().info(handle.texture());
}

BufferInfo RenderTaskBuilder::read(TransientBufferHandle &handle, Sync::AccessType readAccess)
{
    info_.bufferDependencies.push_back(RenderTaskInfo::BufferDependency{
        .kind = TaskDependencyKindBits::Read, .handle = handle, .access = readAccess});
    return framegraph_.resources().info(handle.buffer());
}

std::pair<TransientTextureHandle, TextureInfo>
RenderTaskBuilder::write(TransientTextureHandle handle, Sync::AccessType writeAccess)
{
    // increase the version of the texture handle to record the modification
    auto outputHandle = handle + 1;
    info_.textureDependencies.push_back({
        .kind = TaskDependencyKindBits::Write,
        .handle = outputHandle,
        .access = writeAccess,
    });

    return {outputHandle, framegraph_.resources().info(outputHandle.texture())};
}

std::pair<TransientBufferHandle, BufferInfo> RenderTaskBuilder::write(TransientBufferHandle handle,
                                                                      Sync::AccessType writeAccess)
{
    auto outputHandle = handle + 1;
    info_.bufferDependencies.push_back({
        .kind = TaskDependencyKindBits::Write,
        .handle = outputHandle,
        .access = writeAccess,
    });

    return {outputHandle, framegraph_.resources().info(outputHandle.buffer())};
}

std::pair<TransientTextureHandle, TextureInfo>
RenderTaskBuilder::readWrite(TransientTextureHandle handle, Sync::AccessType readWriteAccess)
{
    info_.textureDependencies.push_back({
        .kind = TaskDependencyKindBits::Read,
        .handle = handle,
        .access = readWriteAccess,
    });

    // increase the version of the texture handle to record the modification
    auto outputHandle = handle + 1;

    info_.textureDependencies.push_back({
        .kind = TaskDependencyKindBits::ReadWrite,
        .handle = outputHandle,
        .access = readWriteAccess,
    });

    return {outputHandle, framegraph_.resources().info(handle.texture())};
}

std::pair<TransientBufferHandle, BufferInfo>
RenderTaskBuilder::readWrite(TransientBufferHandle handle, Sync::AccessType readWriteAccess)
{
    info_.bufferDependencies.push_back({
        .kind = TaskDependencyKindBits::Read,
        .handle = handle,
        .access = readWriteAccess,
    });

    auto outputHandle = handle + 1;

    info_.bufferDependencies.push_back({
        .kind = TaskDependencyKindBits::ReadWrite,
        .handle = outputHandle,
        .access = readWriteAccess,
    });

    return {outputHandle, framegraph_.resources().info(handle.buffer())};
}

TransientRenderPass RenderTaskBuilder::declareRenderPass(RenderPassDeclaration passDeclaration)
{
    return TransientRenderPass{ctx_, framegraph_.resources(), std::move(passDeclaration)};
}

TransientComputePass RenderTaskBuilder::declareComputePass(ComputePassDeclaration passDeclaration)
{
    return TransientComputePass{ctx_, framegraph_.resources(), std::move(passDeclaration)};
}
} // namespace Cory
