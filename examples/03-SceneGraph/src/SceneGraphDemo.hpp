#pragma once

#include <Cory/Application/Application.hpp>
#include <Cory/Application/CameraManipulator.hpp>
#include <Cory/Application/Common.hpp>
#include <Cory/Base/SimulationClock.hpp>
#include <Cory/Framegraph/Common.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/Swapchain.hpp>
#include <Cory/Renderer/UniformBufferObject.hpp>
#include <Cory/SceneGraph/SceneGraph.hpp>
#include <Cory/Systems/SystemCoordinator.hpp>

#include <Magnum/Vk/DescriptorSet.h>
#include <Magnum/Vk/Framebuffer.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <memory>
#include <span>

class SceneGraphDemoApplication : public Cory::Application {
  public:
    SceneGraphDemoApplication(std::span<const char *> args);
    ~SceneGraphDemoApplication();

    void run() override;

  private:
    // create the mesh to be rendered
    void defineRenderPasses(Cory::Framegraph &framegraph, const Cory::FrameContext &frameCtx);

    void drawImguiControls();

    void setupCameraCallbacks();

  private:
    bool disableValidation_{false};
    uint64_t framesToRender_{0}; // the frames to render - 0 is infinite
    std::unique_ptr<Cory::Window> window_;

    std::vector<Magnum::Vk::DescriptorSet> descriptorSets_;
    bool dumpNextFramegraph_{false};

    Cory::SimulationClock clock_;
    Cory::CameraManipulator camera_;
    Cory::SceneGraph sceneGraph_;
    Cory::SystemCoordinator systems_;
    class CubeAnimationSystem *animationSystem_{nullptr};
    class CubeRenderSystem *renderSystem_{nullptr};
    void setupSystems();
};
