#pragma once

#include <Cory/Application/Application.hpp>
#include <Cory/Application/CameraManipulator.hpp>
#include <Cory/Application/Common.hpp>
#include <Cory/Application/DynamicGeometry.hpp>
#include <Cory/Framegraph/Common.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>
#include <Cory/ImGui/Widgets.hpp>
#include <Cory/Renderer/Common.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

namespace Cory {
class HeadlessFrameSource;
}

struct CubeUBO {
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 viewProjection;
    glm::vec3 lightPosition;
    float padding0;
    Cory::BufferDeviceAddress instances;
};

struct alignas(16) InstanceData {
    glm::mat4 modelTransform{1.0f};
    glm::mat4 normalTransform{1.0f};
    glm::vec4 color{1.0f};
    glm::vec4 parameters{0.0f};
};

static_assert(std::is_trivially_copyable_v<InstanceData>);
static_assert(sizeof(InstanceData) == 2 * sizeof(glm::mat4) + 2 * sizeof(glm::vec4));

struct InstanceBuffer {
    Gpu::Buffer buffer;
    Gpu::DeviceSize capacity{0};
};

class CubeDemoApplication : public Cory::Application {
  public:
    CubeDemoApplication(int argc, const char **argv);
    ~CubeDemoApplication() override;

    void run() override;

  private:
    // create the mesh to be rendered
    void createGeometry();
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
    uint32_t prepareInstanceData(float timeSeconds);

  private:
    bool disableValidation_{false};
    bool headless_{false};
    uint64_t framesToRender_{0}; // the frames to render - 0 is infinite
    std::unique_ptr<Cory::Window> window_;
    std::unique_ptr<Cory::HeadlessFrameSource> headlessFrames_;

    Cory::ShaderHandle vertexShader_;
    Cory::ShaderHandle fragmentShader_;
    std::unique_ptr<Cory::Mesh> mesh_;

    std::vector<Gpu::BindGroup> bindGroups_;
    double startupTime_;
    bool dumpNextFramegraph_{false};

    Cory::CameraManipulator camera_;
    std::vector<InstanceBuffer> instanceBuffers_;
    std::vector<InstanceData> instanceData_;
    CoImGui::DeviceMemoryReportHistory memoryReportHistory_{};
};
