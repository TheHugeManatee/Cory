#include "SceneGraphDemo.hpp"

#include "Common.hpp"
#include "CubeAnimationSystem.hpp"
#include "CubeRenderSystem.hpp"

#include <Cory/Application/ImGuiLayer.hpp>
#include <Cory/Application/LayerStack.hpp>
#include <Cory/Application/Window.hpp>
#include <Cory/Base/Random.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Cory.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/ImGui/Inputs.hpp>
#include <Cory/ImGui/Widgets.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Systems/TransformSystem.hpp>

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/Reference.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Vk/CommandBuffer.h>
#include <Magnum/Vk/Device.h>
#include <Magnum/Vk/DeviceProperties.h>
#include <Magnum/Vk/Mesh.h>
#include <Magnum/Vk/PipelineLayout.h>
#include <Magnum/Vk/Queue.h>
#include <Magnum/Vk/RenderPass.h>
#include <Magnum/Vk/SamplerCreateInfo.h>
#include <Magnum/Vk/VertexFormat.h>

#include <CLI/App.hpp>
#include <CLI/CLI.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>

#include <gsl/gsl>
#include <gsl/narrow>

#include <algorithm>

namespace Vk = Magnum::Vk;

SceneGraphDemoApplication::SceneGraphDemoApplication(std::span<const char *> args)
{
    CLI::App app{"SceneGraphDemo"};
    bool disableValidation{false};
    app.add_option("-f,--frames", framesToRender_, "The number of frames to render");
    app.add_flag("--disable-validation", disableValidation, "Disable validation layers");
    app.allow_config_extras(true);
    app.parse(gsl::narrow<int>(args.size()), args.data());

    Cory::ResourceLocator::addSearchPath(SCENEGRAPHDEMO_RESOURCE_DIR);

    init(Cory::ContextCreationInfo{
        .validation =
            disableValidation ? Cory::ValidationLayers::Disabled : Cory::ValidationLayers::Enabled,
        .args = args,
    });

    // determine msaa sample count to use - for simplicity, we use either 8 or one sample
    const auto &limits = ctx().physicalDevice().properties().properties.limits;
    const VkSampleCountFlags counts =
        limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts;
    // 2 samples are guaranteed to be supported, but we'd rather have 8
    const int msaaSamples = counts & VK_SAMPLE_COUNT_8_BIT ? 8 : 2;
    CO_APP_INFO("MSAA sample count: {}", msaaSamples);

    CO_APP_INFO("Vulkan instance version is {}", Cory::queryVulkanInstanceVersion());
    static constexpr auto WINDOW_SIZE = glm::i32vec2{1024, 1024};
    window_ = std::make_unique<Cory::Window>(ctx(), WINDOW_SIZE, "SceneGraphDemo", msaaSamples);

    camera_.setMode(Cory::CameraManipulator::Mode::Fly);
    camera_.setWindowSize(window_->dimensions());
    camera_.setLookat({0.0f, 3.0f, 2.5f}, {0.0f, 4.0f, 2.0f}, {0.0f, 1.0f, 0.0f});
    setupCameraCallbacks();

    setupSystems();

    Cory::LayerAttachInfo layerAttachInfo{.maxFramesInFlight =
                                              window_->swapchain().maxFramesInFlight(),
                                          .viewportDimensions = window_->dimensions()};
    layers().emplacePriorityLayer<Cory::ImGuiLayer>(layerAttachInfo, std::ref(*window_));
}
void SceneGraphDemoApplication::setupSystems()
{
    animationSystem_ = &systems_.emplace<CubeAnimationSystem>();

    // set up the camera updates
    Cory::Entity camera = sceneGraph_.createEntity(sceneGraph_.root(), "camera");
    sceneGraph_.addComponent<CameraComponent>(
        camera, CameraComponent{.fovy = glm::radians(70.0f), .nearPlane = 1.0f, .farPlane = 10.0f});

    // set up a system to update the camera from the camera manipulator
    systems_.emplace<Cory::CallbackSystem<CameraComponent>>(
        [this](Cory::SceneGraph &sg, Cory::TickInfo tick, Cory::Entity e, CameraComponent &c) {
            c.position = camera_.getCameraPosition();
            c.direction = camera_.getCenterPosition() - c.position;
            c.viewMatrix = camera_.getViewMatrix();
        });

    // after the "logic" has updated, sync all the transforms of the scenegraph
    systems_.emplace<Cory::TransformSystem>();

    // render system should go last to be aware of the latest state
    renderSystem_ =
        &systems_.emplace<CubeRenderSystem>(ctx(), window_->swapchain().maxFramesInFlight());
}

SceneGraphDemoApplication::~SceneGraphDemoApplication()
{
    CO_APP_TRACE("Destroying SceneGraphDemoApplication");
}

void SceneGraphDemoApplication::run()
{
    // one framegraph for each frame in flight
    std::vector<Cory::Framegraph> framegraphs;
    std::generate_n(std::back_inserter(framegraphs),
                    window_->swapchain().maxFramesInFlight(),
                    [&]() { return Cory::Framegraph(ctx()); });

    while (!window_->shouldClose()) {
        glfwPollEvents();

        layers().update();
        drawImguiControls();
        // tick the components
        auto tickInfo = clock_.tick();
        systems_.tick(sceneGraph_, tickInfo);

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
    ctx().device()->DeviceWaitIdle(ctx().device());
}

void SceneGraphDemoApplication::defineRenderPasses(Cory::Framegraph &framegraph,
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

    auto mainPass = renderSystem_->cubeRenderTask(
        framegraph.declareTask("TASK_Cubes"), windowColorTarget, windowDepthTarget);

    auto layersOutput = layers().declareRenderTasks(
        framegraph, {.color = mainPass.output().colorOut, .depth = mainPass.output().depthOut});

    framegraph.declareOutput(layersOutput.color);
}

void SceneGraphDemoApplication::drawImguiControls()
{
    const Cory::ScopeTimer st{"Frame/ImGui"};

    if (ImGui::Begin("Demo")) {
        if (ImGui::Button("Dump Framegraph")) { dumpNextFramegraph_ = true; }
        CoImGui::Text("Time: {:.3f}, Frame: {}",
                      clock_.lastTick().now.time_since_epoch().count(),
                      clock_.lastTick().ticks);
        if (ImGui::Button("Restart")) { clock_.reset(); }
    }
    ImGui::End();

    animationSystem_->drawImguiControls();
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

        CoImGui::drawProfilerRecords(records);
    }
    ImGui::End();
}

void SceneGraphDemoApplication::setupCameraCallbacks()
{
    window_->onSwapchainResized.connect([this](Cory::SwapchainResizedEvent event) {
        layers().onEvent(event);
        camera_.setWindowSize(event.size);
    });

    window_->onMouseMoved.connect([this](Cory::MouseMovedEvent event) {
        if (layers().onEvent(event)) { return; }
        if (event.button != Cory::MouseButton::None) {
            camera_.mouseMove(glm::ivec2(event.position), event.button, event.modifiers);
        }
    });
    window_->onMouseButton.connect([this](Cory::MouseButtonEvent event) {
        if (layers().onEvent(event)) { return; }
        camera_.setMousePosition(event.position);
    });
    window_->onMouseScrolled.connect([this](Cory::ScrollEvent event) {
        if (layers().onEvent(event)) { return; }
        camera_.wheel(static_cast<int32_t>(event.scrollDelta.y));
    });
}
