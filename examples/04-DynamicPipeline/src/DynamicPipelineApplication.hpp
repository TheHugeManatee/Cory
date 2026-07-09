#pragma once

#include <Cory/Application/Application.hpp>
#include <Cory/Application/DynamicGeometry.hpp>
#include <Cory/Coro/Coro.hpp>
#include <Cory/Renderer/Gpu.hpp>
#include <Cory/Renderer/Shader.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

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

    Cory::EagerJob loadShaders();
    void createGeometry();
    void recordCommands(Cory::FrameContext &frameCtx);
    void renderImGuiOverlay(Cory::FrameContext &frameCtx,
                            KDGpu::RenderPassCommandRecorder *recorder);
    void drawUi(const Cory::FrameContext &frameCtx);
    bool compileFragmentShaderSource(std::string_view sourceText, uint64_t currentFrameNumber);

    static uint32_t decodeSampleCount(Gpu::SampleCountFlagBits flag);

    double now() const;
    double getElapsedTimeSeconds() const;

    uint64_t framesToRender_{0};
    std::filesystem::path outputPath_{};
    const Cory::Texture *lastRenderedTexture_{nullptr};
    uint32_t lastRenderedWidth_{0};
    uint32_t lastRenderedHeight_{0};
    Gpu::Format lastRenderedFormat_{};
    Gpu::TextureLayout lastRenderedLayout_{Gpu::TextureLayout::PresentSrc};
    bool disableValidation_{false};
    bool headless_{false};
    double startupTime_{0.0};

    std::unique_ptr<Cory::Window> window_;
    std::unique_ptr<Cory::HeadlessFrameSource> headlessFrames_;
    Cory::ImGuiLayer *imguiLayer_{nullptr};
    Cory::Mesh mesh_;

    Cory::ShaderHandle vertexShader_;
    Cory::ShaderHandle fragmentShader_;
    std::optional<Cory::ShaderSource> fragmentShaderCode_;
    Cory::EagerJob shaderAutoReloadTask_;
    bool requestCompile_{false};
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
