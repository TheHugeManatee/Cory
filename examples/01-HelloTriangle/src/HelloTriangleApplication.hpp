#pragma once

#include <Cory/Application/Application.hpp>

#include <memory>

class TrianglePipeline;

namespace Cory {
class Window;
class Context;
} // namespace Cory

struct Mesh;

class HelloTriangleApplication : public Cory::Application {
  public:
    HelloTriangleApplication(int argc, char **argv);
    ~HelloTriangleApplication() override;

    void run() override;

  private:
    // create the mesh to be rendered
    void createGeometry();
    void renderImGuiOverlay(Cory::FrameContext &frameCtx,
                            KDGpu::RenderPassCommandRecorder *recorder);
    // record commands for a new command buffer
    void recordCommands(Cory::FrameContext &frameCtx);
    void createFramebuffers();

    double now() const;
    double getElapsedTimeSeconds() const;

    uint64_t framesToRender_{0}; // the frames to render - 0 is infinite
    std::unique_ptr<Cory::Window> window_;
    std::unique_ptr<TrianglePipeline> pipeline_;
    std::unique_ptr<Mesh> mesh_;
    Cory::ImGuiLayer *imguiLayer_{};

    double startupTime_;
    bool disableValidation_{false};
    void drawImguiControls();
};
