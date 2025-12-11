#include "DescriptorSets.hpp"

#include <Cory/Base/EnumMap.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/UniformBufferObject.hpp>

#include <KDGpu/bind_group.h>
#include <KDGpu/bind_group_layout.h>
#include <KDGpu/bind_group_layout_options.h>
#include <KDGpu/bind_group_options.h>
#include <KDGpu/command_recorder.h>
#include <KDGpu/pipeline_layout.h>

#include <vector>

namespace Cory {

struct DescriptorSetManagerPrivate {
    Gpu::Device *device;
    Gpu::BindGroupLayout layout;
    EnumMap<DescriptorSets::SetType, std::array<Gpu::BindGroup, MAX_FRAMES_IN_FLIGHT>> bindGroups;
    // // For each set, we have one vector of pending writes per frame in flight
    using PerSetWriteStorage = std::array<std::vector<Gpu::BindGroupEntry>, MAX_FRAMES_IN_FLIGHT>;
    EnumMap<DescriptorSets::SetType, PerSetWriteStorage> pendingWrites;
};

// defaulted - nothing to be done here
DescriptorSets::DescriptorSets() = default;
DescriptorSets::~DescriptorSets() = default;

void DescriptorSets::init(Gpu::Device &device, Gpu::BindGroupLayoutOptions defaultLayout)
{
    CO_CORE_ASSERT(data_ == nullptr, "Object already initialized!");
    data_ = std::make_unique<DescriptorSetManagerPrivate>();
    data_->device = &device;
    data_->layout = device.createBindGroupLayout(defaultLayout);

    for (SetType type : magic_enum::enum_values<SetType>()) {
        for (gsl::index i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            data_->bindGroups[type][i] = device.createBindGroup(Gpu::BindGroupOptions{
                .label = fmt::format("Static Descriptor Set {}", i),
                .layout = data_->layout,
                .resources = {},
            });
        }
    }
}

const Gpu::BindGroupLayout &DescriptorSets::layout()
{
    return data_->layout;
}

Gpu::BindGroup &DescriptorSets::get(SetType type, gsl::index frameInFlightIndex)
{
    return data_->bindGroups[type][frameInFlightIndex];
}

DescriptorSets &DescriptorSets::write(SetType type,
                                      gsl::index frameInFlightIndex,
                                      const UniformBufferObjectBase &ubo)
{
    // Use KDGpu types for buffer binding

    data_->pendingWrites[type][frameInFlightIndex].emplace_back(KDGpu::BindGroupEntry{
        .binding = static_cast<uint32_t>(BindPoints::UniformBufferObject),
        .resource =
            KDGpu::UniformBufferBinding{
                .buffer = ubo.handle(),
                .offset = 0,
                .size = KDGpu::UniformBufferBinding::WholeSize,
            },
        .arrayElement = 0,
    });
    return *this;
}

DescriptorSets &DescriptorSets::write(SetType type,
                                      gsl::index frameInFlightIndex,
                                      gsl::span<Gpu::TextureLayout> layouts,
                                      gsl::span<Gpu::TextureViewHandle> images,
                                      gsl::span<Gpu::TextureSamplerHandle> samplers)
{
    for (size_t i = 0; i < images.size(); ++i) {
        data_->pendingWrites[type][frameInFlightIndex].emplace_back(KDGpu::BindGroupEntry{
            .binding = static_cast<uint32_t>(BindPoints::CombinedImageSampler),
            .resource =
                KDGpu::TextureViewSamplerBinding{
                    .textureView = images[i],
                    .sampler = samplers[i],
                    .layout = layouts[i],
                },
            .arrayElement = gsl::narrow_cast<uint32_t>(i),
        });
    }
    return *this;
}

DescriptorSets &DescriptorSets::flushWrites()
{
    // Note: Delayed writing currently somewhat useless within KDGpu abstraction -
    // KDGpu::BindGroup::update always updates directly without an option for submitting batched
    // updates, which makes the whole write/flush approach moot

    for (SetType type : magic_enum::enum_values<SetType>()) {
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            auto &writes = data_->pendingWrites[type][i];

            for (const auto &write : writes) {
                data_->bindGroups[type][i].update(write);
            }
            writes.clear();
        }
    }
    return *this;
}

DescriptorSets &DescriptorSets::bind(Gpu::RenderPassCommandRecorder &cmd,
                                     gsl::index frameInFlightIndex)
{
    for (SetType type : magic_enum::enum_values<SetType>()) {
        cmd.setBindGroup(static_cast<uint32_t>(type), data_->bindGroups[type][frameInFlightIndex]);
    }
    return *this;
}

} // namespace Cory