#include "VolumeRenderDemo.hpp"

#include "Common.hpp"
#include "VolumeRenderSystem.hpp"

#include <Cory/Application/CameraLayer.hpp>
#include <Cory/Application/ImGuiLayer.hpp>
#include <Cory/Application/LayerStack.hpp>
#include <Cory/Application/Window.hpp>
#include <Cory/Base/GlmUtils.hpp>
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
#include <Cory/Renderer/HeadlessFrameSource.hpp>
#include <Cory/Systems/TransformSystem.hpp>

#include <CLI/App.hpp>
#include <CLI/CLI.hpp>
#include <GLFW/glfw3.h>
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
    const int msaaSamples = 2; // Or use window_->samples() after window creation if needed
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

    const auto viewportDimensions =
        headless_ ? glm::i32vec2(headlessFrames_->extent()) : window_->dimensions();
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

    sceneGraph_.createEntityWithComponents(root,
                                           "Main Volume",
                                           Cory::Components::Transform{
                                               .mode = Cory::Components::TransformMode::Local,
                                               .position = {0.0f, 0.0f, 0.0f},
                                               .rotation = {0.0, 0.0, 0.0},
                                               .scale = {1.0f, 1.0f, 1.0f},
                                           },
                                           VolumeComponent{
                                               .size = {5.0f, 5.0f, 5.0f},
                                           });

    sceneGraph_.createEntityWithComponents(root,
                                           "Secondary Volume",
                                           Cory::Components::Transform{
                                               .mode = Cory::Components::TransformMode::Local,
                                               .position = {5.0f, 2.0f, 0.0f},
                                               .rotation = {30.0, 45.0, 0.0},
                                               .scale = {1.0f, 1.0f, 1.0f},
                                           },
                                           VolumeComponent{
                                               .size = {2.0f, 4.0f, 2.0f},
                                           });
}

void VolumeRenderDemoApplication::setupSystems()
{
    using Cory::Components::CameraComponent;
    // set up a system to update the camera from the camera manipulator
    systems_.emplace<Cory::CallbackSystem<CameraComponent>>(
        [this](Cory::SceneGraph &sg, Cory::TickInfo tick, Cory::Entity e, CameraComponent &c) {
            c.position = cameraLayer_->position();
            c.direction = cameraLayer_->focus() - c.position;
            c.viewMatrix = cameraLayer_->worldToViewMatrix();
        });

    // Rotate all transforms slowly around the Y axis to introduce some motion
    systems_.emplace<Cory::CallbackSystem<Cory::Components::Transform>>(
        [](Cory::SceneGraph &, Cory::TickInfo tick, Cory::Entity, Cory::Components::Transform &t) {
            t.rotation += glm::vec3{0.0f, 1.0f * tick.delta.count(), 0.0f};
        });

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
    // one framegraph for each frame in flight
    std::vector<Cory::Framegraph> framegraphs;
    uint32_t idx = 0;
    std::generate_n(std::back_inserter(framegraphs), Cory::MAX_FRAMES_IN_FLIGHT, [&]() {
        return Cory::Framegraph(ctx(), framegraphResources, idx++);
    });

    auto time = Cory::AppClock::now();
    auto runFrame = [&](Cory::FrameContext &frameCtx) {
        if (!headless_) {
            processEvents(0);
            glfwPollEvents();
        }

        // Update time
        auto previousFrameTime = std::exchange(time, Cory::AppClock::now());
        auto delta = time - previousFrameTime;

        if (!headless_) {
            // Update layers
            layers().update(Cory::LogicUpdateContext{
                .simulationTime = std::chrono::duration(time.time_since_epoch()).count(),
                .deltaTime = delta.count(),
            });
        }

        if (!headless_) {
            drawImguiControls();
        }
        // tick the components
        auto tickInfo = clock_.tick();
        systems_.tick(sceneGraph_, tickInfo);

        Cory::Framegraph &fg = framegraphs[frameCtx.inFlightIndex];
        // retire old resources from the last time this framegraph was
        // used - our frame synchronization ensures that the resources
        // are no longer in use
        fg.resetForNextFrame(frameCtx.frameNumber);

        defineRenderPasses(fg, frameCtx);

        auto execInfo = fg.record(frameCtx);

        if (dumpNextFramegraph_) {
            CO_APP_INFO(fg.dump(execInfo));
            dumpNextFramegraph_ = false;
        }
    };

    auto frames = headless_ ? headlessFrames_->frames() : window_->frames();
    for (auto &frameCtx : frames) {
        runFrame(frameCtx);
        if (framesToRender_ > 0 && frameCtx.frameNumber >= framesToRender_) {
            break;
        }
    }

    // wait until last frame is finished rendering
    ctx().device().waitUntilIdle();
}

void VolumeRenderDemoApplication::defineRenderPasses(Cory::Framegraph &framegraph,
                                                     const Cory::FrameContext &frameCtx)
{
    const Cory::ScopeTimer s{"Frame/DeclarePasses"};

    auto frameHandles = framegraph.importFrameContext(frameCtx);

    auto mainPass = volumeRenderer_->cubeRenderTask(
        framegraph.declareTask("TASK_Cubes"), frameHandles.colorImage, frameHandles.depthImage);

    auto layersOutput = layers().declareRenderTasks(
        framegraph, {.color = mainPass.output().colorOut, .depth = mainPass.output().depthOut});

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
    }
    ImGui::End();

    if (ImGui::Begin("Camera")) {
        glm::vec3 position = cameraLayer_->position();
        glm::vec3 center = cameraLayer_->focus();
        glm::vec3 up = cameraLayer_->up();
        glm::mat4 mat = glm::transpose(cameraLayer_->worldToViewMatrix());

        bool changed = CoImGui::Input("position", position, "%.3f");
        changed = CoImGui::Input("center", center, "%.3f") || changed;
        changed = CoImGui::Input("up", up, "%.3f") || changed;

        // if (changed) { camera_.lookAt(position, center, up); }
        if (ImGui::CollapsingHeader("View Matrix")) {
            CoImGui::Input("Row 0", mat[0], "%.3f", ImGuiInputTextFlags_ReadOnly);
            CoImGui::Input("Row 1", mat[1], "%.3f", ImGuiInputTextFlags_ReadOnly);
            CoImGui::Input("Row 2", mat[2], "%.3f", ImGuiInputTextFlags_ReadOnly);
            CoImGui::Input("Row 3", mat[3], "%.3f", ImGuiInputTextFlags_ReadOnly);
        }
    }
    ImGui::End();

    if (ImGui::Begin("Profiling")) {
        auto records = Cory::Profiler::GetRecords();

        CoImGui::drawProfilerRecords(records);
    }
    ImGui::End();
}
