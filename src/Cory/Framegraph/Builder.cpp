#include <Cory/Framegraph/RenderTaskBuilder.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/Framegraph/FramegraphResourceManager.hpp>

#include <optional>

namespace Cory {
namespace {
std::optional<TransientTextureHandle>
findWriteDependency(const std::vector<RenderTaskInfo::TextureDependency> &dependencies,
                    FramegraphTextureHandle texture)
{
    for (const auto &dependency : dependencies) {
        const auto kind = dependency.kind;
        if (dependency.handle.texture() != texture) continue;
        if (kind.is_set(TaskDependencyKindBits::Write) ||
            kind.is_set(TaskDependencyKindBits::ReadWrite) ||
            kind.is_set(TaskDependencyKindBits::CreateWrite)) {
            return dependency.handle;
        }
    }
    return std::nullopt;
}

bool shouldReadAttachment(Gpu::AttachmentLoadOperation loadOp)
{
    return loadOp == Gpu::AttachmentLoadOperation::Load;
}
} // namespace

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
                                                 Gpu::TextureUsageFlags usage,
                                                 Sync::AccessType writeAccess)
{
    const TextureInfo info{.name = std::move(name), .size = size, .format = format};

    auto handle = TransientTextureHandle{framegraph_.resources().declareTexture(info)};

    info_.textureDependencies.push_back(RenderTaskInfo::TextureDependency{
        .kind = TaskDependencyKindBits::CreateWrite,
        .handle = handle,
        .usage = usage,
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
                          .memoryUsage = memoryUsage};

    auto handle = TransientBufferHandle{framegraph_.resources().declareBuffer(info)};

    info_.bufferDependencies.push_back(RenderTaskInfo::BufferDependency{
        .kind = TaskDependencyKindBits::CreateWrite,
        .handle = handle,
        .usage = usage,
        .access = writeAccess,
    });
    return handle;
}

TextureInfo RenderTaskBuilder::read(TransientTextureHandle handle,
                                    Gpu::TextureUsageFlags usage,
                                    Sync::AccessType readAccess)
{
    info_.textureDependencies.push_back(RenderTaskInfo::TextureDependency{
        .kind = TaskDependencyKindBits::Read,
        .handle = handle,
        .usage = usage,
        .access = readAccess});
    return framegraph_.resources().info(handle.texture());
}

BufferInfo RenderTaskBuilder::read(TransientBufferHandle handle,
                                   Gpu::BufferUsageFlags usage,
                                   Sync::AccessType readAccess)
{
    info_.bufferDependencies.push_back(RenderTaskInfo::BufferDependency{
        .kind = TaskDependencyKindBits::Read,
        .handle = handle,
        .usage = usage,
        .access = readAccess});
    return framegraph_.resources().info(handle.buffer());
}

std::pair<TransientTextureHandle, TextureInfo>
RenderTaskBuilder::write(TransientTextureHandle handle,
                         Gpu::TextureUsageFlags usage,
                         Sync::AccessType writeAccess)
{
    // increase the version of the texture handle to record the modification
    auto outputHandle = handle + 1;
    info_.textureDependencies.push_back({
        .kind = TaskDependencyKindBits::Write,
        .handle = outputHandle,
        .usage = usage,
        .access = writeAccess,
    });

    return {outputHandle, framegraph_.resources().info(outputHandle.texture())};
}

std::pair<TransientBufferHandle, BufferInfo>
RenderTaskBuilder::write(TransientBufferHandle handle,
                         Gpu::BufferUsageFlags usage,
                         Sync::AccessType writeAccess)
{
    auto outputHandle = handle + 1;
    info_.bufferDependencies.push_back({
        .kind = TaskDependencyKindBits::Write,
        .handle = outputHandle,
        .usage = usage,
        .access = writeAccess,
    });

    return {outputHandle, framegraph_.resources().info(outputHandle.buffer())};
}

std::pair<TransientTextureHandle, TextureInfo>
RenderTaskBuilder::readWrite(TransientTextureHandle handle,
                             Gpu::TextureUsageFlags usage,
                             Sync::AccessType readWriteAccess)
{
    info_.textureDependencies.push_back({
        .kind = TaskDependencyKindBits::Read,
        .handle = handle,
        .usage = usage,
        .access = readWriteAccess,
    });

    // increase the version of the texture handle to record the modification
    auto outputHandle = handle + 1;

    info_.textureDependencies.push_back({
        .kind = TaskDependencyKindBits::ReadWrite,
        .handle = outputHandle,
        .usage = usage,
        .access = readWriteAccess,
    });

    return {outputHandle, framegraph_.resources().info(handle.texture())};
}

std::pair<TransientBufferHandle, BufferInfo>
RenderTaskBuilder::readWrite(TransientBufferHandle handle,
                             Gpu::BufferUsageFlags usage,
                             Sync::AccessType readWriteAccess)
{
    info_.bufferDependencies.push_back({
        .kind = TaskDependencyKindBits::Read,
        .handle = handle,
        .usage = usage,
        .access = readWriteAccess,
    });

    auto outputHandle = handle + 1;

    info_.bufferDependencies.push_back({
        .kind = TaskDependencyKindBits::ReadWrite,
        .handle = outputHandle,
        .usage = usage,
        .access = readWriteAccess,
    });

    return {outputHandle, framegraph_.resources().info(handle.buffer())};
}

TransientRenderPass RenderTaskBuilder::declareRenderPass(RenderPassDeclaration passDeclaration)
{
    return TransientRenderPass{ctx_, framegraph_.resources(), std::move(passDeclaration)};
}

RenderTaskBuilder::DeclaredRenderPass
RenderTaskBuilder::declareRenderPassWithOutputs(RenderPassDeclaration passDeclaration)
{
    DeclaredRenderPass declared{
        .pass = TransientRenderPass{ctx_, framegraph_.resources(), RenderPassDeclaration{}},
    };

    declared.colorOutputs.reserve(passDeclaration.attachments.size());
    for (auto &attachment : passDeclaration.attachments) {
        const auto existing =
            findWriteDependency(info_.textureDependencies, attachment.target.texture());
        TransientTextureHandle outputHandle{};
        if (existing.has_value()) {
            outputHandle = *existing;
        }
        else if (shouldReadAttachment(attachment.load)) {
            outputHandle =
                readWrite(attachment.target,
                          Gpu::TextureUsageFlagBits::ColorAttachmentBit,
                          Sync::AccessType::ColorAttachmentReadWrite)
                    .first;
        }
        else {
            outputHandle =
                write(attachment.target,
                      Gpu::TextureUsageFlagBits::ColorAttachmentBit,
                      Sync::AccessType::ColorAttachmentWrite)
                    .first;
        }
        attachment.target = outputHandle;
        declared.colorOutputs.push_back(outputHandle);
    }

    if (passDeclaration.depthAttachment.has_value()) {
        auto &depthAttachment = passDeclaration.depthAttachment.value();
        const auto existing =
            findWriteDependency(info_.textureDependencies, depthAttachment.target.texture());
        TransientTextureHandle outputHandle{};
        if (existing.has_value()) {
            outputHandle = *existing;
        }
        else if (shouldReadAttachment(depthAttachment.load)) {
            outputHandle =
                readWrite(depthAttachment.target,
                          Gpu::TextureUsageFlagBits::DepthStencilAttachmentBit,
                          Sync::AccessType::DepthStencilAttachmentReadWrite)
                    .first;
        }
        else {
            outputHandle =
                write(depthAttachment.target,
                      Gpu::TextureUsageFlagBits::DepthStencilAttachmentBit,
                      Sync::AccessType::DepthStencilAttachmentWrite)
                    .first;
        }
        depthAttachment.target = outputHandle;
        declared.depthOutput = outputHandle;
    }

    if (passDeclaration.stencilAttachment.has_value()) {
        auto &stencilAttachment = passDeclaration.stencilAttachment.value();
        const auto existing =
            findWriteDependency(info_.textureDependencies, stencilAttachment.target.texture());
        TransientTextureHandle outputHandle{};
        if (existing.has_value()) {
            outputHandle = *existing;
        }
        else if (shouldReadAttachment(stencilAttachment.load)) {
            outputHandle =
                readWrite(stencilAttachment.target,
                          Gpu::TextureUsageFlagBits::DepthStencilAttachmentBit,
                          Sync::AccessType::DepthStencilAttachmentReadWrite)
                    .first;
        }
        else {
            outputHandle =
                write(stencilAttachment.target,
                      Gpu::TextureUsageFlagBits::DepthStencilAttachmentBit,
                      Sync::AccessType::DepthStencilAttachmentWrite)
                    .first;
        }
        stencilAttachment.target = outputHandle;
        declared.stencilOutput = outputHandle;
    }

    declared.pass = TransientRenderPass{ctx_, framegraph_.resources(), std::move(passDeclaration)};
    return declared;
}

TransientComputePass RenderTaskBuilder::declareComputePass(ComputePassDeclaration passDeclaration)
{
    return TransientComputePass{ctx_, framegraph_.resources(), std::move(passDeclaration)};
}

RenderTaskBuilder RenderTaskBuilder::subtask(std::string_view name) const
{
    auto subtask_name = fmt::format("{}::{}", info_.name, name);
    return {ctx_, framegraph_, subtask_name};
}

const TextureInfo &RenderTaskBuilder::textureInfo(TransientTextureHandle handle) const
{
    return framegraph_.resources().info(handle.texture());
}

const BufferInfo &RenderTaskBuilder::bufferInfo(TransientBufferHandle handle) const
{
    return framegraph_.resources().info(handle.buffer());
}
} // namespace Cory
