#include "DescriptorSets.hpp"

#include <Cory/Base/EnumMap.hpp>
#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/Context.hpp>

#include <KDGpu/bind_group.h>
#include <KDGpu/bind_group_description.h>
#include <KDGpu/bind_group_layout.h>
#include <KDGpu/bind_group_layout_options.h>
#include <KDGpu/bind_group_options.h>
#include <KDGpu/bind_group_pool_options.h>
#include <KDGpu/vulkan/vulkan_graphics_api.h>

#include <vector>

namespace Cory {

struct DescriptorSetManagerPrivate {
    Gpu::Device *device{};
    EnumMap<DescriptorSetType, Gpu::BindGroupLayout> layouts;
    Gpu::BindGroupPool bindGroupPool;
    std::vector<Gpu::BindGroupLayoutHandle> layoutHandles;
    EnumMap<DescriptorSetType, std::array<Gpu::BindGroup, MAX_FRAMES_IN_FLIGHT>> bindGroups;
    using PerSetWriteStorage = std::array<std::vector<Gpu::BindGroupEntry>, MAX_FRAMES_IN_FLIGHT>;
    EnumMap<DescriptorSetType, PerSetWriteStorage> pendingWrites;
};

// defaulted - nothing to be done here
DescriptorSets::DescriptorSets() = default;
DescriptorSets::~DescriptorSets() = default;

void DescriptorSets::init(Gpu::Device &device, DescriptorSetOptions options)
{
    CO_CORE_ASSERT(data_ == nullptr, "Object already initialized!");
    data_ = std::make_unique<DescriptorSetManagerPrivate>();
    data_->device = &device;

    constexpr auto resourceBindingFlags = Gpu::ResourceBindingFlagBits::UpdateAfterBindBit |
                                          Gpu::ResourceBindingFlagBits::PartiallyBoundBit;
    constexpr auto bindGroupLayoutFlags = Gpu::BindGroupLayoutFlagBits::UpdateAfterBind;

    auto makeBinding =
        [&](uint32_t binding, uint32_t count, Gpu::ResourceBindingType resourceType) {
            return Gpu::ResourceBindingLayout{
                .binding = binding,
                .count = count,
                .resourceType = resourceType,
                .shaderStages = options.shaderStages,
                .flags = resourceBindingFlags,
            };
        };

    data_->layouts[DescriptorSetType::GlobalData] =
        device.createBindGroupLayout(Gpu::BindGroupLayoutOptions{
            .label = fmt::format("{} (GlobalData)", options.label),
            .bindings =
                {
                    makeBinding(0, 1, Gpu::ResourceBindingType::UniformBuffer),
                },
            .flags = bindGroupLayoutFlags,
        });

    data_->layouts[DescriptorSetType::BindlessTextures] =
        device.createBindGroupLayout(Gpu::BindGroupLayoutOptions{
            .label = fmt::format("{} (BindlessTextures)", options.label),
            .bindings =
                {
                    makeBinding(ImageBindPoint::Samplers,
                                gsl::narrow_cast<uint32_t>(MAX_SAMPLERS),
                                Gpu::ResourceBindingType::Sampler),
                    makeBinding(ImageBindPoint::Texture2D,
                                gsl::narrow_cast<uint32_t>(MAX_IMAGES),
                                Gpu::ResourceBindingType::CombinedImageSampler),
                    makeBinding(ImageBindPoint::Texture3D,
                                gsl::narrow_cast<uint32_t>(MAX_IMAGES),
                                Gpu::ResourceBindingType::CombinedImageSampler),
                    makeBinding(ImageBindPoint::StorageImage2D,
                                gsl::narrow_cast<uint32_t>(MAX_IMAGES),
                                Gpu::ResourceBindingType::StorageImage),
                    makeBinding(ImageBindPoint::StorageImage3D,
                                gsl::narrow_cast<uint32_t>(MAX_IMAGES),
                                Gpu::ResourceBindingType::StorageImage),
                },
            .flags = bindGroupLayoutFlags,
        });

    data_->layouts[DescriptorSetType::BindlessBuffers] =
        device.createBindGroupLayout(Gpu::BindGroupLayoutOptions{
            .label = fmt::format("{} (BindlessBuffers)", options.label),
            .bindings =
                {
                    makeBinding(BufferBindPoint::StorageBufferReadOnly,
                                gsl::narrow_cast<uint32_t>(MAX_BUFFERS),
                                Gpu::ResourceBindingType::StorageBuffer),
                    makeBinding(BufferBindPoint::StorageBufferReadWrite,
                                gsl::narrow_cast<uint32_t>(MAX_BUFFERS),
                                Gpu::ResourceBindingType::StorageBuffer),
                },
            .flags = bindGroupLayoutFlags,
        });

    data_->layoutHandles.resize(magic_enum::enum_count<DescriptorSetType>());
    for (DescriptorSetType type : magic_enum::enum_values<DescriptorSetType>()) {
        data_->layoutHandles[static_cast<size_t>(type)] = data_->layouts[type].handle();
    }

    data_->bindGroupPool = device.createBindGroupPool(Gpu::BindGroupPoolOptions{
        .label = "Default BindGroupPool",
        .uniformBufferCount = MAX_FRAMES_IN_FLIGHT,
        .dynamicUniformBufferCount = 0,
        .storageBufferCount = gsl::narrow<uint16_t>(MAX_BUFFERS * MAX_FRAMES_IN_FLIGHT),
        .textureSamplerCount = gsl::narrow<uint16_t>(MAX_IMAGES * MAX_FRAMES_IN_FLIGHT),
        .textureCount = gsl::narrow<uint16_t>(MAX_IMAGES * MAX_FRAMES_IN_FLIGHT),
        .samplerCount = gsl::narrow<uint16_t>(MAX_SAMPLERS),
        .imageCount = gsl::narrow<uint16_t>(MAX_IMAGES * MAX_FRAMES_IN_FLIGHT),
        .inputAttachmentCount = 0,
        .accelerationStructureCount = 0,
        .maxBindGroupCount = gsl::narrow<uint16_t>(MAX_FRAMES_IN_FLIGHT *
                                                   magic_enum::enum_count<DescriptorSetType>()),
        .flags = Gpu::BindGroupPoolFlagBits::CreateFreeBindGroups |
                 Gpu::BindGroupPoolFlagBits::UpdateAfterBind,
    });

    for (DescriptorSetType type : magic_enum::enum_values<DescriptorSetType>()) {
        for (gsl::index i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            data_->bindGroups[type][i] = device.createBindGroup(Gpu::BindGroupOptions{
                .label = fmt::format("{} Descriptor Set Frame {}", type, i),
                .layout = data_->layouts[type],
                .resources = {},
                .bindGroupPool = data_->bindGroupPool,
            });
        }
    }
}

const std::vector<Gpu::BindGroupLayoutHandle> &DescriptorSets::layouts() const noexcept
{
    return data_->layoutHandles;
}

Gpu::BindGroup &DescriptorSets::get(DescriptorSetType type, gsl::index frameInFlightIndex)
{
    CO_CORE_DEBUG_ASSERT(data_ != nullptr, "DescriptorSets not initialized, or moved-from");
    return data_->bindGroups[type][frameInFlightIndex];
}

DescriptorSets &DescriptorSets::write(ImageBindPoint bindPoint,
                                      gsl::index instanceIndex,
                                      TextureHeapIndex textureIndex,
                                      Gpu::TextureLayout layout,
                                      Gpu::TextureViewHandle image,
                                      Gpu::TextureSamplerHandle sampler)
{
    CO_CORE_DEBUG_ASSERT(data_ != nullptr, "DescriptorSets not initialized, or moved-from");
    CO_CORE_ASSERT(textureIndex < MAX_IMAGES, "Texture index out of range");

    if (bindPoint == ImageBindPoint::Samplers) {
        CO_CORE_ASSERT(!image.isValid(), "Sampler writes should not provide image layouts");
        CO_CORE_ASSERT(sampler.isValid(), "Sampler writes require samplers");
        write(instanceIndex, textureIndex, sampler);
        return *this;
    }

    auto &writes = data_->pendingWrites[DescriptorSetType::BindlessTextures][instanceIndex];

    const bool isStorageImage =
        bindPoint == ImageBindPoint::StorageImage2D || bindPoint == ImageBindPoint::StorageImage3D;
    if (isStorageImage) {
        writes.emplace_back(Gpu::BindGroupEntry{
            .binding = bindPoint,
            .resource = Gpu::ImageBinding{.textureView = image, .layout = layout},
            .arrayElement = textureIndex,
        });
    }
    else {
        writes.emplace_back(Gpu::BindGroupEntry{
            .binding = bindPoint,
            .resource = Gpu::TextureViewSamplerBinding{.textureView = image,
                                                       .sampler = sampler,
                                                       .layout = layout},
            .arrayElement = textureIndex,
        });
    }

    return *this;
}
DescriptorSets &DescriptorSets::write(gsl::index instanceIndex,
                                      SamplerHeapIndex samplerIndex,
                                      Gpu::TextureSamplerHandle sampler)
{
    CO_CORE_DEBUG_ASSERT(data_ != nullptr, "DescriptorSets not initialized, or moved-from");
    CO_CORE_ASSERT(samplerIndex < MAX_SAMPLERS, "Texture index out of range");

    auto &writes = data_->pendingWrites[DescriptorSetType::BindlessTextures][instanceIndex];
    writes.emplace_back(Gpu::BindGroupEntry{
        .binding = ImageBindPoint::Samplers,
        .resource = Gpu::SamplerBinding{.sampler = sampler},
        .arrayElement = samplerIndex,
    });
    return *this;
}

DescriptorSets &DescriptorSets::write(BufferBindPoint type,
                                      gsl::index instanceIndex,
                                      BufferHeapIndex bufferIndex,
                                      Gpu::BufferHandle buffer)
{
    CO_CORE_DEBUG_ASSERT(data_ != nullptr, "DescriptorSets not initialized, or moved-from");
    CO_CORE_ASSERT(bufferIndex < MAX_BUFFERS, "Buffer index out of range");

    data_->pendingWrites[DescriptorSetType::BindlessBuffers][instanceIndex].emplace_back(
        Gpu::BindGroupEntry{
            .binding = type,
            .resource = Gpu::StorageBufferBinding{.buffer = buffer},
            .arrayElement = bufferIndex,
        });
    return *this;
}

DescriptorSets &DescriptorSets::flush(gsl::index instanceIndex)
{
    CO_CORE_DEBUG_ASSERT(data_ != nullptr, "DescriptorSets not initialized, or moved-from");
    CO_CORE_ASSERT(instanceIndex >= 0 && instanceIndex < MAX_FRAMES_IN_FLIGHT,
                   "Instance index out of range");

    auto &resources = *data_->device->graphicsApi()->resourceManager();
    auto vulkanDevice = resources.getDevice(data_->device->handle());

    // reserve a reasonable amount to avoid multiple allocations
    constexpr size_t maxWrites = 1024;
    std::array<Gpu::WriteBindGroupData, maxWrites> writeStorage;
    gsl::index writeCount = 0;

    for (DescriptorSetType set : magic_enum::enum_values<DescriptorSetType>()) {
        auto &writes = data_->pendingWrites[set][instanceIndex];

        auto vulkanBindGroup =
            resources.getBindGroup(data_->bindGroups[set][instanceIndex].handle());

        for (const auto &write : writes) {
            vulkanBindGroup->fillWriteBindGroupData(writeStorage[writeCount], write);
            ++writeCount;

            // Avoid overflowing the fixed-size array, worst case we still have a couple of extra
            // calls to updateDescriptorSets()
            if (writeCount == maxWrites) {
                vulkanDevice->updateDescriptorSets(gsl::span(writeStorage.data(), writeCount));
                writeCount = 0;
            }
        }
        writes.clear();
    }

    // flush any remaining writes
    if (writeCount > 0) {
        vulkanDevice->updateDescriptorSets(gsl::span(writeStorage.data(), writeCount));
    }

    return *this;
}

DescriptorSets &DescriptorSets::bind(Gpu::RenderPassCommandRecorder &cmd,
                                     gsl::index frameInFlightIndex)
{
    CO_CORE_ASSERT(data_ != nullptr, "DescriptorSets not initialized");

    for (DescriptorSetType type : magic_enum::enum_values<DescriptorSetType>()) {
        cmd.setBindGroup(static_cast<uint32_t>(type), data_->bindGroups[type][frameInFlightIndex]);
    }
    return *this;
}

DescriptorSets &DescriptorSets::bind(Gpu::RenderPassCommandRecorder &cmd,
                                     gsl::index frameInFlightIndex,
                                     Gpu::PipelineLayoutHandle pipelineLayout)
{
    CO_CORE_ASSERT(data_ != nullptr, "DescriptorSets not initialized");

    for (DescriptorSetType type : magic_enum::enum_values<DescriptorSetType>()) {
        cmd.setBindGroup(static_cast<uint32_t>(type),
                         data_->bindGroups[type][frameInFlightIndex],
                         pipelineLayout);
    }
    return *this;
}

DescriptorSets &DescriptorSets::bind(Gpu::ComputePassCommandRecorder &cmd,
                                     gsl::index frameInFlightIndex)
{
    CO_CORE_ASSERT(data_ != nullptr, "DescriptorSets not initialized");

    for (DescriptorSetType type : magic_enum::enum_values<DescriptorSetType>()) {
        cmd.setBindGroup(static_cast<uint32_t>(type), data_->bindGroups[type][frameInFlightIndex]);
    }
    return *this;
}

} // namespace Cory
