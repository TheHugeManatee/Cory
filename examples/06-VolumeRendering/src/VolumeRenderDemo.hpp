#pragma once

#include <Cory/Application/Application.hpp>
#include <Cory/Application/Common.hpp>
#include <Cory/Base/Prop.hpp>
#include <Cory/Base/SimulationClock.hpp>
#include <Cory/Framegraph/Common.hpp>
#include <Cory/Framegraph/FramegraphResourceManager.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/SceneGraph/SceneGraph.hpp>
#include <Cory/Systems/SystemCoordinator.hpp>

#include <glm/vec2.hpp>

#include <memory>
#include <span>

namespace Cory {
class ImGuizmoTransformSystem;
}

class VolumeRenderDemoApplication : public Cory::Application {
  public:
    explicit VolumeRenderDemoApplication(std::span<const char *> args);
    ~VolumeRenderDemoApplication() override;

    void run() override;

    Cory::Property<bool> debugRasterize{false};
    Cory::Property<bool> debugRaycast{false};
    Cory::Property<bool> showImGuizmo{false};
    Cory::Property<bool> temporalAccumulation{false};
    Cory::Property<float> temporalAccumulationAlpha{0.10f};
    Cory::Property<float> alphaDeltaRejectThreshold{0.01f};
    Cory::Property<int32_t> iterations{1};
    Cory::Property<int32_t> msaaSamples{1};

  private:
    // create the mesh to be rendered
    void defineRenderPasses(Cory::Framegraph &framegraph, const Cory::FrameContext &frameCtx);
    void ensureTemporalHistoryTexture(const Cory::FrameContext &frameCtx);

    void drawImguiControls();

  private:
    uint64_t framesToRender_{0}; // the frames to render - 0 is infinite
    std::unique_ptr<Cory::Window> window_;
    std::unique_ptr<Cory::HeadlessFrameSource> headlessFrames_;
    bool headless_{false};

    bool dumpNextFramegraph_{false};

    Cory::SimulationClock clock_;
    Cory::CameraLayer *cameraLayer_;
    Cory::SceneGraph sceneGraph_;
    Cory::SystemCoordinator systems_;

    class VolumeRenderSystem *volumeRenderer_{nullptr};
    Cory::ImGuizmoTransformSystem *imguizmoSystem_{nullptr};

    Cory::FramegraphResourceManager *resourceManager_{nullptr};
    struct TemporalHistoryBuffer {
        Cory::FramegraphTextureHandle resource{};
        glm::u32vec2 extent{0u, 0u};
        Gpu::Format format{};
        Gpu::SampleCountFlagBits sampleCount{Gpu::SampleCountFlagBits::Samples1Bit};
        bool valid{false};
        bool forceTemporalReset{false};
    };
    TemporalHistoryBuffer temporalHistory_{};

    void setupSystems();
    void setupScene();
};
