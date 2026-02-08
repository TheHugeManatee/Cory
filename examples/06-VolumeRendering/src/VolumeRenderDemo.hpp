#pragma once

#include <Cory/Application/Application.hpp>
#include <Cory/Application/Common.hpp>
#include <Cory/Base/SimulationClock.hpp>
#include <Cory/Framegraph/Common.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/SceneGraph/SceneGraph.hpp>
#include <Cory/Systems/SystemCoordinator.hpp>

#include <memory>
#include <span>

class VolumeRenderDemoApplication : public Cory::Application {
  public:
    explicit VolumeRenderDemoApplication(std::span<const char *> args);
    ~VolumeRenderDemoApplication() override;

    void run() override;

  private:
    // create the mesh to be rendered
    void defineRenderPasses(Cory::Framegraph &framegraph, const Cory::FrameContext &frameCtx);

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

    bool debugRaycast_{false};
    class VolumeRenderSystem *volumeRenderer_{nullptr};

    void setupSystems();
    void setupScene();
};
