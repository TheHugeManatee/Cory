#include "CubeDemo.hpp"

#include "Cory/Framegraph/TextureManager.hpp"
#include <Cory/Application/DepthDebugLayer.hpp>
#include <Cory/Application/DynamicGeometry.hpp>
#include <Cory/Application/ImGuiLayer.hpp>
#include <Cory/Application/Window.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Base/Math.hpp>
#include <Cory/Base/Profiling.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Cory.hpp>
#include <Cory/Framegraph/CommandList.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/ImGui/Inputs.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/ResourceManager.hpp>
#include <Cory/Renderer/Swapchain.hpp>

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/Reference.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Vk/BufferCreateInfo.h>
#include <Magnum/Vk/CommandBuffer.h>
#include <Magnum/Vk/DescriptorPool.h>
#include <Magnum/Vk/DescriptorSetLayout.h>
#include <Magnum/Vk/Device.h>
#include <Magnum/Vk/DeviceProperties.h>
#include <Magnum/Vk/FramebufferCreateInfo.h>
#include <Magnum/Vk/Mesh.h>
#include <Magnum/Vk/PipelineLayout.h>
#include <Magnum/Vk/Queue.h>
#include <Magnum/Vk/RenderPass.h>
#include <Magnum/Vk/SamplerCreateInfo.h>
#include <Magnum/Vk/VertexFormat.h>

#include <CLI/App.hpp>
#include <CLI/CLI.hpp>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <imgui.h>

#include <gsl/gsl>
#include <gsl/narrow>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>

#include <algorithm>
#include <chrono>

namespace Vk = Magnum::Vk;

struct PushConstants {
    glm::mat4 modelTransform{1.0f};
    glm::vec4 color{1.0, 0.0, 0.0, 1.0};
    float blend;
};

static struct AnimationData {
    int num_cubes{200};
    float blend{0.8f};

    float ti{1.5f};
    float tsi{2.0f};
    float tsf{100.0f};

    float r0{0.0f};
    float rt{-0.1f};
    float ri{1.3f};
    float rti{0.05f};

    float s0{0.05f};
    float st{0.0f};
    float si{0.4f};

    float c0{-0.75f};
    float cf0{2.0f};
    float cfi{-0.5f};

    glm::vec3 translation{0.0, 0.0f, 2.5f};
    glm::vec3 rotation{0.0f};
} ad;

void animate(PushConstants &d, float t, float i)
{

    const float angle = ad.r0 + ad.rt * t + ad.ri * i + ad.rti * i * t;
    const float scale = ad.s0 + ad.st * t + ad.si * i;

    const float tsf = ad.tsf / 2.0f + ad.tsf * sin(t / 10.0f);
    const glm::vec3 translation{sin(i * tsf) * i * ad.tsi, cos(i * tsf) * i * ad.tsi, i * ad.ti};

    d.modelTransform = Cory::makeTransform(ad.translation + translation,
                                           ad.rotation + glm::vec3{0.0f, angle, angle / 2.0f},
                                           glm::vec3{scale});

    const float colorFreq = 1.0f / (ad.cf0 + ad.cfi * i);
    const float brightness = i + 0.2f * abs(sin(t + i));
    const float r = ad.c0 * t * colorFreq;
    const glm::vec4 start{0.8f, 0.2f, 0.2f, 1.0f};
    const glm::mat4 cm = glm::rotate(
        glm::scale(glm::mat4{1.0f}, glm::vec3{brightness}), r, glm::vec3{1.0f, 1.0f, 1.0f});

    d.color = start * cm;
    d.blend = ad.blend;
}

CubeDemoApplication::CubeDemoApplication(int argc, char **argv)
    : mesh_{}
    , depthDebugLayer_{std::make_unique<Cory::DepthDebugLayer>()}
    , imguiLayer_{std::make_unique<Cory::ImGuiLayer>()}
    , startupTime_{now()}
{
    Cory::Init();

    CLI::App app{"CubeDemo"};
    app.add_option("-f,--frames", framesToRender_, "The number of frames to render");
    app.add_flag("--disable-validation", disableValidation_, "Disable validation layers");
    app.parse(argc, argv);

    Cory::ResourceLocator::addSearchPath(CUBEDEMO_RESOURCE_DIR);

    ctx_ = std::make_unique<Cory::Context>(disableValidation_ ? Cory::ValidationLayers::Disabled
                                                              : Cory::ValidationLayers::Enabled);
    // determine msaa sample count to use - for simplicity, we use either 8 or one sample
    const auto &limits = ctx_->physicalDevice().properties().properties.limits;
    const VkSampleCountFlags counts =
        limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts;
    // 2 samples are guaranteed to be supported, but we'd rather have 8
    const int msaaSamples = counts & VK_SAMPLE_COUNT_8_BIT ? 8 : 2;
    CO_APP_INFO("MSAA sample count: {}", msaaSamples);

    CO_APP_INFO("Vulkan instance version is {}", Cory::queryVulkanInstanceVersion());
    static constexpr auto WINDOW_SIZE = glm::i32vec2{1024, 1024};
    window_ = std::make_unique<Cory::Window>(*ctx_, WINDOW_SIZE, "CubeDemo", msaaSamples);

    createGeometry();
    createShaders();

    imguiLayer_->init(*window_, *ctx_);
    Cory::LayerAttachInfo layerAttachInfo{.maxFramesInFlight =
                                              window_->swapchain().maxFramesInFlight(),
                                          .viewportDimensions = window_->dimensions()};
    depthDebugLayer_->onAttach(*ctx_, layerAttachInfo);

    camera_.setMode(Cory::CameraManipulator::Mode::Fly);
    camera_.setWindowSize(window_->dimensions());
    camera_.setLookat({0.0f, 3.0f, 2.5f}, {0.0f, 4.0f, 2.0f}, {0.0f, 1.0f, 0.0f});
    setupCameraCallbacks();
    createUBO();
}

void CubeDemoApplication::createShaders()
{
    const Cory::ScopeTimer st{"Init/Shaders"};
    vertexShader_ = ctx_->resources().createShader(Cory::ResourceLocator::Locate("cube.vert"));
    fragmentShader_ = ctx_->resources().createShader(Cory::ResourceLocator::Locate("cube.frag"));
}

void CubeDemoApplication::createUBO()
{
    // create and initialize descriptor sets for each frame in flight
    const Cory::ScopeTimer st{"Init/UBO"};
    globalUbo_ = std::make_unique<Cory::UniformBufferObject<CubeUBO>>(
        *ctx_, window_->swapchain().maxFramesInFlight());
}

CubeDemoApplication::~CubeDemoApplication()
{
    auto &resources = ctx_->resources();
    resources.release(vertexShader_);
    resources.release(fragmentShader_);

    imguiLayer_->deinit(*ctx_);
    depthDebugLayer_->onDetach(*ctx_);
    CO_APP_TRACE("Destroying CubeDemoApplication");
}

void CubeDemoApplication::run()
{
    // one framegraph for each frame in flight
    std::vector<Cory::Framegraph> framegraphs;
    std::generate_n(std::back_inserter(framegraphs),
                    window_->swapchain().maxFramesInFlight(),
                    [&]() { return Cory::Framegraph(*ctx_); });

    while (!window_->shouldClose()) {
        glfwPollEvents();
        imguiLayer_->newFrame(*ctx_);
        // TODO process events?

        drawImguiControls();
        depthDebugLayer_->onUpdate();

        Cory::FrameContext frameCtx = window_->nextSwapchainImage();
        Cory::Framegraph &fg = framegraphs[frameCtx.index];
        // retire old resources from the last time this framegraph was
        // used - our frame synchronization ensures that the resources
        // are no longer in use
        fg.resetForNextFrame();

        defineRenderPasses(fg, frameCtx);
        frameCtx.commandBuffer->begin(Vk::CommandBufferBeginInfo{});
        auto execInfo = fg.record(frameCtx);

        frameCtx.commandBuffer->end();

        window_->submitAndPresent(frameCtx);

        if (dumpNextFramegraph_) {
            CO_APP_INFO(fg.dump(execInfo));
            dumpNextFramegraph_ = false;
        }

        // break if number of frames to render are reached
        if (framesToRender_ > 0 && frameCtx.frameNumber >= framesToRender_) { break; }
    }

    // wait until last frame is finished rendering
    ctx_->device()->DeviceWaitIdle(ctx_->device());
}

void CubeDemoApplication::defineRenderPasses(Cory::Framegraph &framegraph,
                                             const Cory::FrameContext &frameCtx)
{
    const Cory::ScopeTimer s{"Frame/DeclarePasses"};

    auto windowColorTarget =
        framegraph.declareInput({.name = "TEX_SwapCh_Color",
                                 .size = glm::u32vec3{window_->dimensions(), 1},
                                 .format = frameCtx.colorImage->format(),
                                 .sampleCount = window_->sampleCount()},
                                Cory::Sync::AccessType::None,
                                *frameCtx.colorImage,
                                *frameCtx.colorImageView);

    auto windowDepthTarget =
        framegraph.declareInput({.name = "TEX_SwapCh_Depth",
                                 .size = glm::u32vec3{window_->dimensions(), 1},
                                 .format = frameCtx.depthImage->format(),
                                 .sampleCount = window_->sampleCount()},
                                Cory::Sync::AccessType::None,
                                *frameCtx.depthImage,
                                *frameCtx.depthImageView);

    auto mainPass =
        cubeRenderTask(framegraph.declareTask("TASK_Cubes"), windowColorTarget, windowDepthTarget);

    auto depthDebugPass = depthDebugLayer_->renderTask(
        framegraph.declareTask("TASK_DepthDebug"),
        {.color = mainPass.output().colorOut, .depth = mainPass.output().depthOut});

    auto imguiPass = imguiRenderTask(
        framegraph.declareTask("TASK_ImGui"), depthDebugPass.output().color, frameCtx);

    auto [outInfo, outState] = framegraph.declareOutput(imguiPass.output().colorOut);
}

Cory::RenderTaskDeclaration<CubeDemoApplication::PassOutputs>
CubeDemoApplication::cubeRenderTask(Cory::RenderTaskBuilder builder,
                                    Cory::TransientTextureHandle colorTarget,
                                    Cory::TransientTextureHandle depthTarget)
{

    VkClearColorValue clearColor{0.0f, 0.0f, 0.0f, 1.0f};
    float clearDepth = 1.0f;

    auto [writtenColorHandle, colorInfo] =
        builder.write(colorTarget, Cory::Sync::AccessType::ColorAttachmentWrite);
    auto [writtenDepthHandle, depthInfo] =
        builder.write(depthTarget, Cory::Sync::AccessType::DepthStencilAttachmentWrite);

    auto cubePass = builder.declareRenderPass("PASS_Cubes")
                        .shaders({vertexShader_, fragmentShader_})
                        .attach(colorTarget,
                                VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR,
                                VK_ATTACHMENT_STORE_OP_STORE,
                                clearColor)
                        .attachDepth(depthTarget,
                                     VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR,
                                     VK_ATTACHMENT_STORE_OP_STORE,
                                     clearDepth)
                        .finish();

    co_yield PassOutputs{.colorOut = writtenColorHandle, .depthOut = writtenDepthHandle};

    /// ^^^^     DECLARATION      ^^^^
    Cory::RenderInput renderApi = co_await builder.finishDeclaration();
    /// vvvv  RENDERING COMMANDS  vvvv

    auto t = gsl::narrow_cast<float>(getElapsedTimeSeconds());

    cubePass.begin(*renderApi.cmd);

    PushConstants pushData{};

    float fovy = glm::radians(70.0f);
    float aspect = static_cast<float>(colorInfo.size.x) / static_cast<float>(colorInfo.size.y);
    glm::mat4 viewMatrix = camera_.getViewMatrix();
    glm::mat4 projectionMatrix = Cory::makePerspective(fovy, aspect, 0.1f, 10.0f);
    glm::mat4 viewProjection = projectionMatrix * viewMatrix;

    Cory::FrameContext &frameCtx = *renderApi.frameCtx;

    // update the uniform buffer
    CubeUBO &ubo = (*globalUbo_)[frameCtx.index];
    ubo.view = viewMatrix;
    ubo.projection = projectionMatrix;
    ubo.viewProjection = viewProjection;
    // need explicit flush otherwise the mapped memory is not synced to the GPU
    globalUbo_->flush(frameCtx.index);

    ctx_->descriptorSets()
        .write(Cory::DescriptorSets::SetType::Static, frameCtx.index, *globalUbo_)
        .flushWrites()
        .bind(renderApi.cmd->handle(), frameCtx.index, ctx_->defaultPipelineLayout());

    for (int idx = 0; idx < ad.num_cubes; ++idx) {
        float i = ad.num_cubes == 1
                      ? 1.0f
                      : static_cast<float>(idx) / static_cast<float>(ad.num_cubes - 1);

        animate(pushData, t, i);

        ctx_->device()->CmdPushConstants(renderApi.cmd->handle(),
                                         ctx_->defaultPipelineLayout(),
                                         VkShaderStageFlagBits::VK_SHADER_STAGE_ALL,
                                         0,
                                         sizeof(pushData),
                                         &pushData);

        // draw our triangle mesh
        renderApi.cmd->handle().draw(*mesh_);
    }

    cubePass.end(*renderApi.cmd);
}

Cory::RenderTaskDeclaration<CubeDemoApplication::PassOutputs>
CubeDemoApplication::imguiRenderTask(Cory::RenderTaskBuilder builder,
                                     Cory::TransientTextureHandle colorTarget,
                                     Cory::FrameContext frameCtx)
{
    auto [writtenColorHandle, colorInfo] =
        builder.readWrite(colorTarget, Cory::Sync::AccessType::ColorAttachmentWrite);

    co_yield PassOutputs{.colorOut = writtenColorHandle};
    Cory::RenderInput renderApi = co_await builder.finishDeclaration();

    // note - currently, we're letting imgui handle the final resolve and transition to
    // present_layout
    imguiLayer_->recordFrameCommands(*ctx_, frameCtx.index, renderApi.cmd->handle());
}

void CubeDemoApplication::createGeometry()
{
    const Cory::ScopeTimer st{"Init/Geometry"};
    mesh_ = std::make_unique<Vk::Mesh>(Cory::DynamicGeometry::createCube(*ctx_));
}
double CubeDemoApplication::now()
{
    return std::chrono::duration<double>(
               std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

double CubeDemoApplication::getElapsedTimeSeconds() const { return now() - startupTime_; }
void CubeDemoApplication::drawImguiControls()
{
    namespace CoImGui = Cory::ImGui;
    const Cory::ScopeTimer st{"Frame/ImGui"};

    if (ImGui::Begin("Animation Params")) {
        if (ImGui::Button("Dump Framegraph")) { dumpNextFramegraph_ = true; }
        if (ImGui::Button("Restart")) { startupTime_ = now(); }

        CoImGui::Input("Cubes", ad.num_cubes, 1, 10000);
        CoImGui::Slider("blend", ad.blend, 0.0f, 1.0f);
        CoImGui::Slider("translation", ad.translation, -3.0f, 3.0f);
        CoImGui::Slider("rotation", ad.rotation, -glm::pi<float>(), glm::pi<float>());

        CoImGui::Slider("ti", ad.ti, 0.0f, 10.0f);
        CoImGui::Slider("tsi", ad.tsi, 0.0f, 10.0f);
        CoImGui::Slider("tsf", ad.tsf, 0.0f, 250.0f);

        CoImGui::Slider("r0", ad.r0, -2.0f, 2.0f);
        CoImGui::Slider("rt", ad.rt, -2.0f, 2.0f);
        CoImGui::Slider("ri", ad.ri, -2.0f, 2.0f);
        CoImGui::Slider("rti", ad.rti, -2.0f, 2.0f);
        CoImGui::Slider("s0", ad.s0, 0.0f, 2.0f);
        CoImGui::Slider("st", ad.st, -0.1f, 0.1f);
        CoImGui::Slider("si", ad.si, 0.0f, 2.0f);
        CoImGui::Slider("c0", ad.c0, -2.0f, 2.0f);
        CoImGui::Slider("cf0", ad.cf0, -10.0f, 10.0f);
        CoImGui::Slider("cfi", ad.cfi, -2.0f, 2.0f);
    }
    ImGui::End();
    if (ImGui::Begin("Camera")) {
        glm::vec3 position = camera_.getCameraPosition();
        glm::vec3 center = camera_.getCenterPosition();
        glm::vec3 up = camera_.getUpVector();
        glm::mat4 mat = glm::transpose(camera_.getViewMatrix());

        bool changed = CoImGui::Input("position", position, "%.3f");
        changed = CoImGui::Input("center", center, "%.3f") || changed;
        changed = CoImGui::Input("up", up, "%.3f") || changed;

        if (changed) { camera_.setLookat(position, center, up); }

        if (ImGui::CollapsingHeader("View Matrix")) {
            CoImGui::Input("r0", mat[0], "%.3f", ImGuiInputTextFlags_ReadOnly);
            CoImGui::Input("r1", mat[1], "%.3f", ImGuiInputTextFlags_ReadOnly);
            CoImGui::Input("r2", mat[2], "%.3f", ImGuiInputTextFlags_ReadOnly);
            CoImGui::Input("r3", mat[3], "%.3f", ImGuiInputTextFlags_ReadOnly);
        }
    }
    ImGui::End();

    if (ImGui::Begin("Profiling")) {
        auto records = Cory::Profiler::GetRecords();

        auto to_ms = [](uint64_t ns) { return double(ns) / 1'000'000.0; };

        if (ImGui::BeginTable("Profiling", 5)) {

            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("min [ms]", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("max [ms]", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("avg [ms]", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("graph", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (auto [name, record] : records) {
                auto stats = record.stats();
                auto hist = record.history();
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                CoImGui::Text("{}", name);
                ImGui::TableNextColumn();
                CoImGui::Text("{:3.2f}", to_ms(stats.min));
                ImGui::TableNextColumn();
                CoImGui::Text("{:3.2f}", to_ms(stats.max));
                ImGui::TableNextColumn();
                CoImGui::Text("{:3.2f}", to_ms(stats.avg));
                ImGui::TableNextColumn();

                auto h = hist | ranges::views::transform([](auto v) { return float(v); }) |
                         ranges::to<std::vector>;
                ImGui::PlotLines(
                    "", h.data(), gsl::narrow<int>(h.size()), 0, nullptr, 0.0f, float(stats.max));
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}
void CubeDemoApplication::setupCameraCallbacks()
{
    window_->onSwapchainResized.connect(
        [this](Cory::SwapchainResizedEvent event) { camera_.setWindowSize(event.size); });

    window_->onMouseMoved.connect([this](Cory::MouseMovedEvent event) {
        if (ImGui::GetIO().WantCaptureMouse) { return; }

        if (depthDebugLayer_->onEvent(event)) { return; }
        if (event.button != Cory::MouseButton::None) {
            camera_.mouseMove(glm::ivec2(event.position), event.button, event.modifiers);
        }
    });
    window_->onMouseButton.connect([this](Cory::MouseButtonEvent event) {
        if (ImGui::GetIO().WantCaptureMouse) { return; }
        if (depthDebugLayer_->onEvent(event)) { return; }
        camera_.setMousePosition(event.position);
    });
    window_->onMouseScrolled.connect([this](Cory::ScrollEvent event) {
        if (ImGui::GetIO().WantCaptureMouse) { return; }
        if (depthDebugLayer_->onEvent(event)) { return; }
        camera_.wheel(static_cast<int32_t>(event.scrollDelta.y));
    });
}
