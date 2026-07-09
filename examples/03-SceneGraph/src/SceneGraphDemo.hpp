#pragma once

#include <Cory/Application/Application.hpp>
#include <Cory/Application/Common.hpp>
#include <Cory/Base/SimulationClock.hpp>
#include <Cory/Framegraph/Common.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/SceneGraph/SceneGraph.hpp>
#include <Cory/Systems/SystemCoordinator.hpp>

#include <filesystem>
#include <memory>
#include <span>

class SceneGraphDemoApplication : public Cory::Application {
  public:
    SceneGraphDemoApplication(std::span<const char *> args);
    ~SceneGraphDemoApplication() override;

    void run() override;

  private:
    // create the mesh to be rendered
    void defineRenderPasses(Cory::Framegraph &framegraph, const Cory::FrameContext &frameCtx);

    void drawImguiControls();

  private:
    uint64_t framesToRender_{0}; // the frames to render - 0 is infinite
    std::filesystem::path outputPath_{};
    const Cory::Texture *lastRenderedTexture_{nullptr};
    uint32_t lastRenderedWidth_{0};
    uint32_t lastRenderedHeight_{0};
    Gpu::Format lastRenderedFormat_{};
    Gpu::TextureLayout lastRenderedLayout_{Gpu::TextureLayout::PresentSrc};
    std::unique_ptr<Cory::Window> window_;
    std::unique_ptr<Cory::HeadlessFrameSource> headlessFrames_;
    bool headless_{false};

    bool dumpNextFramegraph_{false};

    Cory::SimulationClock clock_;
    Cory::CameraLayer *cameraLayer_;
    Cory::SceneGraph sceneGraph_;
    Cory::SystemCoordinator systems_;
    class CubeAnimationSystem *animationSystem_{nullptr};
    class CubeRenderSystem *renderSystem_{nullptr};
    void setupSystems();
    void setupScene();
};
