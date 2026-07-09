#pragma once

#include <Cory/Application/Application.hpp>
#include <Cory/Application/Common.hpp>
#include <Cory/Base/Prop.hpp>
#include <Cory/Base/SimulationClock.hpp>
#include <Cory/Framegraph/Common.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/SceneGraph/SceneGraph.hpp>
#include <Cory/Systems/SystemCoordinator.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace Cory {
class ComponentEditorSystem;
class ImGuizmoTransformSystem;
}

class VolumeRenderDemoApplication : public Cory::Application {
  public:
    explicit VolumeRenderDemoApplication(std::span<const char *> args);
    ~VolumeRenderDemoApplication() override;

    void run() override;

    Cory::KdbProperty<bool> showImGuizmo{false};
    Cory::KdbProperty<int32_t> msaaSamples{1};

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
    size_t volumeSliceSubsampleFactor_{1u};

    Cory::SimulationClock clock_;
    Cory::CameraLayer *cameraLayer_;
    Cory::SceneGraph sceneGraph_;
    Cory::SystemCoordinator systems_;

    class VolumeRenderSystem *volumeRenderer_{nullptr};
    class VolumeManagerSystem *volumeManager_{nullptr};
    Cory::ImGuizmoTransformSystem *imguizmoSystem_{nullptr};
    Cory::ComponentEditorSystem* componentEditorSystem_{nullptr};

    struct CatalogDataset {
        std::string datasetId{};
        std::filesystem::path manifestPath{};
        glm::vec3 volumeSize{4.0f, 4.0f, 4.0f};
    };
    std::vector<CatalogDataset> catalogDatasets_{};

    void setupSystems();
    void setupComponentEditors();
    void setupScene();
};
