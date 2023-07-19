#include "SceneGraphDemo.hpp"

#include <Cory/Application/DepthDebugLayer.hpp>
#include <Cory/Application/DynamicGeometry.hpp>
#include <Cory/Application/ImGuiLayer.hpp>
#include <Cory/Application/LayerStack.hpp>
#include <Cory/Application/Window.hpp>
#include <Cory/Base/Random.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Cory.hpp>
#include <Cory/Framegraph/CommandList.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/Framegraph/TextureManager.hpp>
#include <Cory/ImGui/Inputs.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/ResourceManager.hpp>
#include <Cory/SceneGraph/System.hpp>

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

struct AnimationComponent {
    glm::mat4 modelTransform{1.0f};
    glm::vec4 color{1.0, 0.0, 0.0, 1.0};
    float blend;
};

static struct AnimationData {
    int num_cubes{200};
    float blend{0.8f};

    struct param {
        float val;
        float min;
        float max;
        operator float() const { return val; }
    };
    param ti{1.5f, 0.0f, 10.0f};
    param tsi{2.0f, 0.0f, 3.0f};
    param tsf{100.0f, 0.0f, 250.0f};
    param r0{0.0f, -2.0f, 2.0f};
    param rt{-0.1f, -2.0f, 2.0f};
    param ri{1.3f, -2.0f, 2.0f};
    param rti{0.05f, -2.0f, 2.0f};
    param s0{0.05f, 0.0f, 1.0f};
    param st{0.0f, -0.01f, 0.01f};
    param si{0.4f, 0.0f, 2.0f};
    param c0{-0.75f, -2.0f, 2.0f};
    param cf0{2.0f, -10.0f, 10.0f};
    param cfi{-0.5f, -2.0f, 2.0f};

    glm::vec3 translation{0.0, 0.0f, 2.5f};
    glm::vec3 rotation{0.0f};
} ad;

void randomize(AnimationData::param &p) { p.val = Cory::RNG::Uniform(p.min, p.max); }
void randomize()
{
    randomize(ad.ti);
    randomize(ad.tsi);
    randomize(ad.tsf);
    randomize(ad.r0);
    randomize(ad.rt);
    randomize(ad.ri);
    randomize(ad.rti);
    randomize(ad.s0);
    randomize(ad.st);
    randomize(ad.si);
    randomize(ad.c0);
    randomize(ad.cf0);
    randomize(ad.cfi);
}

void animate(AnimationComponent &d, float t, float i)
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

struct AnimationSystem : public Cory::SimpleSystem<AnimationSystem, AnimationComponent> {
    void Init(Cory::Context &ctx)
    {
        mesh_ = std::make_unique<Vk::Mesh>(Cory::DynamicGeometry::createCube(ctx));
    }
    void Destroy() { mesh_.reset(); }

    void beforeUpdate(Cory::SceneGraph &sg)
    {
        // todo what if we reduce num_cubes?
        while (entityIndex < ad.num_cubes) {
            sg.createEntity(sg.root(), fmt::format("cube{}", entityIndex), AnimationComponent{});
            entityIndex += 1.0f;
        }
        entityIndex = 0.0f;
    }

    void
    update(Cory::SceneGraph &sg, Cory::TickInfo tick, Cory::Entity entity, AnimationComponent &anim)
    {
        animate(anim, tick.now.time_since_epoch().count(), entityIndex);
        entityIndex += 1.0f;
    }

    void recordCommands(Cory::Context &ctx, Cory::SceneGraph &sg, Cory::CommandList &cmd)
    {
        forEach<AnimationComponent>(sg, [&](Cory::Entity e, const AnimationComponent &anim) {
            // update push constants
            ctx.device()->CmdPushConstants(cmd->handle(),
                                           ctx.defaultPipelineLayout(),
                                           VkShaderStageFlagBits::VK_SHADER_STAGE_ALL,
                                           0,
                                           sizeof(anim),
                                           &anim);

            cmd.handle().draw(*mesh_);
        });
    }

  private:
    std::unique_ptr<Vk::Mesh> mesh_;
    float entityIndex{0};
};
static_assert(Cory::System<AnimationSystem>);
static AnimationSystem animationSystem;

SceneGraphDemoApplication::SceneGraphDemoApplication(std::span<const char *> args)
    : mesh_{}
{
    CLI::App app{"SceneGraphDemo"};
    app.add_option("-f,--frames", framesToRender_, "The number of frames to render");
    app.add_flag("--disable-validation", disableValidation_, "Disable validation layers");
    app.allow_config_extras(true);
    app.parse(gsl::narrow<int>(args.size()), args.data());

    Cory::ResourceLocator::addSearchPath(SCENEGRAPHDEMO_RESOURCE_DIR);

    init(Cory::ContextCreationInfo{
        .validation =
            disableValidation_ ? Cory::ValidationLayers::Disabled : Cory::ValidationLayers::Enabled,
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

    createGeometry();
    createShaders();

    Cory::LayerAttachInfo layerAttachInfo{.maxFramesInFlight =
                                              window_->swapchain().maxFramesInFlight(),
                                          .viewportDimensions = window_->dimensions()};
    layers().emplacePriorityLayer<Cory::ImGuiLayer>(layerAttachInfo, std::ref(*window_));

    camera_.setMode(Cory::CameraManipulator::Mode::Fly);
    camera_.setWindowSize(window_->dimensions());
    camera_.setLookat({0.0f, 3.0f, 2.5f}, {0.0f, 4.0f, 2.0f}, {0.0f, 1.0f, 0.0f});
    setupCameraCallbacks();
    createUBO();
}

void SceneGraphDemoApplication::createShaders()
{
    const Cory::ScopeTimer st{"Init/Shaders"};
    vertexShader_ = ctx().resources().createShader(Cory::ResourceLocator::Locate("cube.vert"));
    fragmentShader_ = ctx().resources().createShader(Cory::ResourceLocator::Locate("cube.frag"));
}

void SceneGraphDemoApplication::createUBO()
{
    // create and initialize descriptor sets for each frame in flight
    const Cory::ScopeTimer st{"Init/UBO"};
    globalUbo_ = std::make_unique<Cory::UniformBufferObject<CubeUBO>>(
        ctx(), window_->swapchain().maxFramesInFlight());
}

SceneGraphDemoApplication::~SceneGraphDemoApplication()
{
    animationSystem.Destroy(); // TODO this should be done automatically
    auto &resources = ctx().resources();
    resources.release(vertexShader_);
    resources.release(fragmentShader_);
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
        animationSystem.tick(sceneGraph_, tickInfo);

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

    auto mainPass =
        cubeRenderTask(framegraph.declareTask("TASK_Cubes"), windowColorTarget, windowDepthTarget);

    auto layersOutput = layers().declareRenderTasks(
        framegraph, {.color = mainPass.output().colorOut, .depth = mainPass.output().depthOut});

    auto [outInfo, outState] = framegraph.declareOutput(layersOutput.color);
}

Cory::RenderTaskDeclaration<SceneGraphDemoApplication::PassOutputs>
SceneGraphDemoApplication::cubeRenderTask(Cory::RenderTaskBuilder builder,
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

    cubePass.begin(*renderApi.cmd);

    float fovy = glm::radians(70.0f);
    float aspect = static_cast<float>(colorInfo.size.x) / static_cast<float>(colorInfo.size.y);
    glm::mat4 viewMatrix = camera_.getViewMatrix();
    glm::mat4 projectionMatrix = Cory::makePerspective(fovy, aspect, 1.0f, 10.0f);
    glm::mat4 viewProjection = projectionMatrix * viewMatrix;

    Cory::FrameContext &frameCtx = *renderApi.frameCtx;

    // update the uniform buffer
    CubeUBO &ubo = (*globalUbo_)[frameCtx.index];
    ubo.view = viewMatrix;
    ubo.projection = projectionMatrix;
    ubo.viewProjection = viewProjection;
    // need explicit flush otherwise the mapped memory is not synced to the GPU
    globalUbo_->flush(frameCtx.index);

    ctx()
        .descriptorSets()
        .write(Cory::DescriptorSets::SetType::Static, frameCtx.index, *globalUbo_)
        .flushWrites()
        .bind(renderApi.cmd->handle(), frameCtx.index, ctx().defaultPipelineLayout());

    // records commands for each cube
    animationSystem.recordCommands(ctx(), sceneGraph_, *renderApi.cmd);

    cubePass.end(*renderApi.cmd);
}

void SceneGraphDemoApplication::createGeometry()
{
    const Cory::ScopeTimer st{"Init/Geometry"};
    animationSystem.Init(ctx());
}

void SceneGraphDemoApplication::drawImguiControls()
{
    namespace CoImGui = Cory::ImGui;
    const Cory::ScopeTimer st{"Frame/ImGui"};

    if (ImGui::Begin("Animation Params")) {
        if (ImGui::Button("Dump Framegraph")) { dumpNextFramegraph_ = true; }
        CoImGui::Text("Time: {:.3f}, Frame: {}",
                      clock_.lastTick().now.time_since_epoch().count(),
                      clock_.lastTick().ticks);
        if (ImGui::Button("Restart")) { clock_.reset(); }
        if (ImGui::Button("Randomize")) { randomize(); }

        CoImGui::Input("Cubes", ad.num_cubes, 1, 10000);
        CoImGui::Slider("blend", ad.blend, 0.0f, 1.0f);
        CoImGui::Slider("translation", ad.translation, -3.0f, 3.0f);
        CoImGui::Slider("rotation", ad.rotation, -glm::pi<float>(), glm::pi<float>());

        CoImGui::Slider("ti", ad.ti.val, ad.ti.min, ad.ti.max);
        CoImGui::Slider("tsi", ad.tsi.val, ad.tsi.min, ad.tsi.max);
        CoImGui::Slider("tsf", ad.tsf.val, ad.tsf.min, ad.tsf.max);
        CoImGui::Slider("r0", ad.r0.val, ad.r0.min, ad.r0.max);
        CoImGui::Slider("rt", ad.rt.val, ad.rt.min, ad.rt.max);
        CoImGui::Slider("ri", ad.ri.val, ad.ri.min, ad.ri.max);
        CoImGui::Slider("rti", ad.rti.val, ad.rti.min, ad.rti.max);
        CoImGui::Slider("s0", ad.s0.val, ad.s0.min, ad.s0.max);
        CoImGui::Slider("st", ad.st.val, ad.st.min, ad.st.max);
        CoImGui::Slider("si", ad.si.val, ad.si.min, ad.si.max);
        CoImGui::Slider("c0", ad.c0.val, ad.c0.min, ad.c0.max);
        CoImGui::Slider("cf0", ad.cf0.val, ad.cf0.min, ad.cf0.max);
        CoImGui::Slider("cfi", ad.cfi.val, ad.cfi.min, ad.cfi.max);
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
