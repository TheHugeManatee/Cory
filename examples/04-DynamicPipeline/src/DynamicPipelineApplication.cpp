#include "DynamicPipelineApplication.hpp"

#include <Cory/Application/ApplicationLayer.hpp>
#include <Cory/Application/ImGuiLayer.hpp>
#include <Cory/Application/LayerStack.hpp>
#include <Cory/Application/Window.hpp>
#include <Cory/Base/FileWatchManager.hpp>
#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/GlmUtils.hpp>
#include <Cory/Base/Profiling.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Cory.hpp>
#include <Cory/ImGui/Inputs.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/FrameCapture.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/FrameSource.hpp>
#include <Cory/Renderer/HeadlessFrameSource.hpp>
#include <Cory/Renderer/Shader.hpp>
#include <Cory/Renderer/ShaderManager.hpp>
#include <Cory/Renderer/Swapchain.hpp>

#include <KDGpu/memory_barrier.h>
#include <KDGpu/render_pass_command_recorder.h>
#include <KDGpu/render_pass_command_recorder_options.h>
#include <KDGpu/shader_object_options.h>

#include <CLI/CLI.hpp>
#include <glm/vec2.hpp>
#include <gsl/gsl>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
constexpr double kShaderAutoCompileDelaySeconds = 0.35;

int ShaderEditorCallback(ImGuiInputTextCallbackData *data)
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto *str = static_cast<std::string *>(data->UserData);
        str->resize(gsl::narrow<std::string::size_type>(data->BufTextLen));
        data->Buf = str->data();
    }
    return 0;
}

bool ShaderEditorInputTextMultiline(const char *label,
                                    std::string &text,
                                    const ImVec2 &size,
                                    ImGuiInputTextFlags flags = 0)
{
    flags |= ImGuiInputTextFlags_CallbackResize;
    if (text.capacity() == text.size()) {
        text.reserve(text.size() + 1);
    }

    return ImGui::InputTextMultiline(
        label, text.data(), text.capacity() + 1, size, flags, ShaderEditorCallback, &text);
}

} // namespace

DynamicPipelineApplication::DynamicPipelineApplication(int argc, char **argv)
    : startupTime_{now()}
{
    Cory::Init();

    CLI::App app{"DynamicPipeline"};
    std::string outputPathString{};
    app.add_option("-f,--frames", framesToRender_, "Limit the number of rendered frames");
    app.add_option("--output", outputPathString, "Write the final frame to a BMP file");
    app.add_flag("--disable-validation", disableValidation_, "Disable validation layers");
    app.add_flag("--headless", headless_, "Run without a window and render offscreen");
    app.parse(argc, argv);
    if (!outputPathString.empty()) {
        output_.outputPath = outputPathString;
        output_.forceHeadlessAndFinite(headless_, framesToRender_);
    }
    const std::vector<const char *> appArgs{argv, argv + argc};

    // ResourceLocator appends "shaders/" for shader lookups, so register the demo root.
    Cory::ResourceLocator::addSearchPath(
        std::filesystem::path{DYNAMIC_PIPELINE_RESOURCE_DIR}.parent_path());

    init(Cory::ContextCreationInfo{
        .validation =
            disableValidation_ ? Cory::ValidationLayers::Disabled : Cory::ValidationLayers::Enabled,
        .args = std::span{appArgs},
    });

    static constexpr auto WINDOW_SIZE = glm::i32vec2{1280, 720};
    if (headless_) {
        ctx().setupHeadlessDevice();
        headlessFrames_ = std::make_unique<Cory::HeadlessFrameSource>(
            ctx(),
            Cory::HeadlessFrameSourceCreateInfo{
                .label = "DynamicPipeline-Headless",
                .size = Cory::glmu::u32vec2::from(WINDOW_SIZE),
                .samples = Gpu::SampleCountFlagBits::Samples1Bit,
            });
    }
    else {
        window_ = std::make_unique<Cory::Window>(
            ctx(), WINDOW_SIZE, "04 - Dynamic Pipeline", /*sample count*/ 1);
    }

    resetAttachmentLayouts();
    shaderAutoReloadTask_ = loadShaders();
    createGeometry();

    if (!headless_) {
        auto recreateSizedResources = [&](Cory::SwapchainResizedEvent e) {
            resetAttachmentLayouts();
            layers().processEvent(e);
        };
        window_->onSwapchainResized.connect(recreateSizedResources);
        recreateSizedResources({window_->dimensions()});

        Cory::LayerAttachInfo layerAttachInfo{.maxFramesInFlight = Cory::MAX_FRAMES_IN_FLIGHT,
                                              .viewportDimensions = window_->dimensions()};
        imguiLayer_ =
            &layers().emplacePriorityLayer<Cory::ImGuiLayer>(layerAttachInfo, std::ref(*window_));
    }
}

DynamicPipelineApplication::~DynamicPipelineApplication()
{
    auto &shaders = ctx().shaders();
    shaders.release(vertexShader_);
    shaders.release(fragmentShader_);
    shaders.clearDeferredReleases();
}

void DynamicPipelineApplication::run()
{
    auto &frameSource = headless_ ? static_cast<Cory::FrameSource &>(*headlessFrames_)
                                  : static_cast<Cory::FrameSource &>(*window_);
    runMainLoop(
        frameSource,
        framesToRender_,
        {.headless = headless_,
         .processFileWatchEvents = true,
         .clearDeferredShaderReleases = true},
        [this](Cory::FrameContext &frameCtx, const Cory::LogicUpdateContext &) {
            if (requestCompile_) {
                compileFragmentShaderSource(fragmentShaderEditorSource_, frameCtx.frameNumber);
                requestCompile_ = false;
            }

            recordCommands(frameCtx);

            if (output_.requested()) {
                capturedFrame_ = Cory::CapturedFrame{
                    .texture = frameCtx.swapchainImage,
                    .extent = frameCtx.extent,
                    .format = frameCtx.colorFormat,
                    .layout = headless_ ? Gpu::TextureLayout::ColorAttachmentOptimal
                                        : Gpu::TextureLayout::PresentSrc,
                };
            }
        },
        [this](Cory::FrameContext &frameCtx, const Cory::LogicUpdateContext &) {
            drawUi(frameCtx);
        });

    if (!output_.requested()) {
        return;
    }

    CO_CORE_ASSERT(capturedFrame_.has_value(),
                   "DynamicPipeline output requested, but no frame texture was captured");
    Cory::writeCapturedFrameBmp(ctx(), *capturedFrame_, output_.outputPath);
}

Cory::EagerJob DynamicPipelineApplication::loadShaders()
{
    auto vertexPath =
        Cory::ResourceLocator::Locate("dynamic_pipeline.vert.slang", Cory::ResourceType::Shader);
    if (!vertexPath.has_value()) {
        throw std::runtime_error{vertexPath.error()};
    }
    auto fragmentPath =
        Cory::ResourceLocator::Locate("dynamic_pipeline.frag.slang", Cory::ResourceType::Shader);
    if (!fragmentPath.has_value()) {
        throw std::runtime_error{fragmentPath.error()};
    }

    Cory::ShaderSource vertexShaderSource{*vertexPath, Gpu::ShaderStageFlagBits::VertexBit};

    auto vertexShaderHandle = ctx().shaders().createShader(
        vertexShaderSource.source(), Gpu::ShaderStageFlagBits::VertexBit, *vertexPath);

    const auto &vertexShader = ctx().shaders()[vertexShaderHandle];
    if (!vertexShader.valid()) {
        throw std::runtime_error{vertexShader.error()};
    }
    vertexShader_ = vertexShaderHandle;

    fragmentShaderCode_ = Cory::ShaderSource{*fragmentPath, Gpu::ShaderStageFlagBits::FragmentBit};
    fragmentShaderEditorSource_ = fragmentShaderCode_->source();

    auto fragmentShaderHandle = ctx().shaders().createShader(
        fragmentShaderCode_->source(), Gpu::ShaderStageFlagBits::FragmentBit, *fragmentPath);
    const auto &fragmentShader = ctx().shaders()[fragmentShaderHandle];
    if (!fragmentShader.valid()) {
        throw std::runtime_error{fragmentShader.error()};
    }
    fragmentShader_ = fragmentShaderHandle;
    fragmentShaderDirty_ = false;
    fragmentShaderCompileSuccess_ = true;
    fragmentShaderCompileMessage_ = "Fragment shader loaded";

    // Enter the file watch loop coroutine
    auto &fileWatchManager = ctx().fileWatchManager();
    auto fsWatchHandle = fileWatchManager.watch(Cory::FileWatch{.path = fragmentPath->string()});
    for (auto event = Cory::FileWatchEventType::Unknown;
         event != Cory::FileWatchEventType::WatchEnded;
         event = co_await fileWatchManager.nextEvent(fsWatchHandle)) {
        CO_CORE_INFO("Fragment shader file event {}", event);

        if (event == Cory::FileWatchEventType::Modified) {
            // Reload shader from disk
            fragmentShaderCode_ =
                Cory::ShaderSource{*fragmentPath, Gpu::ShaderStageFlagBits::FragmentBit};
            fragmentShaderEditorSource_ = fragmentShaderCode_->source();
            fragmentShaderDirty_ = true;
            requestCompile_ = true;
            fragmentShaderLastEditTime_ = now();
        }
    }
}

void DynamicPipelineApplication::createGeometry()
{
    static constexpr std::array<Cory::Mesh::Vertex, 3> vertices = {
        Cory::Mesh::Vertex{{-0.8f, -0.8f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.9f, 0.2f, 0.2f, 1.0f}},
        Cory::Mesh::Vertex{{0.0f, 0.8f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.2f, 0.2f, 0.9f, 1.0f}},
        Cory::Mesh::Vertex{{0.8f, -0.8f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.2f, 0.9f, 0.2f, 1.0f}},
    };
    static constexpr std::array<uint32_t, 3> indices = {0, 1, 2};

    mesh_ = Cory::DynamicGeometry::createFromCpuBuffers(ctx(), vertices, indices);

    vertexLayouts_ = {{
        {
            .binding = 0,
            .stride = sizeof(Cory::Mesh::Vertex),
            .inputRate = Gpu::VertexRate::Vertex,
        },
    }};
    vertexAttributes_ = Cory::Mesh::vertexAttributes();
    for (auto &attr : vertexAttributes_) {
        attr.binding = 0;
    }
}

void DynamicPipelineApplication::recordCommands(Cory::FrameContext &frameCtx)
{
    transitionColorAttachmentForRender(frameCtx);
    transitionDepthAttachmentForRender(frameCtx);

    const float time = static_cast<float>(getElapsedTimeSeconds());
    const float hue = std::fmod(time * 0.1f, 1.0f);
    const float r = 0.5f + 0.5f * std::sin(hue * 6.28318f);
    const float g = 0.5f + 0.5f * std::sin((hue + 0.33f) * 6.28318f);
    const float b = 0.5f + 0.5f * std::sin((hue + 0.66f) * 6.28318f);

    KDGpu::RenderPassCommandRecorderWithDynamicRenderingOptions passOptions{
        .colorAttachments = {{
            .view = *frameCtx.swapchainImageView,
            .resolveView = {},
            .clearValue = {{r, g, b, 1.0f}},
            .initialLayout = Gpu::TextureLayout::ColorAttachmentOptimal,
            .finalLayout = headless_ ? Gpu::TextureLayout::ColorAttachmentOptimal
                                     : Gpu::TextureLayout::PresentSrc,
        }},
        .depthStencilAttachment =
            {
                .view = *frameCtx.depthImageView,
                .resolveView = {},
                .initialLayout = Gpu::TextureLayout::DepthStencilAttachmentOptimal,
            },
        .samples = frameCtx.sampleCount,
    };

    auto pass = frameCtx.commandBuffer.beginRenderPass(passOptions);

    auto vertexShaderObjectHandle = ctx().shaders()[vertexShader_].shaderHandle();
    auto fragmentShaderObjectHandle = ctx().shaders()[fragmentShader_].shaderHandle();
    pass.bindShaders({Gpu::ShaderStageFlagBits::VertexBit, Gpu::ShaderStageFlagBits::FragmentBit},
                     {vertexShaderObjectHandle, fragmentShaderObjectHandle});

    pass.setPrimitiveTopology(Gpu::PrimitiveTopology::TriangleList);
    pass.setPrimitiveRestartEnabled(settings_.primitiveRestart);

    pass.setCullMode(settings_.cullMode);
    pass.setFrontFace(settings_.frontFace);
    pass.setPolygonMode(settings_.polygonMode);
    if (settings_.polygonMode == Gpu::PolygonMode::Line) {
        pass.setLineWidth(settings_.lineWidthValue);
    }
    pass.setRasterizerDiscardEnabled(settings_.rasterizerDiscard);
    pass.setRasterizationSamples(frameCtx.sampleCount);
    pass.setTessellationDomainOrigin(Gpu::TessellationDomainOrigin::UpperLeft);

    pass.setDepthTestEnabled(settings_.depthTest);
    pass.setDepthWriteEnabled(settings_.depthWrite);
    pass.setDepthCompareOp(Gpu::CompareOperation::LessOrEqual);
    pass.setDepthBiasEnabled(settings_.depthBias);
    pass.setDepthBoundsTestEnabled(settings_.depthBounds);
    pass.setDepthClampEnabled(settings_.depthClamp);
    pass.setStencilTestEnabled(false);
    pass.setStencilOp(Gpu::StencilFaceFlagBits::FrontBit,
                      Gpu::StencilOperation::Keep,
                      Gpu::StencilOperation::Keep,
                      Gpu::StencilOperation::Keep,
                      Gpu::CompareOperation::Always);

    pass.setAlphaToCoverageEnabled(settings_.alphaToCoverage);
    pass.setAlphaToOneEnabled(settings_.alphaToOne);

    const auto viewportScale = settings_.animateViewport
                                   ? 0.35f + 0.35f * (std::sin(time) * 0.5f + 0.5f) + 0.3f
                                   : settings_.viewportScale;
    const float viewportWidth = viewportScale * static_cast<float>(frameCtx.extent.x);
    const float viewportHeight = viewportScale * static_cast<float>(frameCtx.extent.y);
    const float viewportOffsetX = 0.5f * (static_cast<float>(frameCtx.extent.x) - viewportWidth);
    const float viewportOffsetY = 0.5f * (static_cast<float>(frameCtx.extent.y) - viewportHeight);
    const Gpu::Viewport viewport{
        .x = viewportOffsetX,
        .y = viewportOffsetY,
        .width = viewportWidth,
        .height = viewportHeight,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    const std::vector<Gpu::Viewport> viewports{viewport};
    pass.setViewportWithCount(viewports);

    const float insetX = settings_.scissorInset * static_cast<float>(frameCtx.extent.x);
    const float insetY = settings_.scissorInset * static_cast<float>(frameCtx.extent.y);
    const int32_t maxOffsetX = static_cast<int32_t>(frameCtx.extent.x / 2);
    const int32_t maxOffsetY = static_cast<int32_t>(frameCtx.extent.y / 2);
    const int32_t scissorOffsetX =
        std::clamp(static_cast<int32_t>(std::round(insetX)), 0, maxOffsetX);
    const int32_t scissorOffsetY =
        std::clamp(static_cast<int32_t>(std::round(insetY)), 0, maxOffsetY);
    const uint32_t scissorWidth =
        std::max(1u, frameCtx.extent.x - static_cast<uint32_t>(scissorOffsetX * 2));
    const uint32_t scissorHeight =
        std::max(1u, frameCtx.extent.y - static_cast<uint32_t>(scissorOffsetY * 2));
    const Gpu::Rect2D scissorRect{
        .offset = {scissorOffsetX, scissorOffsetY},
        .extent = {scissorWidth, scissorHeight},
    };
    const std::vector<Gpu::Rect2D> scissors{scissorRect};
    pass.setScissorWithCount(scissors);

    Gpu::ColorComponentFlags colorMask{};
    colorMask.setFlag(Gpu::ColorComponentFlagBits::RedBit, settings_.writeMask[0]);
    colorMask.setFlag(Gpu::ColorComponentFlagBits::GreenBit, settings_.writeMask[1]);
    colorMask.setFlag(Gpu::ColorComponentFlagBits::BlueBit, settings_.writeMask[2]);
    colorMask.setFlag(Gpu::ColorComponentFlagBits::AlphaBit, settings_.writeMask[3]);

    const Gpu::ColorBlendEquation blendEquation{
        .srcColorBlendFactor =
            settings_.colorBlend ? Gpu::BlendFactor::SrcAlpha : Gpu::BlendFactor::One,
        .dstColorBlendFactor =
            settings_.colorBlend ? Gpu::BlendFactor::OneMinusSrcAlpha : Gpu::BlendFactor::Zero,
        .colorBlendOp = Gpu::BlendOperation::Add,
        .srcAlphaBlendFactor = Gpu::BlendFactor::One,
        .dstAlphaBlendFactor = Gpu::BlendFactor::Zero,
        .alphaBlendOp = Gpu::BlendOperation::Add,
    };
    const std::vector blendEnabled{settings_.colorBlend};
    const std::vector blendEquations{blendEquation};
    const std::vector colorMasks{colorMask};
    pass.setColorBlendEnabled(0, blendEnabled);
    pass.setColorBlendEquations(0, blendEquations);
    pass.setColorWriteMasks(0, colorMasks);

    pass.setLogicOp(settings_.logicOperation);
    pass.setLogicOpEnabled(settings_.logicOp);

    const uint32_t sampleCount = decodeSampleCount(frameCtx.sampleCount);
    const uint32_t maskWordCount = std::max(1u, (sampleCount + 31u) / 32u);
    const Gpu::SampleMask sampleMask = settings_.sampleMaskAlternating ? 0xAAAAAAAAu : 0xFFFFFFFFu;
    const std::vector sampleMasks(maskWordCount, sampleMask);
    pass.setSampleMask(frameCtx.sampleCount, sampleMasks);

    pass.setVertexInput(vertexLayouts_, vertexAttributes_);
    std::vector<Gpu::VertexBufferBinding> bindings = {{
        {
            .buffer = mesh_.vertexBuffer,
            .offset = 0,
            .size = mesh_.vertexCount * sizeof(Cory::Mesh::Vertex),
            .stride = sizeof(Cory::Mesh::Vertex),
        },
    }};
    pass.setVertexBuffers(0, bindings);
    pass.setIndexBuffer(mesh_.indexBuffer, 0, Gpu::IndexType::Uint32);

    const Gpu::DrawIndexedCommand drawCmd{
        .indexCount = mesh_.indexCount,
    };
    pass.drawIndexed(drawCmd);

    renderImGuiOverlay(frameCtx, &pass);

    pass.end();

    transitionColorAttachmentForPresent(frameCtx);
}

void DynamicPipelineApplication::renderImGuiOverlay(Cory::FrameContext &frameCtx,
                                                    KDGpu::RenderPassCommandRecorder *recorder)
{
    if (!imguiLayer_) {
        return;
    }
    imguiLayer_->recordFrameCommands(frameCtx, recorder);
}

void DynamicPipelineApplication::drawUi(const Cory::FrameContext &frameCtx)
{
    (void)frameCtx;
    if (ImGui::Begin("Dynamic pipeline controls")) {
        CoImGui::ComboBox("Cull Mode", settings_.cullMode);
        CoImGui::ComboBox("Polygon Mode", settings_.polygonMode);
        ImGui::SliderFloat("Line/Point size", &settings_.lineWidthValue, 1.0f, 15.0f);
        CoImGui::ComboBox("Front Face", settings_.frontFace);
        CoImGui::ComboBox("Logic Operation", settings_.logicOperation);

        ImGui::Separator();

        ImGui::Checkbox("Depth test", &settings_.depthTest);
        ImGui::Checkbox("Depth write", &settings_.depthWrite);
        ImGui::Checkbox("Depth clamp", &settings_.depthClamp);
        ImGui::Checkbox("Depth bias", &settings_.depthBias);
        ImGui::Checkbox("Depth bounds", &settings_.depthBounds);
        ImGui::Checkbox("Primitive restart", &settings_.primitiveRestart);
        ImGui::Checkbox("Rasterizer discard", &settings_.rasterizerDiscard);
        ImGui::Checkbox("Alpha to coverage", &settings_.alphaToCoverage);
        ImGui::Checkbox("Alpha to one", &settings_.alphaToOne);
        ImGui::Checkbox("Color blending", &settings_.colorBlend);
        ImGui::Checkbox("Logic op enable", &settings_.logicOp);

        ImGui::Separator();

        ImGui::Checkbox("Animate viewport", &settings_.animateViewport);
        ImGui::SliderFloat("Viewport scale", &settings_.viewportScale, 0.2f, 1.0f);
        ImGui::SliderFloat("Scissor inset", &settings_.scissorInset, 0.0f, 0.45f);
        ImGui::Checkbox("Alternating sample mask", &settings_.sampleMaskAlternating);

        ImGui::Separator();
        ImGui::Checkbox("Write Red", &settings_.writeMask[0]);
        ImGui::Checkbox("Write Green", &settings_.writeMask[1]);
        ImGui::Checkbox("Write Blue", &settings_.writeMask[2]);
        ImGui::Checkbox("Write Alpha", &settings_.writeMask[3]);
    }
    ImGui::End();

    if (ImGui::Begin("Fragment Shader")) {
        const std::string shaderPathString = fragmentShaderCode_->filePath().string();
        ImGui::TextDisabled("%s", shaderPathString.c_str());

        if (ImGui::Button("Compile shader")) {
            requestCompile_ = true;
        }
        ImGui::SameLine();
        ImGui::Checkbox("Auto compile", &fragmentShaderAutoCompile_);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Recompile automatically after edits (%.0f ms delay)",
                              kShaderAutoCompileDelaySeconds * 1000.0);
        }
        ImGui::NewLine();
        const float editorHeight = 320.0f;
        const ImVec2 editorSize{ImGui::GetContentRegionAvail().x, editorHeight};
        const bool editorChanged = ShaderEditorInputTextMultiline(
            "##FragmentShaderEditor",
            fragmentShaderEditorSource_,
            editorSize,
            ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_NoHorizontalScroll);
        const bool editorFocusedThisFrame = ImGui::IsItemFocused();
        if (editorChanged) {
            fragmentShaderDirty_ = true;
            fragmentShaderLastEditTime_ = getElapsedTimeSeconds();
        }

        const ImGuiIO &io = ImGui::GetIO();
        if (editorFocusedThisFrame && (io.KeyCtrl || io.KeySuper) &&
            ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
            requestCompile_ = true;
        }

        if (!requestCompile_ && fragmentShaderDirty_ && fragmentShaderAutoCompile_) {
            const double timeSinceEdit = now() - fragmentShaderLastEditTime_;
            if (timeSinceEdit >= kShaderAutoCompileDelaySeconds) {
                requestCompile_ = true;
            }
        }

        std::string statusLine;
        ImVec4 statusColor{0.8f, 0.8f, 0.8f, 1.0f};
        if (fragmentShaderDirty_) {
            statusLine = "Modified - compile to apply";
            statusColor = ImVec4(0.95f, 0.78f, 0.25f, 1.0f);
        }
        else if (fragmentShaderCompileSuccess_) {
            statusLine = fragmentShaderCompileMessage_.empty() ? "Compilation succeeded"
                                                               : fragmentShaderCompileMessage_;
            statusColor = ImVec4(0.45f, 0.85f, 0.45f, 1.0f);
        }
        else {
            statusLine = "Last compile failed";
            statusColor = ImVec4(0.95f, 0.45f, 0.45f, 1.0f);
        }

        ImGui::Spacing();
        ImGui::TextColored(statusColor, "%s", statusLine.c_str());

        if ((!fragmentShaderDirty_ || !fragmentShaderCompileSuccess_) &&
            !fragmentShaderCompileMessage_.empty()) {
            ImGui::PushTextWrapPos();
            if (fragmentShaderCompileSuccess_) {
                ImGui::TextDisabled("%s", fragmentShaderCompileMessage_.c_str());
            }
            else {
                ImGui::TextUnformatted(fragmentShaderCompileMessage_.c_str());
            }
            ImGui::PopTextWrapPos();
        }
    }
    ImGui::End();
}

bool DynamicPipelineApplication::compileFragmentShaderSource(std::string_view sourceText,
                                                             uint64_t currentFrameNumber)
{
    const auto shaderPath = fragmentShaderCode_->filePath();
    Cory::ShaderSource editedSource{
        std::string{sourceText},
        Gpu::ShaderStageFlagBits::FragmentBit,
        shaderPath,
    };

    Cory::ElapsedTimer timer;
    auto shaderHandle = ctx().shaders().createShader(
        editedSource.source(), Gpu::ShaderStageFlagBits::FragmentBit, shaderPath);
    const auto &shader = ctx().shaders()[shaderHandle];
    if (!shader.valid()) {
        fragmentShaderCompileSuccess_ = false;
        fragmentShaderCompileMessage_ = shader.error();
        return false;
    }

    ctx().shaders().release(fragmentShader_, currentFrameNumber);
    fragmentShader_ = shaderHandle;
    fragmentShaderCode_ = std::move(editedSource);
    fragmentShaderDirty_ = false;
    fragmentShaderCompileSuccess_ = true;
    fragmentShaderCompileMessage_ =
        fmt::format("Compilation succeeded ({:.3} ms)", timer.elapsed() * 1000.0);
    fragmentShaderLastEditTime_ = getElapsedTimeSeconds();
    return true;
}

void DynamicPipelineApplication::resetAttachmentLayouts()
{
    if (!window_ && !headlessFrames_) {
        swapchainLayouts_.clear();
        depthLayouts_.clear();
        return;
    }

    const size_t imageCount = window_ ? window_->swapchain().size() : headlessFrames_->size();
    swapchainLayouts_.assign(imageCount, Gpu::TextureLayout::Undefined);
    depthLayouts_.assign(imageCount, Gpu::TextureLayout::Undefined);
}

void DynamicPipelineApplication::transitionColorAttachmentForRender(Cory::FrameContext &frameCtx)
{
    if (swapchainLayouts_.empty()) {
        return;
    }
    const uint32_t index = frameCtx.swapchainImageIndex;
    if (index >= swapchainLayouts_.size()) {
        return;
    }

    const auto oldLayout = swapchainLayouts_[index];
    const auto srcStages = oldLayout == Gpu::TextureLayout::ColorAttachmentOptimal
                               ? Gpu::PipelineStageFlagBit::ColorAttachmentOutputBit
                               : Gpu::PipelineStageFlagBit::TopOfPipeBit;
    const auto srcMask = oldLayout == Gpu::TextureLayout::ColorAttachmentOptimal
                             ? Gpu::AccessFlagBit::ColorAttachmentWriteBit
                             : Gpu::AccessFlagBit::None;

    frameCtx.commandBuffer.textureMemoryBarrier(Gpu::TextureMemoryBarrierOptions{
        .srcStages = srcStages,
        .srcMask = srcMask,
        .dstStages = Gpu::PipelineStageFlagBit::ColorAttachmentOutputBit,
        .dstMask = Gpu::AccessFlagBit::ColorAttachmentWriteBit,
        .oldLayout = oldLayout,
        .newLayout = Gpu::TextureLayout::ColorAttachmentOptimal,
        .texture = frameCtx.swapchainImage->handle(),
        .range = {.aspectMask = Gpu::TextureAspectFlagBits::ColorBit,
                  .baseMipLevel = 0,
                  .levelCount = 1,
                  .baseArrayLayer = 0,
                  .layerCount = 1},
    });

    swapchainLayouts_[index] = Gpu::TextureLayout::ColorAttachmentOptimal;
}

void DynamicPipelineApplication::transitionColorAttachmentForPresent(Cory::FrameContext &frameCtx)
{
    if (headless_) {
        return;
    }
    if (swapchainLayouts_.empty()) {
        return;
    }
    const uint32_t index = frameCtx.swapchainImageIndex;
    if (index >= swapchainLayouts_.size()) {
        return;
    }

    frameCtx.commandBuffer.textureMemoryBarrier(Gpu::TextureMemoryBarrierOptions{
        .srcStages = Gpu::PipelineStageFlagBit::ColorAttachmentOutputBit,
        .srcMask = Gpu::AccessFlagBit::ColorAttachmentWriteBit,
        .dstStages = Gpu::PipelineStageFlagBit::BottomOfPipeBit,
        .dstMask = Gpu::AccessFlagBit::None,
        .oldLayout = Gpu::TextureLayout::ColorAttachmentOptimal,
        .newLayout = Gpu::TextureLayout::PresentSrc,
        .texture = frameCtx.swapchainImage->handle(),
        .range = {.aspectMask = Gpu::TextureAspectFlagBits::ColorBit,
                  .baseMipLevel = 0,
                  .levelCount = 1,
                  .baseArrayLayer = 0,
                  .layerCount = 1},
    });

    swapchainLayouts_[index] = Gpu::TextureLayout::PresentSrc;
}

void DynamicPipelineApplication::transitionDepthAttachmentForRender(Cory::FrameContext &frameCtx)
{
    if (depthLayouts_.empty()) {
        return;
    }
    const uint32_t index = frameCtx.swapchainImageIndex;
    if (index >= depthLayouts_.size()) {
        return;
    }

    const auto oldLayout = depthLayouts_[index];
    const auto srcStages = oldLayout == Gpu::TextureLayout::DepthStencilAttachmentOptimal
                               ? (Gpu::PipelineStageFlagBit::EarlyFragmentTestBit |
                                  Gpu::PipelineStageFlagBit::LateFragmentTestBit)
                               : Gpu::PipelineStageFlagBit::TopOfPipeBit;
    const auto srcMask = oldLayout == Gpu::TextureLayout::DepthStencilAttachmentOptimal
                             ? Gpu::AccessFlagBit::DepthStencilAttachmentWriteBit
                             : Gpu::AccessFlagBit::None;

    frameCtx.commandBuffer.textureMemoryBarrier(Gpu::TextureMemoryBarrierOptions{
        .srcStages = srcStages,
        .srcMask = srcMask,
        .dstStages = Gpu::PipelineStageFlagBit::EarlyFragmentTestBit |
                     Gpu::PipelineStageFlagBit::LateFragmentTestBit,
        .dstMask = Gpu::AccessFlagBit::DepthStencilAttachmentWriteBit,
        .oldLayout = oldLayout,
        .newLayout = Gpu::TextureLayout::DepthStencilAttachmentOptimal,
        .texture = frameCtx.depthImage->handle(),
        .range = {.aspectMask =
                      Gpu::TextureAspectFlagBits::DepthBit | Gpu::TextureAspectFlagBits::StencilBit,
                  .baseMipLevel = 0,
                  .levelCount = 1,
                  .baseArrayLayer = 0,
                  .layerCount = 1},
    });

    depthLayouts_[index] = Gpu::TextureLayout::DepthStencilAttachmentOptimal;
}

uint32_t DynamicPipelineApplication::decodeSampleCount(Gpu::SampleCountFlagBits flag)
{
    switch (flag) {
    case Gpu::SampleCountFlagBits::Samples2Bit:
        return 2;
    case Gpu::SampleCountFlagBits::Samples4Bit:
        return 4;
    case Gpu::SampleCountFlagBits::Samples8Bit:
        return 8;
    case Gpu::SampleCountFlagBits::Samples16Bit:
        return 16;
    case Gpu::SampleCountFlagBits::Samples32Bit:
        return 32;
    case Gpu::SampleCountFlagBits::Samples64Bit:
        return 64;
    default:
        return 1;
    }
}

double DynamicPipelineApplication::now() const
{
    return std::chrono::duration<double>(
               std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

double DynamicPipelineApplication::getElapsedTimeSeconds() const
{
    return now() - startupTime_;
}
