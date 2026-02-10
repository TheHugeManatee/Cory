#include "VolumeRenderDemo.hpp"

#include "Common.hpp"
#include "VolumeRenderSystem.hpp"

#include <Cory/Application/CameraLayer.hpp>
#include <Cory/Application/ImGuiLayer.hpp>
#include <Cory/Application/LayerStack.hpp>
#include <Cory/Application/Window.hpp>
#include <Cory/Base/FileWatchManager.hpp>
#include <Cory/Base/GlmUtils.hpp>
#include <Cory/Base/Math.hpp>
#include <Cory/Base/Random.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Base/Time.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/Framegraph/FramegraphResourceManager.hpp>
#include <Cory/ImGui/Inputs.hpp>
#include <Cory/ImGui/Widgets.hpp>
#include <Cory/RenderTasks/StandardRenderTasks.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/FrameSource.hpp>
#include <Cory/Renderer/HeadlessFrameSource.hpp>
#include <Cory/Renderer/ShaderManager.hpp>
#include <Cory/Systems/ImGuizmoTransformSystem.hpp>
#include <Cory/Systems/TransformSystem.hpp>

#include <CLI/App.hpp>
#include <CLI/CLI.hpp>
#include <imgui.h>

#include <gsl/gsl>
#include <gsl/narrow>

#include <algorithm>

VolumeRenderDemoApplication::VolumeRenderDemoApplication(std::span<const char *> args)
{
    CLI::App app{"VolumeRenderDemoApplication"};
    bool disableValidation{false};
    app.add_option("-f,--frames", framesToRender_, "The number of frames to render");
    app.add_flag("--disable-validation", disableValidation, "Disable validation layers");
    app.add_flag("--headless", headless_, "Run without a window and render offscreen");
    app.allow_config_extras(true);
    app.parse(gsl::narrow<int>(args.size()), args.data());

    Cory::ResourceLocator::addSearchPath(VOLUMERENDERING_RESOURCE_DIR);

    init(Cory::ContextCreationInfo{
        .validation =
            disableValidation ? Cory::ValidationLayers::Disabled : Cory::ValidationLayers::Enabled,
        .args = args,
    });

    // Use Cory API for MSAA sample count
    const int msaaSamples = 1; // Or use window_->samples() after window creation if needed
    static constexpr auto WINDOW_SIZE = glm::i32vec2{1024, 1024};
    if (headless_) {
        ctx().setupHeadlessDevice();
        headlessFrames_ = std::make_unique<Cory::HeadlessFrameSource>(
            ctx(),
            Cory::HeadlessFrameSourceCreateInfo{
                .label = "ParticleCompute-Headless",
                .size = Cory::glmu::u32vec2::from(WINDOW_SIZE),
                .samples = static_cast<Gpu::SampleCountFlagBits>(msaaSamples),
            });
    }
    else {
        window_ = std::make_unique<Cory::Window>(
            ctx(), WINDOW_SIZE, "Particle Compute Demo", msaaSamples);
    }

    setupScene();
    setupSystems();

    auto &frameSource = headless_ ? static_cast<Cory::FrameSource &>(*headlessFrames_)
                                  : static_cast<Cory::FrameSource &>(*window_);
    const auto viewportDimensions = glm::i32vec2(frameSource.extent());
    Cory::LayerAttachInfo layerAttachInfo{.maxFramesInFlight = Cory::MAX_FRAMES_IN_FLIGHT,
                                          .viewportDimensions = viewportDimensions};
    cameraLayer_ = &layers().addLayer<Cory::CameraLayer>(layerAttachInfo);
    if (!headless_) {
        layers().emplacePriorityLayer<Cory::ImGuiLayer>(layerAttachInfo, std::ref(*window_));
        layers().connectToWindow(*window_);
    }
}

void VolumeRenderDemoApplication::setupScene()
{
    // set up the camera updates
    Cory::Entity root = sceneGraph_.root();
    Cory::Entity camera = sceneGraph_.createEntity(root, "camera");
    sceneGraph_.addComponent<Cory::Components::CameraComponent>(
        camera,
        Cory::Components::CameraComponent{.viewMatrix = glm::mat4{1.0f},
                                          .position = {0.0f, 0.0f, 1.0f},
                                          .direction = {0.0f, 0.0f, -1.0f},
                                          .fovy = glm::radians(55.0f),
                                          .nearPlane = 0.2f,
                                          .farPlane = 1000.0f});

    sceneGraph_.createEntityWithComponents(
        root,
        "Main Volume",
        Cory::Components::Transform{
            .mode = Cory::Components::TransformMode::Local,
            .position = {0.0f, 0.0f, 0.0f},
            .orientation = Cory::eulerYXZToQuaternion({0.0, 0.0, 0.0}),
            .scale = {1.0f, 1.0f, 1.0f},
        },
        VolumeComponent{
            .size = {5.0f, 5.0f, 5.0f},
            .raymarchStepSizeMultiplier = 4.0f,
            .transferFunction =
                {
                    .densityMin = 0.05f,
                    .densityMax = 0.80f,
                    .opacityScale = 30.0f,
                    .gamma = 0.9f,
                },
        });

    sceneGraph_.createEntityWithComponents(
        root,
        "Secondary Volume",
        Cory::Components::Transform{
            .mode = Cory::Components::TransformMode::Local,
            .position = {5.0f, 2.0f, 0.0f},
            .orientation = Cory::eulerYXZToQuaternion({30.0, 45.0, 0.0}),
            .scale = {1.0f, 1.0f, 1.0f},
        },
        VolumeComponent{
            .size = {2.0f, 4.0f, 2.0f},
            .raymarchStepSizeMultiplier = 2.0f,
            .transferFunction =
                {
                    .densityMin = 0.25f,
                    .densityMax = 0.98f,
                    .opacityScale = 14.0f,
                    .gamma = 1.45f,
                },
        });
}

void VolumeRenderDemoApplication::setupSystems()
{
    using Cory::Components::CameraComponent;
    // set up a system to update the camera from the camera manipulator
    systems_.emplace<Cory::CallbackSystem<CameraComponent>>(
        [this]([[maybe_unused]] Cory::SceneGraph &sg,
               [[maybe_unused]] Cory::TickInfo tick,
               [[maybe_unused]] Cory::Entity e,
               CameraComponent &c) {
            c.position = cameraLayer_->position();
            c.direction = cameraLayer_->focus() - c.position;
            c.viewMatrix = cameraLayer_->worldToViewMatrix();
        });

    // Rotate all transforms slowly around the Y axis to introduce some motion
    systems_.emplace<Cory::CallbackSystem<Cory::Components::Transform>>(
        [](Cory::SceneGraph &, Cory::TickInfo tick, Cory::Entity, Cory::Components::Transform &t) {
            const float deltaYaw = static_cast<float>(0.1 * tick.delta.count());
            const auto delta = glm::angleAxis(deltaYaw, glm::vec3{0.0f, 1.0f, 0.0f});
            t.orientation = glm::normalize(delta * t.orientation);
        });

    // render ImGuizmo handles for all transform components before propagation to world space
    systems_.emplace<Cory::ImGuizmoTransformSystem>();

    // after the "logic" has updated, sync all the transforms of the scenegraph
    systems_.emplace<Cory::TransformSystem>();

    // render system should go last to be aware of the latest state
    volumeRenderer_ = &systems_.emplace<VolumeRenderSystem>(ctx());
}

VolumeRenderDemoApplication::~VolumeRenderDemoApplication()
{
    CO_APP_TRACE("Destroying VolumeRenderDemoApplication");
}

void VolumeRenderDemoApplication::run()
{
    Cory::FramegraphResourceManager framegraphResources{ctx()};
    auto framegraphs = createFramegraphs(framegraphResources);

    auto &frameSource = headless_ ? static_cast<Cory::FrameSource &>(*headlessFrames_)
                                  : static_cast<Cory::FrameSource &>(*window_);
    runMainLoop(
        frameSource,
        framesToRender_,
        {.headless = headless_,
         .pollPlatformEvents = true,
         .processFileWatchEvents = true,
         .clearDeferredShaderReleases = true},
        [this, &framegraphs](Cory::FrameContext &frameCtx, const Cory::LogicUpdateContext &) {
            auto tickInfo = clock_.tick();
            systems_.tick(sceneGraph_, tickInfo);

            auto recordedFrame = recordFramegraph(
                framegraphs,
                frameCtx,
                [this](Cory::Framegraph &fg, const Cory::FrameContext &currentFrame) {
                    defineRenderPasses(fg, currentFrame);
                });

            if (dumpNextFramegraph_) {
                dumpFramegraph(recordedFrame.framegraph,
                               recordedFrame.executionInfo,
                               "VolumeRenderDemo",
                               frameCtx.frameNumber);
                dumpNextFramegraph_ = false;
            }
        },
        [this](Cory::FrameContext &, const Cory::LogicUpdateContext &) { drawImguiControls(); });
}

void VolumeRenderDemoApplication::defineRenderPasses(Cory::Framegraph &framegraph,
                                                     const Cory::FrameContext &frameCtx)
{
    const Cory::ScopeTimer s{"Frame/DeclarePasses"};

    auto frameHandles = framegraph.importFrameContext(frameCtx);

    Cory::TransientTextureHandle colorForLayers = frameHandles.colorImage;
    auto depthForLayers = frameHandles.depthImage;

    if (debugRasterize.get()) {
        auto rasterization = volumeRenderer_->rasterizationTask(
            framegraph.declareTask("TASK_Cubes"), colorForLayers, depthForLayers);
        colorForLayers = rasterization.output().colorOut;
        depthForLayers = rasterization.output().depthOut;
    }
    else if (debugRaycast.get()) {
        auto clearAttachments = Cory::StandardRenderTasks::clearAttachments(
            framegraph.declareTask("TASK_ClearAttachments"),
            frameHandles.colorImage,
            frameHandles.depthImage);
        auto volumeGeneration =
            volumeRenderer_->volumeGenerationTask(framegraph.declareTask("TASK_VolumeGenerate"));

        auto debugRaycast = volumeRenderer_->cubeRaycastDebugTask(
            framegraph.declareTask("TASK_VolumeRaycastDebug"),
            clearAttachments.output().color,
            clearAttachments.output().depth.value_or(depthForLayers),
            volumeGeneration.output());
        colorForLayers = debugRaycast.output();
        depthForLayers = clearAttachments.output().depth.value_or(depthForLayers);
    }
    else {
        auto clearAttachments = Cory::StandardRenderTasks::clearAttachments(
            framegraph.declareTask("TASK_ClearAttachments"),
            frameHandles.colorImage,
            frameHandles.depthImage);
        auto volumeGeneration =
            volumeRenderer_->volumeGenerationTask(framegraph.declareTask("TASK_VolumeGenerate"));

        auto mainRaycast = volumeRenderer_->cubeRaycastTask(
            framegraph.declareTask("TASK_VolumeRaycast"),
            clearAttachments.output().color,
            clearAttachments.output().depth.value_or(depthForLayers),
            volumeGeneration.output());
        colorForLayers = mainRaycast.output();
        depthForLayers = clearAttachments.output().depth.value_or(depthForLayers);
    }

    auto layersOutput =
        layers().declareRenderTasks(framegraph, {.color = colorForLayers, .depth = depthForLayers});

    auto resolvedSwapchain =
        Cory::StandardRenderTasks::resolve(
            framegraph.declareTask("TASK_Resolve"), layersOutput.color, frameHandles.swapchainImage)
            .output();

    framegraph.declareOutput(resolvedSwapchain, Cory::Sync::AccessType::Present);
}

void VolumeRenderDemoApplication::drawImguiControls()
{
    const Cory::ScopeTimer st{"Frame/ImGui"};

    if (ImGui::Begin("Demo")) {
        if (ImGui::Button("Dump Framegraph")) {
            dumpNextFramegraph_ = true;
        }
        CoImGui::Text("Time: {:.3f}, Frame: {}",
                      clock_.lastTick().now.time_since_epoch().count(),
                      clock_.lastTick().ticks);
        if (ImGui::Button("Restart")) {
            clock_.reset();
        }

        CoImGui::CheckBox("Debug Rasterizer", debugRasterize);
        CoImGui::CheckBox("Debug Raycast", debugRaycast);

        ImGui::Separator();
        CoImGui::Text("Volume Transfer Functions");
        bool hasVolumeComponent = false;
        for (auto entity : sceneGraph_.depthFirstTraversal()) {
            auto *volume = sceneGraph_.getComponent<VolumeComponent>(entity);
            if (volume == nullptr) {
                continue;
            }
            hasVolumeComponent = true;
            const auto &meta = sceneGraph_.data(entity);
            ImGui::PushID(static_cast<int>(entity));
            if (ImGui::CollapsingHeader(meta.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                auto &tf = volume->transferFunction;
                ImGui::Checkbox("Enable Jitter", &volume->raymarchJitteringEnabled);
                CoImGui::Slider(
                    "Step Multiplier (vox)", volume->raymarchStepSizeMultiplier, 0.25f, 8.0f);
                CoImGui::Slider("Density Min", tf.densityMin, 0.0f, 1.0f);
                CoImGui::Slider("Density Max", tf.densityMax, 0.0f, 1.0f);
                CoImGui::Slider("Opacity Scale", tf.opacityScale, 0.01f, 64.0f);
                CoImGui::Slider("Gamma", tf.gamma, 0.05f, 3.0f);

                volume->raymarchStepSizeMultiplier =
                    std::max(volume->raymarchStepSizeMultiplier, 0.01f);
                tf.densityMin = std::clamp(tf.densityMin, 0.0f, 1.0f);
                tf.densityMax = std::clamp(tf.densityMax, tf.densityMin + 0.001f, 1.0f);
                tf.opacityScale = std::max(tf.opacityScale, 0.01f);
                tf.gamma = std::max(tf.gamma, 0.05f);
            }
            ImGui::PopID();
        }
        if (!hasVolumeComponent) {
            CoImGui::Text("No VolumeComponent found in scene.");
        }
    }
    ImGui::End();

    if (ImGui::Begin("Profiling")) {
        auto records = Cory::Profiler::GetRecords();

        CoImGui::drawProfilerRecords(records);
    }
    ImGui::End();
}
