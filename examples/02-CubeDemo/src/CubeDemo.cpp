#include "CubeDemo.hpp"

#include <Cory/Application/DepthDebugLayer.hpp>
#include <Cory/Application/DynamicGeometry.hpp>
#include <Cory/Application/ImGuiLayer.hpp>
#include <Cory/Application/LayerStack.hpp>
#include <Cory/Application/Window.hpp>
#include <Cory/Base/GlmUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Base/Math.hpp>
#include <Cory/Base/Profiling.hpp>
#include <Cory/Base/Random.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/ImGui/Inputs.hpp>
#include <Cory/ImGui/Widgets.hpp>
#include <Cory/RenderTasks/StandardRenderTasks.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/FrameSource.hpp>
#include <Cory/Renderer/HeadlessFrameSource.hpp>
#include <Cory/Renderer/Shader.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include <KDGpu/buffer_options.h>
#include <KDGpu/vulkan/vulkan_resource_manager.h>

#include <CLI/App.hpp>
#include <CLI/CLI.hpp>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <gsl/gsl>
#include <gsl/narrow>
#include <imgui.h>

#include <Cory/Framegraph/FramegraphResourceManager.hpp>
#include <Cory/Framegraph/ShaderBindingContext.hpp>
#include <algorithm>
#include <chrono>
#include <cstddef>

static struct AnimationData {
    int num_cubes{5000};
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

void randomize(AnimationData::param &p)
{
    p.val = Cory::RNG::Uniform(p.min, p.max);
}
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

void animate(InstanceData &d, float t, float i)
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

    d.normalTransform = transpose(inverse(d.modelTransform));
    d.color = start * cm;
    d.parameters = glm::vec4{ad.blend, 0.0f, 0.0f, 0.0f};
}

CubeDemoApplication::CubeDemoApplication(int argc, const char **argv)
    : mesh_{}
    , startupTime_{now()}
{
    CLI::App app{"CubeDemo"};
    app.add_option("-f,--frames", framesToRender_, "The number of frames to render");
    app.add_flag("--disable-validation", disableValidation_, "Disable validation layers");
    app.add_flag("--headless", headless_, "Run without a window and render offscreen");
    app.parse(argc, argv);
    const std::vector<const char *> appArgs{argv, argv + argc};

    Cory::ResourceLocator::addSearchPath(CUBEDEMO_RESOURCE_DIR);

    init(Cory::ContextCreationInfo{.validation = disableValidation_
                                                     ? Cory::ValidationLayers::Disabled
                                                     : Cory::ValidationLayers::Enabled,
                                   .args = std::span{appArgs}});

    // determine msaa sample count to use - for simplicity, we use either 8 or one sample
    // const auto &limits = ctx().physicalDevice().limits;
    // const Gpu::SampleCountFlags counts =
    //     limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts;
    // // 2 samples are guaranteed to be supported, but we'd rather have 8
    // const auto msaaSamples = counts & Gpu::SampleCountFlagBits::Samples8Bit ? 8 : 2;
    // CO_APP_INFO("MSAA sample count: {}", msaaSamples);

    static constexpr auto WINDOW_SIZE = glm::i32vec2{1024, 1024};
    if (headless_) {
        ctx().setupHeadlessDevice();
        headlessFrames_ = std::make_unique<Cory::HeadlessFrameSource>(
            ctx(),
            Cory::HeadlessFrameSourceCreateInfo{
                .label = "CubeDemo-Headless",
                .size = Cory::glmu::u32vec2::from(WINDOW_SIZE),
                .samples = Gpu::SampleCountFlagBits::Samples8Bit,
            });
    }
    else {
        window_ = std::make_unique<Cory::Window>(ctx(), WINDOW_SIZE, "CubeDemo", 8);
    }

    createGeometry();
    createShaders();

    if (!headless_) {
        auto recreateSizedResources = [&](Cory::SwapchainResizedEvent e) {
            // createFramebuffers();
            layers().processEvent(e);
        };
        window_->onSwapchainResized.connect(recreateSizedResources);
        recreateSizedResources({window_->dimensions()});

        Cory::LayerAttachInfo layerAttachInfo{.maxFramesInFlight = Cory::MAX_FRAMES_IN_FLIGHT,
                                              .viewportDimensions = window_->dimensions()};
        layers().addLayer<Cory::DepthDebugLayer>(layerAttachInfo);
        layers().emplacePriorityLayer<Cory::ImGuiLayer>(layerAttachInfo, std::ref(*window_));

        camera_.setMode(Cory::CameraManipulator::Mode::Trackball);
        camera_.setWindowSize(window_->dimensions());
        camera_.setLookat({0.0f, 3.0f, 2.5f}, {0.0f, 4.0f, 2.0f}, {0.0f, 1.0f, 0.0f});
        setupCameraCallbacks();
    }
}

void CubeDemoApplication::createShaders()
{
    const Cory::ScopeTimer st{"Init/Shaders"};

    vertexShader_ = ctx().shaders().createShader(
        Cory::ShaderSource{Cory::ResourceLocator::Locate("cube.vert.slang")});
    fragmentShader_ = ctx().shaders().createShader(
        Cory::ShaderSource{Cory::ResourceLocator::Locate("cube.frag.slang")});
}

CubeDemoApplication::~CubeDemoApplication()
{
    auto &shaders = ctx().shaders();
    shaders.release(vertexShader_);
    shaders.release(fragmentShader_);
    CO_APP_TRACE("Destroying CubeDemoApplication");
}

void CubeDemoApplication::run()
{
    Cory::FramegraphResourceManager framegraphResources{ctx()};
    // one framegraph for each frame in flight
    std::vector<Cory::Framegraph> framegraphs;
    uint32_t idx = 0;
    std::generate_n(std::back_inserter(framegraphs), Cory::MAX_FRAMES_IN_FLIGHT, [&]() {
        return Cory::Framegraph(ctx(), framegraphResources, idx++);
    });

    auto time = getElapsedTimeSeconds();

    auto runFrame = [&](Cory::FrameContext &frameCtx) {
        // Process KDGui events
        if (!headless_) {
            processEvents(0);
            glfwPollEvents();
        }

        // Update time
        auto previousFrameTime = std::exchange(time, getElapsedTimeSeconds());
        auto delta = time - previousFrameTime;

        if (!headless_) {
            // Update layers
            layers().update(Cory::LogicUpdateContext{
                .simulationTime = time,
                .deltaTime = delta,
            });
        }

        if (!headless_) {
            drawImguiControls();
        }

        Cory::Framegraph &fg = framegraphs[frameCtx.inFlightIndex];
        // retire old resources from the last time this framegraph was
        // used - our frame synchronization ensures that the resources
        // are no longer in use
        fg.resetForNextFrame(frameCtx.frameNumber);

        defineRenderPasses(fg, frameCtx);

        auto execInfo = fg.record(frameCtx);

        if (dumpNextFramegraph_) {
            std::filesystem::path outputPath =
                std::filesystem::current_path() /
                fmt::format("CubeDemo_Frame_{:04}.html", frameCtx.frameNumber);
            fg.dump(execInfo, outputPath);
            auto file_link = "file://" + absolute(outputPath).string();
            // replace backslashes with forward slashes so IDE's add auto-links for convenience
            std::ranges::replace(file_link, '\\', '/');

            CO_CORE_INFO("Dumped framegraph to\n{}", file_link);
            dumpNextFramegraph_ = false;
        }
    };

    auto &frameSource = headless_ ? static_cast<Cory::FrameSource &>(*headlessFrames_)
                                  : static_cast<Cory::FrameSource &>(*window_);
    auto frames = frameSource.frames();
    for (auto &frameCtx : frames) {
        runFrame(frameCtx);
        if (framesToRender_ > 0 && frameCtx.frameNumber >= framesToRender_) {
            break;
        }
    }

    // wait until last frame is finished rendering
    ctx().device().waitUntilIdle();
}

void CubeDemoApplication::defineRenderPasses(Cory::Framegraph &framegraph,
                                             const Cory::FrameContext &frameCtx)
{
    const Cory::ScopeTimer s{"Frame/DeclarePasses"};

    auto [colorImage, depthImage, swapchainImage] = framegraph.importFrameContext(frameCtx);

    auto mainPass = cubeRenderTask(framegraph.declareTask("TASK_Cubes"), colorImage, depthImage);

    auto layersOutput = layers().declareRenderTasks(
        framegraph, {.color = mainPass.output().colorOut, .depth = mainPass.output().depthOut});

    auto resolvedSwapchain =
        Cory::StandardRenderTasks::resolve(
            framegraph.declareTask("TASK_Resolve"), layersOutput.color, swapchainImage)
            .output();

    framegraph.declareOutput(resolvedSwapchain, Cory::Sync::AccessType::Present);
}

Cory::RenderTaskDeclaration<CubeDemoApplication::PassOutputs>
CubeDemoApplication::cubeRenderTask(Cory::RenderTaskBuilder builder,
                                    Cory::TransientTextureHandle colorTarget,
                                    Cory::TransientTextureHandle depthTarget)
{

    Gpu::ColorClearValue clearColor{{0.0f, 0.0f, 0.0f, 1.0f}};
    Gpu::DepthStencilClearValue clearDepthStencil = {1.0f, 0};

    const auto &colorInfo = builder.textureInfo(colorTarget);

    auto cubePass = builder.declareRenderPass(Cory::RenderPassDeclaration{
        .name = "PASS_Cubes",
        .shaders = {vertexShader_, fragmentShader_},
        .attachments = {{
            {
                // main color target
                .target = colorTarget,
                .load = Gpu::AttachmentLoadOperation::Clear,
                .store = Gpu::AttachmentStoreOperation::Store,
                .clearColor = clearColor,
                .blend = std::nullopt,
            },
        }},
        .depthAttachment =
            Cory::DepthStencilAttachment{
                .target = depthTarget,
                .load = Gpu::AttachmentLoadOperation::Clear,
                .store = Gpu::AttachmentStoreOperation::Store,
                .clearDepthStencil = clearDepthStencil,
            },
        .stencilAttachment = {},
        .vertexOptions = vertexOptions(),
        .dynamicStates = {.cullMode = Cory::CullMode::None},
    });
    const auto colorOut = cubePass.colorOutputs().front();
    const auto depthOut = cubePass.depthOutput().value();

    /// ^^^^     DECLARATION      ^^^^
    Cory::RenderInput renderApi =
        co_await builder.finishDeclaration(PassOutputs{.colorOut = colorOut, .depthOut = depthOut});
    /// vvvv  RENDERING COMMANDS  vvvv

    auto t = gsl::narrow_cast<float>(getElapsedTimeSeconds());

    auto passRecorder = cubePass.begin(renderApi);

    float fovy = glm::radians(70.0f);
    float aspect = static_cast<float>(colorInfo.size.x) / static_cast<float>(colorInfo.size.y);
    glm::mat4 viewMatrix = camera_.getViewMatrix();
    glm::mat4 projectionMatrix = Cory::makePerspective(fovy, aspect, 1.0f, 10.0f);
    glm::mat4 viewProjection = projectionMatrix * viewMatrix;

    const uint32_t instanceCount = prepareInstanceData(t);
    CO_CORE_ASSERT(instanceCount > 0, "No instances to render!");

    auto alloc = renderApi.bindingContext->alloc<InstanceData>(instanceCount);
    std::copy_n(instanceData_.begin(), instanceCount, alloc.cpu);

    // update the per-frame data
    auto data = renderApi.bindingContext->alloc<CubeUBO>();
    data->view = viewMatrix;
    data->projection = projectionMatrix;
    data->viewProjection = viewProjection;
    data->lightPosition = camera_.getCameraPosition();
    data->instances = alloc.gpu;
    renderApi.bindingContext->push(data.gpu);

    // bind the mesh buffers
    passRecorder.setVertexBuffer(0, mesh_->vertexBuffer);
    passRecorder.setIndexBuffer(mesh_->indexBuffer);

    // draw all instances in a single call
    passRecorder.drawIndexed(Gpu::DrawIndexedCommand{
        .indexCount = mesh_->indexCount,
        .instanceCount = instanceCount,
        .firstIndex = 0,
        .vertexOffset = 0,
        .firstInstance = 0,
    });

    cubePass.end(std::move(passRecorder));
}

Gpu::VertexOptions CubeDemoApplication::vertexOptions() const
{
    auto attributes = Cory::Mesh::vertexAttributes();
    for (auto &attribute : attributes) {
        attribute.binding = 0;
    }

    return Gpu::VertexOptions{
        .buffers =
            {
                Gpu::VertexBufferLayout{
                    .binding = 0,
                    .stride = sizeof(Cory::Mesh::Vertex),
                    .inputRate = Gpu::VertexRate::Vertex,
                },
            },
        .attributes = std::move(attributes),
    };
}

uint32_t CubeDemoApplication::prepareInstanceData(float timeSeconds)
{
    const int requestedInstances = std::max(ad.num_cubes, 0);
    const uint32_t instanceCount = gsl::narrow_cast<uint32_t>(requestedInstances);

    instanceData_.resize(instanceCount);

    for (uint32_t idx = 0; idx < instanceCount; ++idx) {
        const float i = instanceCount == 1
                            ? 1.0f
                            : static_cast<float>(idx) / static_cast<float>(instanceCount - 1);

        animate(instanceData_[idx], timeSeconds, i);
    }

    return instanceCount;
}

void CubeDemoApplication::createGeometry()
{
    const Cory::ScopeTimer st{"Init/Geometry"};
    mesh_ = std::make_unique<Cory::Mesh>(Cory::DynamicGeometry::createCube(ctx()));
}
double CubeDemoApplication::now()
{
    return std::chrono::duration<double>(
               std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

double CubeDemoApplication::getElapsedTimeSeconds() const
{
    return now() - startupTime_;
}
void CubeDemoApplication::drawImguiControls()
{
    const Cory::ScopeTimer st{"Frame/ImGui"};

    if (ImGui::Begin("Animation Params")) {
        if (ImGui::Button("Dump Framegraph")) {
            dumpNextFramegraph_ = true;
        }
        if (ImGui::Button("Restart")) {
            startupTime_ = now();
        }
        if (ImGui::Button("Randomize")) {
            randomize();
        }

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

        if (changed) {
            camera_.setLookat(position, center, up);
        }

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

void CubeDemoApplication::setupCameraCallbacks()
{
    window_->onSwapchainResized.connect([this](Cory::SwapchainResizedEvent event) {
        layers().processEvent(event);
        camera_.setWindowSize(event.size);
    });

    window_->onMouseMoved.connect([this](Cory::MouseMovedEvent event) {
        if (layers().processEvent(event)) {
            return;
        }
        if (event.button != Cory::MouseButton::None) {
            camera_.mouseMove(glm::ivec2(event.position), event.button, event.modifiers);
        }
    });
    window_->onMouseButton.connect([this](Cory::MouseButtonEvent event) {
        if (layers().processEvent(event)) {
            return;
        }
        camera_.setMousePosition(event.position);
    });

    window_->onMouseScrolled.connect([this](Cory::ScrollEvent event) {
        if (layers().processEvent(event)) {
            return;
        }
        camera_.wheel(static_cast<int32_t>(event.scrollDelta.y));
    });
    window_->onKeyCallback.connect([this](Cory::KeyEvent event) {
        // Camera is not interested in key events currently
        layers().processEvent(event);
    });
}
