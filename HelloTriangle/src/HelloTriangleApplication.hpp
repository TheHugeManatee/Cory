#pragma once

#include <Cory/UI/Application.hpp>

#include <Cory/UI/SwapChain.hpp>

#include <Magnum/Vk/Framebuffer.h>

#include <memory>

class TrianglePipeline;

namespace Cory {
class Window;
class Context;
} // namespace Cory

namespace Magnum::Vk {
class Framebuffer;
}

class HelloTriangleApplication : public Cory::Application {
  public:
    HelloTriangleApplication();
    ~HelloTriangleApplication();

    void run() override;

  private:
    // create a framebuffer for each of the swap chain images
    void createFramebuffers();
    // record commands for a new command buffer
    void recordCommands(Cory::FrameContext &frameCtx);

  private:
    std::unique_ptr<Cory::Context> ctx_;
    std::unique_ptr<Cory::Window> window_;
    std::unique_ptr<TrianglePipeline> pipeline_;
    std::vector<Magnum::Vk::Framebuffer> frameBuffers_;
};