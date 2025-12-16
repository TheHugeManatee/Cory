#pragma once

#include <Cory/Application/Application.hpp>
#include <Cory/Application/CameraManipulator.hpp>
#include <Cory/Application/Common.hpp>
#include <Cory/Application/DynamicGeometry.hpp>
#include <Cory/Framegraph/Common.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>
#include <Cory/Renderer/Common.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <memory>
#include <vector>

struct CubeUBO {
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 viewProjection;
    glm::vec3 lightPosition;
};

struct InstanceData {
    glm::mat4 modelTransform{1.0f};
    glm::vec4 color{1.0f};
    float blend{1.0f};
    float padding[3]{};
};

struct InstanceBuffer {
    Gpu::Buffer buffer;
    Gpu::DeviceSize capacity{0};
};

class CubeDemoApplication : public Cory::Application {
  public:
    CubeDemoApplication(int argc, const char **argv);
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
    cubeRenderTask(Cory::RenderTaskBuilder builder,
                   Cory::TransientTextureHandle colorTarget,
                   Cory::TransientTextureHandle depthTarget);

    static double now();
    [[nodiscard]] double getElapsedTimeSeconds() const;

    void drawImguiControls();

    void setupCameraCallbacks();
    Gpu::VertexOptions vertexOptions() const;
    InstanceBuffer &instanceBufferForFrame(uint32_t frameIndex, uint32_t instanceCount);
    uint32_t prepareInstanceData(float timeSeconds);

  private:
    bool disableValidation_{false};
    uint64_t framesToRender_{0}; // the frames to render - 0 is infinite
    std::unique_ptr<Cory::Window> window_;

    Cory::ShaderHandle vertexShader_;
    Cory::ShaderHandle fragmentShader_;
    std::unique_ptr<Cory::Mesh> mesh_;

    std::unique_ptr<Cory::UniformBufferObject<CubeUBO>> globalUbo_;
    std::vector<Gpu::BindGroup> bindGroups_;
    double startupTime_;
    bool dumpNextFramegraph_{false};

    Cory::CameraManipulator camera_;
    std::vector<InstanceBuffer> instanceBuffers_;
    std::vector<InstanceData> instanceData_;
};
