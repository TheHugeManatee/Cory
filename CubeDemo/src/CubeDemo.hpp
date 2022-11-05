#pragma once

#include <Cory/Application/Application.hpp>
#include <Cory/Application/Common.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/Swapchain.hpp>

#include <Magnum/Vk/Framebuffer.h>

#include <memory>

class CubePipeline;

class CubeDemoApplication : public Cory::Application {
  public:
    CubeDemoApplication(int argc, char **argv);
    ~CubeDemoApplication();

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
    std::unique_ptr<CubePipeline> pipeline_;
    std::vector<Magnum::Vk::Framebuffer> framebuffers_;
    std::unique_ptr<Magnum::Vk::Mesh> mesh_;
    std::unique_ptr<Cory::ImGuiLayer> imguiLayer_;

    double startupTime_;
    void drawImguiControls();
};
