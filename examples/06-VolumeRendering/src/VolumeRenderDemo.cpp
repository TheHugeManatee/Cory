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
#include <fmt/format.h>
#include <imgui.h>

#include <gsl/gsl>
#include <gsl/narrow>

#include <algorithm>
#include <vector>

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

    static constexpr auto WINDOW_SIZE = glm::i32vec2{1024, 1024};
    if (headless_) {
        ctx().setupHeadlessDevice();
        headlessFrames_ = std::make_unique<Cory::HeadlessFrameSource>(
            ctx(),
            Cory::HeadlessFrameSourceCreateInfo{
                .label = "ParticleCompute-Headless",
                .size = Cory::glmu::u32vec2::from(WINDOW_SIZE),
                .samples = static_cast<Gpu::SampleCountFlagBits>(msaaSamples()),
            });
    }
    else {
        window_ = std::make_unique<Cory::Window>(
            ctx(), WINDOW_SIZE, "Particle Compute Demo", msaaSamples());
        msaaSamples = static_cast<int32_t>(window_->sampleCount());
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
                    .densityMax = 0.20f,
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

    // // Rotate all transforms slowly around the Y axis to introduce some motion
    // systems_.emplace<Cory::CallbackSystem<Cory::Components::Transform>>(
    //     [](Cory::SceneGraph &, Cory::TickInfo tick, Cory::Entity, Cory::Components::Transform &t)
    //     {
    //         const float deltaYaw = static_cast<float>(0.1 * tick.delta.count());
    //         const auto delta = glm::angleAxis(deltaYaw, glm::vec3{0.0f, 1.0f, 0.0f});
    //         t.orientation = glm::normalize(delta * t.orientation);
    //     });

    // render ImGuizmo handles for all transform components before propagation to world space
    imguizmoSystem_ = &systems_.emplace<Cory::ImGuizmoTransformSystem>();
    imguizmoSystem_->setEnabled(showImGuizmo.get());

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
    resourceManager_ = &framegraphResources;
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
            imguizmoSystem_->setEnabled(showImGuizmo.get());
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
    resourceManager_ = nullptr;
}

void VolumeRenderDemoApplication::ensureTemporalHistoryTexture(const Cory::FrameContext &frameCtx)
{
    CO_CORE_ASSERT(resourceManager_ != nullptr,
                   "FramegraphResourceManager must be available before declaring temporal history");

    const auto extent = frameCtx.extent;
    const auto format = frameCtx.colorFormat;
    const auto sampleCount = frameCtx.sampleCount;
    const bool needsRecreate =
        !temporalHistory_.resource.valid() || temporalHistory_.extent != extent ||
        temporalHistory_.format != format || temporalHistory_.sampleCount != sampleCount;
    if (!needsRecreate) {
        return;
    }

    auto historyTexture = resourceManager_->declareTexture(Cory::TextureInfo{
        .name = "TEX_VolumeTemporalHistory",
        .size = glm::uvec3{extent, 1u},
        .format = format,
        .usage = Gpu::TextureUsageFlagBits::StorageBit | Gpu::TextureUsageFlagBits::TransferSrcBit |
                 Gpu::TextureUsageFlagBits::TransferDstBit,
        .sampleCount = sampleCount,
        .textureType = Gpu::TextureType::TextureType2D,
    });
    resourceManager_->allocate(std::vector<Cory::FramegraphTextureHandle>{historyTexture});
    temporalHistory_.resource = historyTexture;
    temporalHistory_.extent = extent;
    temporalHistory_.format = format;
    temporalHistory_.sampleCount = sampleCount;
    temporalHistory_.valid = false;
    temporalHistory_.forceTemporalReset = true;
}

void VolumeRenderDemoApplication::defineRenderPasses(Cory::Framegraph &framegraph,
                                                     const Cory::FrameContext &frameCtx)
{
    const Cory::ScopeTimer s{"Frame/DeclarePasses"};

    auto frameHandles = framegraph.importFrameContext(frameCtx);

    Cory::TransientTextureHandle colorForLayers = frameHandles.colorImage;
    auto depthForLayers = frameHandles.depthImage;

    if (debugRasterize.get()) {
        temporalHistory_.resource = {};
        temporalHistory_.valid = false;
        auto rasterization = volumeRenderer_->rasterizationTask(
            framegraph.declareTask("TASK_Cubes"), colorForLayers, depthForLayers);
        colorForLayers = rasterization.output().colorOut;
        depthForLayers = rasterization.output().depthOut;
    }
    else if (debugRaycast.get()) {
        temporalHistory_.resource = {};
        temporalHistory_.valid = false;
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
        ensureTemporalHistoryTexture(frameCtx);
        auto clearAttachments = Cory::StandardRenderTasks::clearAttachments(
            framegraph.declareTask("TASK_ClearAttachments"),
            frameHandles.colorImage,
            frameHandles.depthImage);
        CO_CORE_ASSERT(temporalHistory_.resource.valid(),
                       "Temporal history resource expected to be valid");
        auto temporalHistory =
            framegraph.declareInput(Cory::TransientTextureHandle{temporalHistory_.resource});
        auto volumeGeneration =
            volumeRenderer_->volumeGenerationTask(framegraph.declareTask("TASK_VolumeGenerate"));

        const float temporalAlpha = std::clamp(temporalAccumulationAlpha.get(), 0.01f, 1.0f);
        const bool applyTemporal = temporalAccumulation.get() && temporalHistory_.valid &&
                                   !temporalHistory_.forceTemporalReset;
        const float blendFactor = applyTemporal ? temporalAlpha : 1.0f;

        auto raycastResult =
            volumeRenderer_
                ->cubeRaycastTask(framegraph.declareTask("TASK_VolumeRaycast"),
                                  temporalHistory,
                                  clearAttachments.output().depth.value_or(depthForLayers),
                                  volumeGeneration.output(),
                                  blendFactor,
                                  iterations(),
                                  alphaDeltaRejectThreshold())
                .output();

        colorForLayers = raycastResult;
        depthForLayers = clearAttachments.output().depth.value_or(depthForLayers);
        temporalHistory_.valid = true;
        temporalHistory_.forceTemporalReset = false;
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
            temporalHistory_.forceTemporalReset = true;
            temporalHistory_.valid = false;
        }

        CoImGui::CheckBox("Debug Rasterizer", debugRasterize);
        CoImGui::CheckBox("Debug Raycast", debugRaycast);
        CoImGui::CheckBox("Show ImGuizmo", showImGuizmo);
        if (!headless_ && window_ != nullptr) {
            ImGui::Separator();
            CoImGui::Text("Raster");

            const int32_t currentMsaa = static_cast<int32_t>(window_->sampleCount());
            auto selectedMsaa = currentMsaa;
            const auto supportedMsaaSamples = window_->supportedSampleCounts();
            if (ImGui::BeginCombo("MSAA Samples", fmt::format("{}x", currentMsaa).c_str())) {
                for (const auto sampleCountBits : supportedMsaaSamples) {
                    const auto sampleCount = static_cast<int32_t>(sampleCountBits);
                    const bool isSelected = sampleCount == currentMsaa;
                    if (ImGui::Selectable(fmt::format("{}x", sampleCount).c_str(), isSelected)) {
                        selectedMsaa = sampleCount;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            if (selectedMsaa != currentMsaa) {
                window_->requestSampleCount(static_cast<Gpu::SampleCountFlagBits>(selectedMsaa));
                msaaSamples = static_cast<int32_t>(window_->sampleCount());
                temporalHistory_.valid = false;
                temporalHistory_.forceTemporalReset = true;
            }
        }

        ImGui::Separator();
        CoImGui::Text("Temporal Accumulation");
        CoImGui::CheckBox("Enable Temporal", temporalAccumulation);
        auto temporalAlpha = temporalAccumulationAlpha.get();
        auto alphaRejectThreshold = alphaDeltaRejectThreshold.get();
        CoImGui::Slider("Iterations", iterations, 1, 200);
        CoImGui::Slider("Temporal Alpha", temporalAlpha, 0.01f, 1.0f);
        CoImGui::Slider("Alpha Reject Threshold", alphaRejectThreshold, 0.0f, 0.25f);
        temporalAccumulationAlpha = std::clamp(temporalAlpha, 0.01f, 1.0f);
        alphaDeltaRejectThreshold = std::max(alphaRejectThreshold, 0.0f);
        if (ImGui::Button("Reset Temporal")) {
            temporalHistory_.forceTemporalReset = true;
            temporalHistory_.valid = false;
        }

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
