#include "SceneGraphDemo.hpp"

#include "Common.hpp"
#include "CubeAnimationSystem.hpp"
#include "CubeRenderSystem.hpp"

#include <Cory/Application/CameraLayer.hpp>
#include <Cory/Application/ImGuiLayer.hpp>
#include <Cory/Application/LayerStack.hpp>
#include <Cory/Application/Window.hpp>
#include <Cory/Base/GlmUtils.hpp>
#include <Cory/Base/Random.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Base/Time.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/ImGui/Inputs.hpp>
#include <Cory/ImGui/Widgets.hpp>
#include <Cory/RenderTasks/StandardRenderTasks.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/FrameSource.hpp>
#include <Cory/Renderer/HeadlessFrameSource.hpp>
#include <Cory/Systems/ComponentEditorSystem.hpp>
#include <Cory/Systems/TransformSystem.hpp>

#include <CLI/App.hpp>
#include <CLI/CLI.hpp>
#include <imgui.h>

#include <gsl/gsl>
#include <gsl/narrow>

#include <algorithm>

SceneGraphDemoApplication::SceneGraphDemoApplication(std::span<const char *> args)
{
    CLI::App app{"SceneGraphDemo"};
    bool disableValidation{false};
    app.add_option("-f,--frames", framesToRender_, "The number of frames to render");
    app.add_flag("--disable-validation", disableValidation, "Disable validation layers");
    app.add_flag("--headless", headless_, "Run without a window and render offscreen");
    app.allow_config_extras(true);
    app.parse(gsl::narrow<int>(args.size()), args.data());

    Cory::ResourceLocator::addSearchPath(SCENEGRAPHDEMO_RESOURCE_DIR);

    init(Cory::ContextCreationInfo{
        .validation =
            disableValidation ? Cory::ValidationLayers::Disabled : Cory::ValidationLayers::Enabled,
        .args = args,
    });

    // Use Cory API for MSAA sample count
    const int msaaSamples = 4; // Or use window_->samples() after window creation if needed
    static constexpr auto WINDOW_SIZE = glm::i32vec2{1024, 1024};
    if (headless_) {
        ctx().setupHeadlessDevice();
        headlessFrames_ = std::make_unique<Cory::HeadlessFrameSource>(
            ctx(),
            Cory::HeadlessFrameSourceCreateInfo{
                .label = "SceneGraphDemo-Headless",
                .size = Cory::glmu::u32vec2::from(WINDOW_SIZE),
                .samples = static_cast<Gpu::SampleCountFlagBits>(msaaSamples),
            });
    }
    else {
        window_ = std::make_unique<Cory::Window>(ctx(), WINDOW_SIZE, "SceneGraphDemo", msaaSamples);
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

    clock_.setTimeScale(0.01);
}

void SceneGraphDemoApplication::setupScene()
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
    auto center = sceneGraph_.createEntity(root,
                                           fmt::format("center"),
                                           AnimationComponent{
                                               .blend = 0.8f,
                                               .entityIndex = 0.0f,
                                           },
                                           Cory::Components::Transform{
                                               .position{0.0f, 0.0f, 0.0f},
                                           });

    // creates 5 cubes within a sphere around the given parent
    auto add_subcubes = [this](Cory::Entity parent, float level) -> std::vector<Cory::Entity> {
        std::vector<Cory::Entity> entities;

        float numChildren = Cory::RNG::Uniform(4.0f, 15.0f);
        for (int i = 0; i < numChildren; ++i) {
            const float radius = Cory::RNG::Uniform(3.0f, 7.0f);

            auto pos = Cory::RNG::UniformInSphere() * radius;
            auto scale = glm::vec3{Cory::RNG::Uniform(0.25f, 0.65f)};
            auto index = level + (level / 2.0f * Cory::RNG::Uniform(-1.0f, 2.0f));

            auto child = sceneGraph_.createEntity(parent,
                                                  fmt::format("cube{}", i),
                                                  AnimationComponent{
                                                      .blend = 0.8f,
                                                      .entityIndex = index,
                                                  },
                                                  Cory::Components::Transform{
                                                      .position = pos,
                                                      .scale = scale,
                                                  });
            entities.push_back(child);
        }
        return entities;
    };

    for (auto &e : add_subcubes(center, 0.25)) {
        for (auto &sub_e : add_subcubes(e, 0.5)) {
            add_subcubes(sub_e, 0.75);
        }
    }

    /// add a coordinate system indicator
    auto make_colored_axis =
        [&](std::string_view axis_name, glm::vec3 color, glm::vec3 axis, uint32_t steps) {
            // create entity with an AnimationComponent and a TransformComponent for each step
            for (uint32_t i = 0; i < steps / 2; ++i) {
                sceneGraph_.createEntity(root,
                                         fmt::format("{}{}", axis_name, i),
                                         AnimationComponent{
                                             .color = glm::vec4{color, 1.0f},
                                             .blend = 0.5f,
                                             .entityIndex = -1.0f,
                                         },
                                         Cory::Components::Transform{
                                             .position = 0.2f * axis * static_cast<float>(i),
                                             .scale = glm::vec3{0.1f},
                                         });
            }
        };
    // create colored axes for X, Y and Z
    make_colored_axis("X", {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 100);
    make_colored_axis("Y", {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 100);
    make_colored_axis("Z", {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, 100);
}

void SceneGraphDemoApplication::setupSystems()
{
    animationSystem_ = &systems_.emplace<CubeAnimationSystem>();

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

    // after the "logic" has updated, sync all the transforms of the scenegraph
    systems_.emplace<Cory::TransformSystem>();

    // render system should go last to be aware of the latest state
    renderSystem_ = &systems_.emplace<CubeRenderSystem>(ctx());

    auto &componentEditor = systems_.emplace<Cory::ComponentEditorSystem>();
    componentEditor.addComponentEditor(
        "Animation", [](Cory::SceneGraph &sceneGraph, Cory::Entity entity) {
            auto *animation = sceneGraph.getComponent<AnimationComponent>(entity);
            if (animation == nullptr ||
                !ImGui::CollapsingHeader("Animation Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                return;
            }

            bool animated = animation->entityIndex >= 0.0f;
            if (ImGui::Checkbox("Driven by Animation System", &animated)) {
                if (animated && animation->entityIndex < 0.0f) {
                    animation->entityIndex = 0.0f;
                }
                else if (!animated) {
                    animation->entityIndex = -1.0f;
                }
            }

            if (animated) {
                ImGui::DragFloat("Animation Index", &animation->entityIndex, 0.05f, 0.0f, 200.0f);
                CoImGui::Text("Color/Blend are authored by CubeAnimationSystem at runtime.");
            }
            else {
                ImGui::ColorEdit4("Color", &animation->color.x);
                ImGui::SliderFloat("Blend", &animation->blend, 0.0f, 1.0f, "%.3f");
            }

            CoImGui::Text("Current RGBA: [{:.3f}, {:.3f}, {:.3f}, {:.3f}]",
                          animation->color.x,
                          animation->color.y,
                          animation->color.z,
                          animation->color.w);
            CoImGui::Text("Current Blend: {:.3f}", animation->blend);
        });
}

SceneGraphDemoApplication::~SceneGraphDemoApplication()
{
    CO_APP_TRACE("Destroying SceneGraphDemoApplication");
}

void SceneGraphDemoApplication::run()
{
    auto framegraphs = createFramegraphs();

    auto &frameSource = headless_ ? static_cast<Cory::FrameSource &>(*headlessFrames_)
                                  : static_cast<Cory::FrameSource &>(*window_);
    runMainLoop(
        frameSource,
        framesToRender_,
        {.headless = headless_},
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
                               "SceneGraphDemo",
                               frameCtx.frameNumber);
                dumpNextFramegraph_ = false;
            }
        },
        [this](Cory::FrameContext &, const Cory::LogicUpdateContext &) { drawImguiControls(); });
}

void SceneGraphDemoApplication::defineRenderPasses(Cory::Framegraph &framegraph,
                                                   const Cory::FrameContext &frameCtx)
{
    const Cory::ScopeTimer s{"Frame/DeclarePasses"};

    auto frameHandles = framegraph.importFrameContext(frameCtx);

    auto mainPass = renderSystem_->cubeRenderTask(
        framegraph.declareTask("TASK_Cubes"), frameHandles.colorImage, frameHandles.depthImage);

    auto layersOutput = layers().declareRenderTasks(
        framegraph, {.color = mainPass.output().colorOut, .depth = mainPass.output().depthOut});

    auto copiedSwapchain =
        Cory::StandardRenderTasks::copyToTarget(framegraph.declareTask("TASK_CopyToTarget"),
                                                layersOutput.color,
                                                frameHandles.swapchainImage)
            .output();

    framegraph.declareOutput(copiedSwapchain, Cory::Sync::AccessType::Present);
}

void SceneGraphDemoApplication::drawImguiControls()
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

    animationSystem_->drawImguiControls();
    if (ImGui::Begin("Camera")) {
        glm::vec3 position = cameraLayer_->position();
        glm::vec3 center = cameraLayer_->focus();
        glm::vec3 up = cameraLayer_->up();
        glm::mat4 mat = glm::transpose(cameraLayer_->worldToViewMatrix());

        [[maybe_unused]] const bool changed = CoImGui::Input("position", position, "%.3f") ||
                                              CoImGui::Input("center", center, "%.3f") ||
                                              CoImGui::Input("up", up, "%.3f");

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
