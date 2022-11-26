#pragma once

#include <Cory/Application/Application.hpp>
#include <Cory/Renderer/Swapchain.hpp>

#include <Magnum/Vk/Framebuffer.h>

#include <memory>

class TrianglePipeline;

namespace Cory {
class Window;
class Context;
class ImGuiLayer;
} // namespace Cory

namespace Magnum::Vk {
class Mesh;
}

class HelloTriangleApplication : public Cory::Application {
  public:
    HelloTriangleApplication(int argc, char** argv);
    ~HelloTriangleApplication();

    void run() override;

  private:
    // create a framebuffer for each of the swap chain images
    void createFramebuffers();
    // create the mesh to be rendered
    void createGeometry();
    // record commands for a new command buffer
    void recordCommands(Cory::FrameContext &frameCtx);

    double now() const;
    double getElapsedTimeSeconds() const;

  private:
    uint64_t framesToRender_{0}; // the frames to render - 0 is infinite
    std::unique_ptr<Cory::Context> ctx_;
    std::unique_ptr<Cory::Window> window_;
    std::unique_ptr<TrianglePipeline> pipeline_;
    std::vector<Magnum::Vk::Framebuffer> framebuffers_;
    std::unique_ptr<Magnum::Vk::Mesh> mesh_;
    std::unique_ptr<Cory::ImGuiLayer> imguiLayer_;

    double startupTime_;
    void drawImguiControls();
};
