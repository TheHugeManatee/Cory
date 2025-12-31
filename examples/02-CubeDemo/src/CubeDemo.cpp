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
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/ShaderManager.hpp>
#include <Cory/Renderer/UniformBufferObject.hpp>

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
    app.parse(argc, argv);

    Cory::ResourceLocator::addSearchPath(CUBEDEMO_RESOURCE_DIR);

    init(Cory::ContextCreationInfo{.validation = disableValidation_
                                                     ? Cory::ValidationLayers::Disabled
                                                     : Cory::ValidationLayers::Enabled,
                                   .args = std::span{argv, gsl::narrow<std::size_t>(argc)}});

    // determine msaa sample count to use - for simplicity, we use either 8 or one sample
    // const auto &limits = ctx().physicalDevice().limits;
    // const Gpu::SampleCountFlags counts =
    //     limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts;
    // // 2 samples are guaranteed to be supported, but we'd rather have 8
    // const auto msaaSamples = counts & Gpu::SampleCountFlagBits::Samples8Bit ? 8 : 2;
    // CO_APP_INFO("MSAA sample count: {}", msaaSamples);

    static constexpr auto WINDOW_SIZE = glm::i32vec2{1024, 1024};
    window_ = std::make_unique<Cory::Window>(ctx(), WINDOW_SIZE, "CubeDemo", 8);

    createGeometry();
    createShaders();

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

void CubeDemoApplication::createShaders()
{
    const Cory::ScopeTimer st{"Init/Shaders"};

    vertexShader_ = ctx().shaders().createShader(Cory::ResourceLocator::Locate("cube.vert.slang"));
    fragmentShader_ =
        ctx().shaders().createShader(Cory::ResourceLocator::Locate("cube.frag.slang"));
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
    // one framegraph for each frame in flight
    std::vector<Cory::Framegraph> framegraphs;
    uint32_t idx = 0;
    std::generate_n(std::back_inserter(framegraphs), Cory::MAX_FRAMES_IN_FLIGHT, [&]() {
        return Cory::Framegraph(ctx(), idx++);
    });

    auto time = getElapsedTimeSeconds();

    while (!window_->shouldClose()) {
        // Process KDGui events
        processEvents(0);
        glfwPollEvents();

        // Update time
        auto previousFrameTime = std::exchange(time, getElapsedTimeSeconds());
        auto delta = time - previousFrameTime;

        // Update layers
        layers().update(Cory::LogicUpdateContext{
            .simulationTime = time,
            .deltaTime = delta,
        });

        drawImguiControls();

        Cory::FrameContext frameCtx = window_->nextSwapchainImage();
        Cory::Framegraph &fg = framegraphs[frameCtx.inFlightIndex];
        // retire old resources from the last time this framegraph was
        // used - our frame synchronization ensures that the resources
        // are no longer in use
        fg.resetForNextFrame();

        defineRenderPasses(fg, frameCtx);

        auto execInfo = fg.record(frameCtx);

        window_->submitAndPresent(frameCtx);

        if (dumpNextFramegraph_) {
            CO_APP_INFO(fg.dump(execInfo));
            dumpNextFramegraph_ = false;
        }

        // break if number of frames to render are reached
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

    Gpu::ColorClearValue clearColor{0.0f, 0.0f, 0.0f, 1.0f};
    Gpu::DepthStencilClearValue clearDepthStencil = {1.0f, 0};

    auto [writtenColorHandle, colorInfo] =
        builder.write(colorTarget, Cory::Sync::AccessType::ColorAttachmentWrite);
    auto [writtenDepthHandle, depthInfo] =
        builder.write(depthTarget, Cory::Sync::AccessType::DepthStencilAttachmentWrite);

    Gpu::PushConstantRange pushRange{
        .offset = 0,
        .size = sizeof(Cory::BufferDeviceAddress),
        .shaderStages = Gpu::ShaderStageFlagBits::VertexBit | Gpu::ShaderStageFlagBits::FragmentBit,
    };
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
            },
        }},
        .depthAttachment =
            Cory::DepthStencilAttachment{
                .target = depthTarget,
                .load = Gpu::AttachmentLoadOperation::Clear,
                .store = Gpu::AttachmentStoreOperation::Store,
                .clearDepthStencil = clearDepthStencil,
            },
        .pushConstantRanges = {pushRange},
        .vertexOptions = vertexOptions(),
    });

    /// ^^^^     DECLARATION      ^^^^
    Cory::RenderInput renderApi = co_await builder.finishDeclaration(PassOutputs{
        .colorOut = writtenColorHandle,
        .depthOut = writtenDepthHandle,
    });
    /// vvvv  RENDERING COMMANDS  vvvv

    auto t = gsl::narrow_cast<float>(getElapsedTimeSeconds());

    auto passRecorder = cubePass.begin(*renderApi.cmd);

    float fovy = glm::radians(70.0f);
    float aspect = static_cast<float>(colorInfo.size.x) / static_cast<float>(colorInfo.size.y);
    glm::mat4 viewMatrix = camera_.getViewMatrix();
    glm::mat4 projectionMatrix = Cory::makePerspective(fovy, aspect, 1.0f, 10.0f);
    glm::mat4 viewProjection = projectionMatrix * viewMatrix;

    Cory::FrameContext &frameCtx = *renderApi.frameCtx;

    // update the per-frame data
    auto data = renderApi.bindingContext->alloc<CubeUBO>();
    data->view = viewMatrix;
    data->projection = projectionMatrix;
    data->viewProjection = viewProjection;
    data->lightPosition = camera_.getCameraPosition();
    passRecorder.pushConstant(pushRange, &data.gpu);

    auto &descriptorSets = ctx().descriptors();
    constexpr Cory::BufferHeapIndex kInstanceBufferIndex = 0;

    // Set dynamic states
    passRecorder.setCullMode(KDGpu::CullModeFlagBits::None);
    passRecorder.setDepthTestEnabled(true);
    passRecorder.setDepthWriteEnabled(true);
    passRecorder.setDepthCompareOp(KDGpu::CompareOperation::Less);

    // bind the mesh buffers
    passRecorder.setVertexBuffer(0, mesh_->vertexBuffer);
    passRecorder.setIndexBuffer(mesh_->indexBuffer);

    const uint32_t instanceCount = prepareInstanceData(t);
    if (instanceCount > 0) {
        auto &instanceBuffer = instanceBufferForFrame(frameCtx.inFlightIndex, instanceCount);
        auto *mapped = static_cast<std::byte *>(instanceBuffer.buffer.map());
        std::memcpy(mapped,
                    instanceData_.data(),
                    static_cast<size_t>(instanceCount) * sizeof(InstanceData));
        instanceBuffer.buffer.unmap();

        descriptorSets.write(Cory::BufferBindPoint::StorageBufferReadOnly,
                             frameCtx.inFlightIndex,
                             kInstanceBufferIndex,
                             instanceBuffer.buffer);
    }
    descriptorSets.bind(passRecorder, frameCtx.inFlightIndex);

    if (instanceCount > 0) {
        // draw all instances in a single call
        passRecorder.drawIndexed(Gpu::DrawIndexedCommand{
            .indexCount = mesh_->indexCount,
            .instanceCount = instanceCount,
            .firstIndex = 0,
            .vertexOffset = 0,
            .firstInstance = 0,
        });
    }

    passRecorder.end();
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

InstanceBuffer &CubeDemoApplication::instanceBufferForFrame(uint32_t frameIndex,
                                                            uint32_t instanceCount)
{
    if (instanceBuffers_.size() <= frameIndex) {
        instanceBuffers_.resize(frameIndex + 1);
    }

    const Gpu::DeviceSize requiredSize =
        gsl::narrow_cast<Gpu::DeviceSize>(instanceCount) * sizeof(InstanceData);
    auto &instanceBuffer = instanceBuffers_[frameIndex];
    if (!instanceBuffer.buffer.isValid() || instanceBuffer.capacity < requiredSize) {
        instanceBuffer.buffer = ctx().device().createBuffer(Gpu::BufferOptions{
            .label = "Cube Instance Buffer",
            .size = requiredSize,
            .usage = Gpu::BufferUsageFlagBits::StorageBufferBit,
            .memoryUsage = Gpu::MemoryUsage::CpuToGpu,
        });
        instanceBuffer.capacity = requiredSize;
    }

    return instanceBuffer;
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
