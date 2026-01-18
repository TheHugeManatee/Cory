#include "ParticleComputeDemo.hpp"

#include "Common.hpp"
#include "PointSpriteRenderSystem.hpp"

#include <Cory/Application/CameraLayer.hpp>
#include <Cory/Application/ImGuiLayer.hpp>
#include <Cory/Application/LayerStack.hpp>
#include <Cory/Application/Window.hpp>
#include <Cory/Base/GlmUtils.hpp>
#include <Cory/Base/Random.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Base/Time.hpp>
#include <Cory/Cory.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
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

ParticleComputeDemoApplication::ParticleComputeDemoApplication(std::span<const char *> args)
{
    CLI::App app{"ParticleComputeDemoApplication"};
    bool disableValidation{false};
    app.add_option("-f,--frames", framesToRender_, "The number of frames to render");
    app.add_flag("--disable-validation", disableValidation, "Disable validation layers");
    app.add_flag("--headless", headless_, "Run without a window and render offscreen");
    app.allow_config_extras(true);
    app.parse(gsl::narrow<int>(args.size()), args.data());

    Cory::ResourceLocator::addSearchPath(PARTICLECOMPUTEDEMO_RESOURCE_DIR);

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
        headless_ ? headlessFrames_->extent() : window_->dimensions();
    Cory::LayerAttachInfo layerAttachInfo{.maxFramesInFlight = Cory::MAX_FRAMES_IN_FLIGHT,
                                          .viewportDimensions = viewportDimensions};
    cameraLayer_ = &layers().addLayer<Cory::CameraLayer>(layerAttachInfo);
    if (!headless_) {
        layers().emplacePriorityLayer<Cory::ImGuiLayer>(layerAttachInfo, std::ref(*window_));
        layers().connectToWindow(*window_);
    }
}

void ParticleComputeDemoApplication::setupScene()
{
    // set up the camera updates
    Cory::Entity camera = sceneGraph_.createEntity(sceneGraph_.root(), "camera");
    sceneGraph_.addComponent<Cory::Components::CameraComponent>(
        camera,
        Cory::Components::CameraComponent{.viewMatrix = glm::mat4{1.0f},
                                          .position = {0.0f, 0.0f, 1.0f},
                                          .direction = {0.0f, 0.0f, -1.0f},
                                          .fovy = glm::radians(55.0f),
                                          .nearPlane = 0.2f,
                                          .farPlane = 1000.0f});

    Cory::Entity root = sceneGraph_.root();

    static constexpr size_t numSprites = 50000;
    static constexpr float sceneRadius = 15.0f;

    for (size_t i = 0; i < numSprites; ++i) {
        const float radius = Cory::RNG::Uniform(0.1f, 0.3f);
        auto pos = Cory::RNG::UniformInSphere() * sceneRadius;
        auto color = glm::vec4{Cory::RNG::Uniform(0.0f, 1.0f),
                               Cory::RNG::Uniform(0.0f, 1.0f),
                               Cory::RNG::Uniform(0.0f, 1.0f),
                               1.0f};

        sceneGraph_.createEntity(
            root,
            fmt::format("Sprite {}", i),
            PointSpriteComponent{.position = pos, .radius = radius, .color = color});
    }

    /// add a coordinate system indicator
    auto make_colored_axis =
        [&](std::string_view axis_name, glm::vec3 color, glm::vec3 axis, uint32_t steps) {
            // create entity with an AnimationComponent and a TransformComponent for each step
            for (uint32_t i = 0; i < steps / 2; ++i) {
                sceneGraph_.createEntity(root,
                                         fmt::format("{}{}", axis_name, i),
                                         PointSpriteComponent{
                                             .position = 0.2f * axis * static_cast<float>(i),
                                             .radius = 0.1f,
                                             .color = glm::vec4{color, 1.0f},
                                         });
            }
        };
    // create colored axes for X, Y and Z
    make_colored_axis("X", {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 100);
    make_colored_axis("Y", {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 100);
    make_colored_axis("Z", {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, 100);
}

void ParticleComputeDemoApplication::setupSystems()
{
    using Cory::Components::CameraComponent;
    // set up a system to update the camera from the camera manipulator
    systems_.emplace<Cory::CallbackSystem<CameraComponent>>(
        [this](Cory::SceneGraph &sg, Cory::TickInfo tick, Cory::Entity e, CameraComponent &c) {
            c.position = cameraLayer_->position();
            c.direction = cameraLayer_->focus() - c.position;
            c.viewMatrix = cameraLayer_->worldToViewMatrix();
        });

    // after the "logic" has updated, sync all the transforms of the scenegraph
    systems_.emplace<Cory::TransformSystem>();

    // render system should go last to be aware of the latest state
    renderSystem_ = &systems_.emplace<PointSpriteRenderSystem>(ctx());
}

ParticleComputeDemoApplication::~ParticleComputeDemoApplication()
{
    CO_APP_TRACE("Destroying ParticleComputeDemoApplication");
}

void ParticleComputeDemoApplication::run()
{
    // one framegraph for each frame in flight
    std::vector<Cory::Framegraph> framegraphs;
    uint32_t idx = 0;
    std::generate_n(std::back_inserter(framegraphs), Cory::MAX_FRAMES_IN_FLIGHT, [&]() {
        return Cory::Framegraph(ctx(), idx++);
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
        fg.resetForNextFrame();

        defineRenderPasses(fg, frameCtx);

        auto execInfo = fg.record(frameCtx);

        if (dumpNextFramegraph_) {
            CO_APP_INFO(fg.dump(execInfo));
            dumpNextFramegraph_ = false;
        }

    };

    if (headless_) {
        for (auto &frameCtx : headlessFrames_->frames()) {
            runFrame(frameCtx);
            if (framesToRender_ > 0 && frameCtx.frameNumber >= framesToRender_) {
                break;
            }
        }
    }
    else {
        for (auto &frameCtx : window_->frames()) {
            runFrame(frameCtx);
            if (framesToRender_ > 0 && frameCtx.frameNumber >= framesToRender_) {
                break;
            }
        }
    }

    // wait until last frame is finished rendering
    ctx().device().waitUntilIdle();
}

void ParticleComputeDemoApplication::defineRenderPasses(Cory::Framegraph &framegraph,
                                                        const Cory::FrameContext &frameCtx)
{
    const Cory::ScopeTimer s{"Frame/DeclarePasses"};

    auto frameHandles = framegraph.importFrameContext(frameCtx);

    auto mainPass = renderSystem_->spriteRenderTask(framegraph.declareTask("TASK_PointSprites"),
                                                    frameHandles.colorImage,
                                                    frameHandles.depthImage);

    auto layersOutput = layers().declareRenderTasks(
        framegraph, {.color = mainPass.output().colorOut, .depth = mainPass.output().depthOut});

    auto resolvedSwapchain =
        Cory::StandardRenderTasks::resolve(
            framegraph.declareTask("TASK_Resolve"), layersOutput.color, frameHandles.swapchainImage)
            .output();

    framegraph.declareOutput(resolvedSwapchain, Cory::Sync::AccessType::Present);
}

void ParticleComputeDemoApplication::drawImguiControls()
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
