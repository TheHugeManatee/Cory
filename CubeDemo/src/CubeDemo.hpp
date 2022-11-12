#pragma once

#include <Cory/Application/Application.hpp>
#include <Cory/Application/CameraManipulator.hpp>
#include <Cory/Application/Common.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/Swapchain.hpp>
#include <Cory/Renderer/UniformBufferObject.hpp>

#include <Magnum/Vk/DescriptorSet.h>
#include <Magnum/Vk/Framebuffer.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <memory>

class CubePipeline;

struct CubeUBO {
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 viewProjection;
    glm::vec3 lightPosition;
};

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

    void drawImguiControls();

    void setupCameraCallbacks();

  private:
    uint64_t framesToRender_{0}; // the frames to render - 0 is infinite
    std::unique_ptr<Cory::Context> ctx_;
    std::unique_ptr<Cory::Window> window_;
    std::unique_ptr<CubePipeline> pipeline_;
    std::vector<Magnum::Vk::Framebuffer> framebuffers_;
    std::unique_ptr<Magnum::Vk::Mesh> mesh_;
    std::unique_ptr<Cory::ImGuiLayer> imguiLayer_;

    std::unique_ptr<Cory::UniformBufferObject<CubeUBO>> globalUbo_;
    std::vector<Magnum::Vk::DescriptorSet> descriptorSets_;

    double startupTime_;

    Cory::CameraManipulator camera_;
};
