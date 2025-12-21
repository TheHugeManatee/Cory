#pragma once

#include <Cory/Application/Application.hpp>
#include <Cory/Application/DynamicGeometry.hpp>
#include <Cory/Renderer/Gpu.hpp>
#include <expected>

#include <KDGpu/shader_object.h>

#include <Cory/Renderer/Shader.hpp>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Cory {
class ImGuiLayer;
class Window;
struct FrameContext;
} // namespace Cory

namespace KDGpu {
class RenderPassCommandRecorder;
}

class DynamicPipelineApplication : public Cory::Application {
  public:
    DynamicPipelineApplication(int argc, char **argv);
    ~DynamicPipelineApplication() override;

    void run() override;

  private:
    struct DynamicStateSettings {
        Gpu::CullModeFlags cullMode{Gpu::CullModeFlagBits::BackBit};
        Gpu::FrontFace frontFace{Gpu::FrontFace::CounterClockwise};
        Gpu::PolygonMode polygonMode{Gpu::PolygonMode::Fill};
        bool depthTest{true};
        bool depthWrite{true};
        bool depthClamp{false};
        bool depthBias{false};
        float lineWidthValue{1.0f};
        bool depthBounds{false};
        bool primitiveRestart{false};
        bool rasterizerDiscard{false};
        bool alphaToCoverage{false};
        bool alphaToOne{false};
        bool colorBlend{true};
        bool logicOp{false};
        bool animateViewport{true};
        float viewportScale{0.75f};
        float scissorInset{0.02f};
        bool sampleMaskAlternating{false};
        bool writeMask[4]{true, true, true, true};
        Gpu::LogicOperation logicOperation{Gpu::LogicOperation::Copy};
    };

    void loadShaders();
    void createGeometry();
    void recordCommands(Cory::FrameContext &frameCtx);
    void renderImGuiOverlay(Cory::FrameContext &frameCtx,
                            KDGpu::RenderPassCommandRecorder *recorder);
    void drawUi();
    bool compileFragmentShaderSource(std::string_view sourceText);

    static uint32_t decodeSampleCount(Gpu::SampleCountFlagBits flag);
    std::expected<Gpu::ShaderObject, std::string>
    createShaderObject(const Cory::ShaderSource &sourcePath,
                       std::string_view label,
                       Gpu::ShaderStageFlagBits stage,
                       Gpu::ShaderStageFlags nextStage);

    double now() const;
    double getElapsedTimeSeconds() const;

    uint64_t framesToRender_{0};
    bool disableValidation_{false};
    double startupTime_{0.0};

    std::unique_ptr<Cory::Window> window_;
    Cory::ImGuiLayer *imguiLayer_{nullptr};
    Cory::Mesh mesh_;

    Gpu::ShaderObject vertexShader_;
    Gpu::ShaderObject fragmentShader_;
    std::optional<Cory::ShaderSource> fragmentShaderCode_;
    std::string fragmentShaderEditorSource_;
    std::string fragmentShaderCompileMessage_;
    double fragmentShaderLastEditTime_{0.0};
    bool fragmentShaderDirty_{false};
    bool fragmentShaderAutoCompile_{false};
    bool fragmentShaderCompileSuccess_{true};

    std::vector<Gpu::VertexBufferLayout> vertexLayouts_;
    std::vector<Gpu::VertexAttribute> vertexAttributes_;

    DynamicStateSettings settings_{};
    std::vector<Gpu::TextureLayout> swapchainLayouts_;
    std::vector<Gpu::TextureLayout> depthLayouts_;

    void resetAttachmentLayouts();
    void transitionColorAttachmentForRender(Cory::FrameContext &frameCtx);
    void transitionColorAttachmentForPresent(Cory::FrameContext &frameCtx);
    void transitionDepthAttachmentForRender(Cory::FrameContext &frameCtx);
};
