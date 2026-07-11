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
#include <Cory/Renderer/FrameCapture.hpp>
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
#include <glm/common.hpp>

ParticleComputeDemoApplication::ParticleComputeDemoApplication(std::span<const char *> args)
{
    CLI::App app{"ParticleComputeDemoApplication"};
    bool disableValidation{false};
    std::string outputPathString{};
    app.add_option("-f,--frames", framesToRender_, "The number of frames to render");
    app.add_option("--output", outputPathString, "Write the final frame to a BMP file");
    app.add_flag("--disable-validation", disableValidation, "Disable validation layers");
    app.add_flag("--headless", headless_, "Run without a window and render offscreen");
    app.allow_config_extras(true);
    app.parse(gsl::narrow<int>(args.size()), args.data());
    if (!outputPathString.empty()) {
        output_.outputPath = outputPathString;
        output_.forceHeadlessAndFinite(headless_, framesToRender_);
    }

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
    renderSystem_ = &systems_.emplace<PointSpriteRenderSystem>(ctx());

    auto &componentEditor = systems_.emplace<Cory::ComponentEditorSystem>();
    componentEditor.addComponentEditor(
        "Point Sprite", [](Cory::SceneGraph &sceneGraph, Cory::Entity entity) {
            auto *sprite = sceneGraph.getComponent<PointSpriteComponent>(entity);
            if (sprite == nullptr ||
                !ImGui::CollapsingHeader("Point Sprite Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                return;
            }

            ImGui::DragFloat3("Position", &sprite->position.x, 0.05f);

            auto diameter = sprite->radius * 2.0f;
            if (ImGui::DragFloat(
                    "Diameter", &diameter, 0.005f, 0.002f, 20.0f, "%.4f", ImGuiSliderFlags_Logarithmic)) {
                sprite->radius = std::max(diameter * 0.5f, 0.001f);
            }

            if (ImGui::DragFloat(
                    "Radius", &sprite->radius, 0.005f, 0.001f, 10.0f, "%.4f", ImGuiSliderFlags_Logarithmic)) {
                sprite->radius = std::max(sprite->radius, 0.001f);
            }

            ImGui::ColorEdit4("Color", &sprite->color.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
            sprite->color.w = glm::clamp(sprite->color.w, 0.0f, 1.0f);

            const auto luminance =
                glm::dot(glm::vec3{sprite->color}, glm::vec3{0.2126f, 0.7152f, 0.0722f});
            CoImGui::Text("Perceived luminance: {:.3f}", luminance);
        });
}

ParticleComputeDemoApplication::~ParticleComputeDemoApplication()
{
    CO_APP_TRACE("Destroying ParticleComputeDemoApplication");
}

void ParticleComputeDemoApplication::run()
{
    auto framegraphs = createFramegraphs();

    auto &frameSource = headless_ ? static_cast<Cory::FrameSource &>(*headlessFrames_)
                                  : static_cast<Cory::FrameSource &>(*window_);
    runMainLoop(
        frameSource,
        framesToRender_,
        {.headless = headless_, .pollPlatformEvents = true},
        [this, &framegraphs](Cory::FrameContext &frameCtx, const Cory::LogicUpdateContext &) {
            auto tickInfo = clock_.tick();
            systems_.tick(sceneGraph_, tickInfo);

            auto recordedFrame = recordFramegraph(
                framegraphs,
                frameCtx,
                [this](Cory::Framegraph &fg, const Cory::FrameContext &currentFrame) {
                    defineRenderPasses(fg, currentFrame);
                });

            if (output_.requested()) {
                capturedFrame_ = Cory::CapturedFrame{
                    .texture = frameCtx.swapchainImage,
                    .extent = frameCtx.extent,
                    .format = frameCtx.colorFormat,
                    .layout = Gpu::TextureLayout::PresentSrc,
                };
            }

            if (dumpNextFramegraph_) {
                dumpFramegraph(recordedFrame.framegraph,
                               recordedFrame.executionInfo,
                               "ParticleComputeDemo",
                               frameCtx.frameNumber);
                dumpNextFramegraph_ = false;
            }
        },
        [this](Cory::FrameContext &, const Cory::LogicUpdateContext &) { drawImguiControls(); });

    if (!output_.requested()) {
        return;
    }

    CO_CORE_ASSERT(capturedFrame_.has_value(),
                   "ParticleComputeDemo output requested, but no frame texture was captured");
    Cory::writeCapturedFrameBmp(ctx(), *capturedFrame_, output_.outputPath);
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

    auto copiedSwapchain =
        Cory::StandardRenderTasks::copyToTarget(framegraph.declareTask("TASK_CopyToTarget"),
                                                layersOutput.color,
                                                frameHandles.swapchainImage)
            .output();

    framegraph.declareOutput(copiedSwapchain, Cory::Sync::AccessType::Present);
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
