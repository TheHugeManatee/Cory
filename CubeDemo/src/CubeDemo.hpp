#pragma once

#include <Cory/Application/Application.hpp>
#include <Cory/Application/CameraManipulator.hpp>
#include <Cory/Application/Common.hpp>
#include <Cory/Framegraph/Common.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>
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

class CubeDemoApplication : public Cory::Application, Cory::NoCopy, Cory::NoMove {
  public:
    CubeDemoApplication(int argc, char **argv);
    ~CubeDemoApplication();

    void run() override;

  private:
    // create the mesh to be rendered
    void createGeometry();
    void createUBO();
    void createShaders();
    void defineRenderPasses(Cory::Framegraph &framegraph, const Cory::FrameContext &frameCtx);

    struct PassOutputs {
        Cory::TransientTextureHandle colorOut;
        Cory::TransientTextureHandle depthOut;
    };
    Cory::RenderTaskDeclaration<PassOutputs>
    mainCubeRenderTask(Cory::Builder builder,
                       Cory::TransientTextureHandle colorTarget,
                       Cory::TransientTextureHandle depthTarget,
                       const Cory::FrameContext &frameCtx);

    Cory::RenderTaskDeclaration<PassOutputs>
    depthDebugTask(Cory::Builder builder,
                   Cory::TransientTextureHandle depthTarget,
                   const Cory::FrameContext &frameCtx);

    Cory::RenderTaskDeclaration<PassOutputs>
    imguiRenderTask(Cory::Builder builder,
                    Cory::TransientTextureHandle colorTarget,
                    const Cory::FrameContext &ctx);

    static double now();
    [[nodiscard]] double getElapsedTimeSeconds() const;

    void drawImguiControls();

    void setupCameraCallbacks();

  private:
    uint64_t framesToRender_{0}; // the frames to render - 0 is infinite
    std::unique_ptr<Cory::Context> ctx_;
    std::unique_ptr<Cory::Window> window_;

    Cory::ShaderHandle vertexShader_;
    Cory::ShaderHandle fragmentShader_;
    Cory::ShaderHandle fullscreenTriShader_;
    Cory::ShaderHandle depthDebugShader_;
    std::unique_ptr<Magnum::Vk::Mesh> mesh_;
    std::unique_ptr<Cory::ImGuiLayer> imguiLayer_;

    std::unique_ptr<Cory::UniformBufferObject<CubeUBO>> globalUbo_;
    std::vector<Magnum::Vk::DescriptorSet> descriptorSets_;
    double startupTime_;
    bool dumpNextFramegraph_{false};

    Cory::CameraManipulator camera_;
};
