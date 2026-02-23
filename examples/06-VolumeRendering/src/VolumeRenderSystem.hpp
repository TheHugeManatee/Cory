#pragma once

#include "Common.hpp"

#include <Cory/Application/DynamicGeometry.hpp>
#include <Cory/Base/Prop.hpp>
#include <Cory/Framegraph/Common.hpp>
#include <Cory/Framegraph/RenderTaskBuilder.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/ShaderHotReloader.hpp>
#include <Cory/SceneGraph/System.hpp>
#include <Cory/Systems/CommonComponents.hpp>

#include <KDGpu/buffer.h>
#include <KDGpu/sampler.h>

#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

namespace Cory {
class Context;
}

struct alignas(16) InstanceData {
    glm::mat4 modelToWorld{1.0f};
    glm::mat4 worldToModel{1.0f};
    glm::mat4 normalToWorld{1.0f};
    glm::vec4 color{1.0f};
    VolumeTransferParams transferParams{};
    float raymarchStepSizeMultiplier{2.0f};
    uint32_t samples{1u};
    // bools not allowed in std140/std430, use float as workaround
    float raymarchJitteringEnabled{1.0f};
    VolumeRenderMode renderMode{VolumeRenderMode::StochasticSingleBounce};
    uint32_t volumeTextureIndex{0u};
    glm::uvec3 volumeDimensions{0u}; // x/y/z=dimensions
};

static_assert(std::is_trivially_copyable_v<InstanceData>);
static_assert(sizeof(InstanceData) == 3 * sizeof(glm::mat4) + 3 * sizeof(glm::vec4) +
                                          sizeof(uint32_t) + sizeof(glm::uvec3));

/**
 * @brief Render-only system for drawing volume entities.
 *
 * Responsibilities:
 * - Reads `VolumeComponent` + `Transform` and records per-instance raymarch data.
 * - Executes raster debug and compute raymarch passes.
 * - Owns temporal history integration/copy to frame color.
 *
 * Component interaction contract:
 * - Expects `VolumeComponent::textureView/textureDimensions/hasTexture` to be maintained by
 *   `VolumeManagerSystem`.
 * - Does not load/generate textures and does not own streaming state.
 *
 */
class VolumeRenderSystem
    : public Cory::BasicSystem<VolumeRenderSystem, VolumeComponent, Cory::Components::Transform> {
  public:
    explicit VolumeRenderSystem(Cory::Context &ctx);
    ~VolumeRenderSystem();

    Cory::Property<bool> debugRasterize{false};
    Cory::Property<bool> debugRaycast{false};
    Cory::Property<bool> temporalAccumulation{true};
    Cory::Property<float> temporalEmaTauMs{50.0f};
    Cory::Property<float> alphaDeltaRejectThreshold{0.5f};

    /// Drops current temporal accumulation history and forces re-initialization next frame.
    void resetTemporalHistory();

    /// Per-frame prepass: processes shader hot reloads and snapshots camera state.
    void beforeUpdate(Cory::SceneGraph &sg, uint64_t frameNumber);

    void update(Cory::SceneGraph &sg,
                Cory::TickInfo tick,
                Cory::Entity entity,
                const VolumeComponent &volume,
                const Cory::Components::Transform &transform);

    struct PassOutputs {
        Cory::TransientTextureHandle colorOut;
        Cory::TransientTextureHandle depthOut;
    };

    /// Optional raster debug path for cube bounds.
    Cory::RenderTaskDeclaration<PassOutputs>
    rasterizationTask(Cory::RenderTaskBuilder builder,
                      Cory::TransientTextureHandle colorTarget,
                      Cory::TransientTextureHandle depthTarget);

    /**
     * @brief Main per-frame volume task.
     *
     * Behavior:
     * - Clears frame attachments
     * - Runs raymarch (or debug/raster paths)
     * - Copies temporal history result to frame color so downstream layers do not render into
     *   history texture.
     */
    Cory::RenderTaskDeclaration<PassOutputs>
    volumeFrameTask(Cory::RenderTaskBuilder builder,
                    Cory::Framegraph &framegraph,
                    const Cory::FrameContext &frameCtx,
                    Cory::TransientTextureHandle colorTarget,
                    Cory::TransientTextureHandle depthTarget);

    /// Compute raymarch path writing into the persistent temporal history target.
    Cory::RenderTaskDeclaration<Cory::TransientTextureHandle>
    cubeRaycastTask(Cory::RenderTaskBuilder builder,
                    Cory::TransientTextureHandle colorTarget,
                    Cory::TransientTextureHandle depthTarget);

    /// Compute debug path visualizing local-space ray/box intersection.
    Cory::RenderTaskDeclaration<Cory::TransientTextureHandle>
    cubeRaycastDebugTask(Cory::RenderTaskBuilder builder,
                         Cory::TransientTextureHandle colorTarget,
                         Cory::TransientTextureHandle depthTarget);

  private:
    struct RenderStateEntry {
        InstanceData data{};
        Gpu::TextureViewHandle textureView{};
        bool hasTexture{false};
    };
    std::vector<RenderStateEntry> renderState_;
    Cory::Components::CameraComponent camera_;

    Cory::Mesh cube_;

    Cory::ShaderHandle vertexShader_;
    Cory::ShaderHandle fragmentShader_;
    Cory::ShaderHandle raycastShader_;
    Cory::ShaderHandle raycastDebugShader_;
    Cory::ShaderHotReloader shaderHotReloader_;

    Gpu::Sampler volumeSampler_;
    float currentFrameTimeSeconds_{0.0f};
    float lastFrameDeltaSeconds_{1.0f / 60.0f};

    Cory::Context *ctx_{nullptr};
    struct TemporalHistoryBuffer {
        Cory::FramegraphTextureHandle resource{};
        glm::u32vec2 extent{0u, 0u};
        Gpu::Format format{};
        Gpu::SampleCountFlagBits sampleCount{Gpu::SampleCountFlagBits::Samples1Bit};
        bool valid{false};
        bool forceTemporalReset{false};
    };
    TemporalHistoryBuffer temporalHistory_{};

    void ensureTemporalHistoryTexture(const Cory::FrameContext &frameCtx);
};
