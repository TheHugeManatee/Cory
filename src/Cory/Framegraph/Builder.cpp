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

struct TextureAccessPreset {
    Gpu::TextureUsageFlags usage;
    Sync::AccessType access;
};

struct BufferAccessPreset {
    Gpu::BufferUsageFlags usage;
    Sync::AccessType access;
};

TextureAccessPreset toTextureReadPreset(RenderTaskBuilder::TextureReadPreset preset)
{
    switch (preset) {
    case RenderTaskBuilder::TextureReadPreset::TransferSrc:
        return {
            .usage = Gpu::TextureUsageFlagBits::TransferSrcBit,
            .access = Sync::AccessType::TransferRead,
        };
    case RenderTaskBuilder::TextureReadPreset::ComputeSampled:
        return {
            .usage = Gpu::TextureUsageFlagBits::SampledBit,
            .access = Sync::AccessType::ComputeShaderReadOther,
        };
    case RenderTaskBuilder::TextureReadPreset::FragmentSampled:
        return {
            .usage = Gpu::TextureUsageFlagBits::SampledBit,
            .access = Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer,
        };
    }
    CO_CORE_ASSERT(false, "Unhandled texture read preset {}", static_cast<int>(preset));
}

TextureAccessPreset toTextureWritePreset(RenderTaskBuilder::TextureWritePreset preset)
{
    switch (preset) {
    case RenderTaskBuilder::TextureWritePreset::TransferDst:
        return {
            .usage = Gpu::TextureUsageFlagBits::TransferDstBit,
            .access = Sync::AccessType::TransferWrite,
        };
    case RenderTaskBuilder::TextureWritePreset::ColorAttachment:
        return {
            .usage = Gpu::TextureUsageFlagBits::ColorAttachmentBit,
            .access = Sync::AccessType::ColorAttachmentWrite,
        };
    case RenderTaskBuilder::TextureWritePreset::DepthStencilAttachment:
        return {
            .usage = Gpu::TextureUsageFlagBits::DepthStencilAttachmentBit,
            .access = Sync::AccessType::DepthStencilAttachmentWrite,
        };
    case RenderTaskBuilder::TextureWritePreset::ComputeStorage:
        return {
            .usage = Gpu::TextureUsageFlagBits::StorageBit,
            .access = Sync::AccessType::ComputeShaderWrite,
        };
    }
    CO_CORE_ASSERT(false, "Unhandled texture write preset {}", static_cast<int>(preset));
}

TextureAccessPreset toTextureReadWritePreset(RenderTaskBuilder::TextureReadWritePreset preset)
{
    switch (preset) {
    case RenderTaskBuilder::TextureReadWritePreset::GeneralStorage:
        return {
            .usage = Gpu::TextureUsageFlagBits::StorageBit,
            .access = Sync::AccessType::General,
        };
    case RenderTaskBuilder::TextureReadWritePreset::ColorAttachment:
        return {
            .usage = Gpu::TextureUsageFlagBits::ColorAttachmentBit,
            .access = Sync::AccessType::ColorAttachmentReadWrite,
        };
    case RenderTaskBuilder::TextureReadWritePreset::DepthStencilAttachment:
        return {
            .usage = Gpu::TextureUsageFlagBits::DepthStencilAttachmentBit,
            .access = Sync::AccessType::DepthStencilAttachmentReadWrite,
        };
    }
    CO_CORE_ASSERT(false, "Unhandled texture readWrite preset {}", static_cast<int>(preset));
}

BufferAccessPreset toBufferReadPreset(RenderTaskBuilder::BufferReadPreset preset)
{
    switch (preset) {
    case RenderTaskBuilder::BufferReadPreset::ComputeStorage:
        return {
            .usage = Gpu::BufferUsageFlagBits::StorageBufferBit,
            .access = Sync::AccessType::ComputeShaderReadOther,
        };
    case RenderTaskBuilder::BufferReadPreset::VertexStorage:
        return {
            .usage = Gpu::BufferUsageFlagBits::StorageBufferBit,
            .access = Sync::AccessType::VertexShaderReadOther,
        };
    }
    CO_CORE_ASSERT(false, "Unhandled buffer read preset {}", static_cast<int>(preset));
}

BufferAccessPreset toBufferWritePreset(RenderTaskBuilder::BufferWritePreset preset)
{
    switch (preset) {
    case RenderTaskBuilder::BufferWritePreset::ComputeStorage:
        return {
            .usage = Gpu::BufferUsageFlagBits::StorageBufferBit |
                     Gpu::BufferUsageFlagBits::ShaderDeviceAddressBit,
            .access = Sync::AccessType::ComputeShaderWrite,
        };
    }
    CO_CORE_ASSERT(false, "Unhandled buffer write preset {}", static_cast<int>(preset));
}

BufferAccessPreset toBufferReadWritePreset(RenderTaskBuilder::BufferReadWritePreset preset)
{
    switch (preset) {
    case RenderTaskBuilder::BufferReadWritePreset::ComputeStorage:
        return {
            .usage = Gpu::BufferUsageFlagBits::StorageBufferBit |
                     Gpu::BufferUsageFlagBits::ShaderDeviceAddressBit,
            .access = Sync::AccessType::General,
        };
    }
    CO_CORE_ASSERT(false, "Unhandled buffer readWrite preset {}", static_cast<int>(preset));
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
                                                 Sync::AccessType writeAccess,
                                                 Gpu::TextureType textureType)
{
    const TextureInfo info{.name = std::move(name),
                           .size = size,
                           .format = format,
                           .usage = usage,
                           .textureType = textureType};

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
    const BufferInfo info{.name = std::move(name), .size = size, .memoryUsage = memoryUsage};

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
    info_.textureDependencies.push_back(
        RenderTaskInfo::TextureDependency{.kind = TaskDependencyKindBits::Read,
                                          .handle = handle,
                                          .usage = usage,
                                          .access = readAccess});
    return framegraph_.resources().info(handle.texture());
}

TextureInfo RenderTaskBuilder::read(TransientTextureHandle handle, TextureReadPreset preset)
{
    const auto p = toTextureReadPreset(preset);
    return read(handle, p.usage, p.access);
}

BufferInfo RenderTaskBuilder::read(TransientBufferHandle handle,
                                   Gpu::BufferUsageFlags usage,
                                   Sync::AccessType readAccess)
{
    info_.bufferDependencies.push_back(
        RenderTaskInfo::BufferDependency{.kind = TaskDependencyKindBits::Read,
                                         .handle = handle,
                                         .usage = usage,
                                         .access = readAccess});
    return framegraph_.resources().info(handle.buffer());
}

BufferInfo RenderTaskBuilder::read(TransientBufferHandle handle, BufferReadPreset preset)
{
    const auto p = toBufferReadPreset(preset);
    return read(handle, p.usage, p.access);
}

std::pair<TransientTextureHandle, TextureInfo> RenderTaskBuilder::write(
    TransientTextureHandle handle, Gpu::TextureUsageFlags usage, Sync::AccessType writeAccess)
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

std::pair<TransientTextureHandle, TextureInfo>
RenderTaskBuilder::write(TransientTextureHandle handle, TextureWritePreset preset)
{
    const auto p = toTextureWritePreset(preset);
    return write(handle, p.usage, p.access);
}

std::pair<TransientBufferHandle, BufferInfo> RenderTaskBuilder::write(TransientBufferHandle handle,
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

std::pair<TransientBufferHandle, BufferInfo> RenderTaskBuilder::write(TransientBufferHandle handle,
                                                                      BufferWritePreset preset)
{
    const auto p = toBufferWritePreset(preset);
    return write(handle, p.usage, p.access);
}

std::pair<TransientTextureHandle, TextureInfo> RenderTaskBuilder::readWrite(
    TransientTextureHandle handle, Gpu::TextureUsageFlags usage, Sync::AccessType readWriteAccess)
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

std::pair<TransientTextureHandle, TextureInfo>
RenderTaskBuilder::readWrite(TransientTextureHandle handle, TextureReadWritePreset preset)
{
    const auto p = toTextureReadWritePreset(preset);
    return readWrite(handle, p.usage, p.access);
}

std::pair<TransientBufferHandle, BufferInfo> RenderTaskBuilder::readWrite(
    TransientBufferHandle handle, Gpu::BufferUsageFlags usage, Sync::AccessType readWriteAccess)
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

std::pair<TransientBufferHandle, BufferInfo>
RenderTaskBuilder::readWrite(TransientBufferHandle handle, BufferReadWritePreset preset)
{
    const auto p = toBufferReadWritePreset(preset);
    return readWrite(handle, p.usage, p.access);
}

TransientRenderPass RenderTaskBuilder::declareRenderPass(RenderPassDeclaration passDeclaration)
{
    std::vector<TransientTextureHandle> colorOutputs;
    colorOutputs.reserve(passDeclaration.attachments.size());
    for (auto &attachment : passDeclaration.attachments) {
        const auto existing =
            findWriteDependency(info_.textureDependencies, attachment.target.texture());
        TransientTextureHandle outputHandle{};
        if (existing.has_value()) {
            outputHandle = *existing;
        }
        else if (shouldReadAttachment(attachment.load)) {
            outputHandle = readWrite(attachment.target,
                                     Gpu::TextureUsageFlagBits::ColorAttachmentBit,
                                     Sync::AccessType::ColorAttachmentReadWrite)
                               .first;
        }
        else {
            outputHandle = write(attachment.target,
                                 Gpu::TextureUsageFlagBits::ColorAttachmentBit,
                                 Sync::AccessType::ColorAttachmentWrite)
                               .first;
        }
        attachment.target = outputHandle;
        colorOutputs.push_back(outputHandle);
    }

    std::optional<TransientTextureHandle> depthOutput;
    if (passDeclaration.depthAttachment.has_value()) {
        auto &depthAttachment = passDeclaration.depthAttachment.value();
        const auto existing =
            findWriteDependency(info_.textureDependencies, depthAttachment.target.texture());
        TransientTextureHandle outputHandle{};
        if (existing.has_value()) {
            outputHandle = *existing;
        }
        else if (shouldReadAttachment(depthAttachment.load)) {
            outputHandle = readWrite(depthAttachment.target,
                                     Gpu::TextureUsageFlagBits::DepthStencilAttachmentBit,
                                     Sync::AccessType::DepthStencilAttachmentReadWrite)
                               .first;
        }
        else {
            outputHandle = write(depthAttachment.target,
                                 Gpu::TextureUsageFlagBits::DepthStencilAttachmentBit,
                                 Sync::AccessType::DepthStencilAttachmentWrite)
                               .first;
        }
        depthAttachment.target = outputHandle;
        depthOutput = outputHandle;
    }

    std::optional<TransientTextureHandle> stencilOutput;
    if (passDeclaration.stencilAttachment.has_value()) {
        auto &stencilAttachment = passDeclaration.stencilAttachment.value();
        const auto existing =
            findWriteDependency(info_.textureDependencies, stencilAttachment.target.texture());
        TransientTextureHandle outputHandle{};
        if (existing.has_value()) {
            outputHandle = *existing;
        }
        else if (shouldReadAttachment(stencilAttachment.load)) {
            outputHandle = readWrite(stencilAttachment.target,
                                     Gpu::TextureUsageFlagBits::DepthStencilAttachmentBit,
                                     Sync::AccessType::DepthStencilAttachmentReadWrite)
                               .first;
        }
        else {
            outputHandle = write(stencilAttachment.target,
                                 Gpu::TextureUsageFlagBits::DepthStencilAttachmentBit,
                                 Sync::AccessType::DepthStencilAttachmentWrite)
                               .first;
        }
        stencilAttachment.target = outputHandle;
        stencilOutput = outputHandle;
    }

    return TransientRenderPass{ctx_,
                               framegraph_.resources(),
                               std::move(passDeclaration),
                               std::move(colorOutputs),
                               std::move(depthOutput),
                               std::move(stencilOutput)};
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

} // namespace Cory
