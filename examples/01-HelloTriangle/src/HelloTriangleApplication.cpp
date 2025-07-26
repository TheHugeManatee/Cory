#include "HelloTriangleApplication.hpp"

#include "TrianglePipeline.hpp"

// #include <Cory/Application/ImGuiLayer.hpp>
#include <Cory/Application/LayerStack.hpp>
#include <Cory/Application/Window.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Base/Profiling.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Cory.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/Swapchain.hpp>

#include <CLI/App.hpp>
#include <CLI/CLI.hpp>
#include <KDGpu/buffer_options.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat2x2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <gsl/gsl>
#include <imgui.h>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>

#include <array>
#include <chrono>

struct PushConstants {
    glm::vec4 color{1.0, 0.0, 0.0, 1.0};
    glm::mat2 transform{1.0f};
    glm::vec2 offset{0.0};
};

static struct AnimationData {
    int num_cubes{30};

    float r0{0.0f};
    float rt{-0.1f};
    float ri{0.0f};
    float rti{0.68f};
    float s0{2.0f};
    float st{0.003f};
    float si{-2.0f};

    float c0{-0.75f};
    float cf0{2.0f};
    float cfi{-0.5f};
} ad;

void animate(PushConstants &d, float t, float i)
{
    glm::mat4 m{1.0};
    // m = glm::translate(m, glm::vec3{0.0f, i - 0.5f, 0.0f});
    // m = glm::translate(m, glm::vec3{0.5f, 0.5f, 0.0f});
    m = glm::rotate(m, ad.r0 + ad.rt * t + ad.ri * i + ad.rti * i * t, glm::vec3{0.0, 0.0, 1.0});

    m = glm::scale(m, glm::vec3{ad.s0 + ad.st * t + ad.si * i});
    // m = glm::translate(m, glm::vec3{-0.5f, -0.5f, 0.0f});

    d.transform[0][0] = m[0][0];
    d.transform[1][0] = m[1][0];
    d.transform[0][1] = m[0][1];
    d.transform[1][1] = m[1][1];

    d.offset[0] = m[3][0];
    d.offset[1] = m[3][1];

    float colorFreq = 1.0f / (ad.cf0 + ad.cfi * i);
    float brightness = i + 0.2f * abs(sin(t + i));
    float r = ad.c0 * t * colorFreq;
    glm::vec4 start{0.8f, 0.2f, 0.2f, 1.0f};
    glm::mat4 cm = glm::rotate(
        glm::scale(glm::mat4{1.0f}, glm::vec3{brightness}), r, glm::vec3{1.0f, 1.0f, 1.0f});

    d.color = start * cm;
}

HelloTriangleApplication::HelloTriangleApplication(int argc, char **argv)
    : mesh_{}
    , startupTime_{now()}
{
    Cory::Init();

    CLI::App app{"HelloTriangle"};
    app.add_option("-f,--frames", framesToRender_, "The number of frames to render");
    app.add_flag("--disable-validation", disableValidation_, "Disable validation layers");
    app.parse(argc, argv);

    Cory::ResourceLocator::addSearchPath(TRIANGLE_RESOURCE_DIR);

    init(Cory::ContextCreationInfo{
        .validation =
            disableValidation_ ? Cory::ValidationLayers::Disabled : Cory::ValidationLayers::Enabled,
    });

    // // determine msaa sample count to use - for simplicity, we use either 8 or one sample
    // const auto &limits = ctx().physicalDevice().limits;
    // KDGpu::SampleCountFlags counts =
    //     limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts;
    // // 2 samples are guaranteed to be supported, but we'd rather have 8
    // int msaaSamples = counts.testFlag(KDGpu::SampleCountFlagBits::Samples8Bit) ? 8 : 2;
    int msaaSamples = 1;
    CO_APP_INFO("MSAA sample count: {}", msaaSamples);

    CO_APP_INFO("Vulkan instance version is {}", Cory::queryVulkanInstanceVersion());
    static constexpr auto WINDOW_SIZE = glm::i32vec2{1024, 1024};
    window_ = std::make_unique<Cory::Window>(ctx(), WINDOW_SIZE, "HelloTriangle", msaaSamples);

    createGeometry();
    pipeline_ = std::make_unique<TrianglePipeline>(ctx(),
                                                   *window_,
                                                   *mesh_,
                                                   std::filesystem::path{"simple_shader.vert"},
                                                   std::filesystem::path{"simple_shader.frag"});

    auto recreateSizedResources = [&](Cory::SwapchainResizedEvent) { createFramebuffers(); };
    window_->onSwapchainResized.connect(recreateSizedResources);
    recreateSizedResources({window_->dimensions()});

    Cory::LayerAttachInfo layerAttachInfo{.maxFramesInFlight = Cory::Window::FRAMES_IN_FLIGHT,
                                          .viewportDimensions = window_->dimensions()};
    // imguiLayer_ =
    //     &layers().emplacePriorityLayer<Cory::ImGuiLayer>(layerAttachInfo, std::ref(*window_));
}

HelloTriangleApplication::~HelloTriangleApplication()
{
    CO_APP_TRACE("Destroying HelloTriangleApplication");
}

void HelloTriangleApplication::run()
{
    while (!window_->shouldClose()) {

        Cory::FrameContext frameCtx = window_->nextSwapchainImage();

        layers().update();

        // ImGui::ShowDemoWindow();

        // drawImguiControls();

        recordCommands(frameCtx);

        window_->submitAndPresent(frameCtx);

        // break if number of frames to render are reached
        if (framesToRender_ > 0 && frameCtx.frameNumber >= framesToRender_) { break; }

        // Process KDGui events
        processEvents(0);
    }

    // wait until last frame is finished rendering
    ctx().device().waitUntilIdle();
}

void HelloTriangleApplication::createFramebuffers()
{
    // TODO
}

void HelloTriangleApplication::recordCommands(Cory::FrameContext &frameCtx)
{
    // do some color swirly thingy
    auto t = gsl::narrow_cast<float>(getElapsedTimeSeconds());
    // Magnum::Color4 clearColor{sin(t) / 2.0f + 0.5f, cos(t) / 2.0f + 0.5f, 0.5f};
    glm::vec4 clearColor{0.0f, 0.0f, 0.0f, 1.0f};

    // TODO imgui
    // imguiLayer_->recordFrameCommands(ctx(), frameCtx.index, *frameCtx.commandBuffer);

    auto opaquePassOptions = KDGpu::RenderPassCommandRecorderOptions{
        .colorAttachments = {{
            .view = *frameCtx.swapchainImageView,
            .clearValue = {clearColor.x, clearColor.y, clearColor.z, clearColor.w},
            .finalLayout = KDGpu::TextureLayout::PresentSrc,
        }},
        .depthStencilAttachment = {.view = *frameCtx.depthImageView},
    };

    auto opaquePass = frameCtx.commandBuffer.beginRenderPass(opaquePassOptions);

    opaquePass.setPipeline(pipeline_->pipeline());
    opaquePass.setVertexBuffer(0, mesh_->vertexBuffer);
    opaquePass.setIndexBuffer(mesh_->indexBuffer);
    opaquePass.setScissor({.offset = {0, 0},
                           .extent = {static_cast<uint32_t>(window_->dimensions().x),
                                      static_cast<uint32_t>(window_->dimensions().y)}});
    opaquePass.setViewport({.x = 0.0f,
                            .y = 0.0f,
                            .width = static_cast<float>(window_->dimensions().x),
                            .height = static_cast<float>(window_->dimensions().y),
                            .minDepth = 0.0f,
                            .maxDepth = 1.0f});
    const KDGpu::DrawIndexedCommand drawCmd = {.indexCount = 3};

    PushConstants pushData{};

    for (int idx = 0; idx < ad.num_cubes; ++idx) {
        float i = static_cast<float>(idx) / static_cast<float>(ad.num_cubes - 1);

        animate(pushData, t, i);

        KDGpu::PushConstantRange range{
            .offset = 0,
            .size = sizeof(PushConstants),
            .shaderStages =
                KDGpu::ShaderStageFlagBits::VertexBit | KDGpu::ShaderStageFlagBits::FragmentBit,
        };
        opaquePass.pushConstant(range, &pushData, pipeline_->layout());

        // draw our triangle mesh
        opaquePass.drawIndexed(drawCmd);
    }

    // renderImGuiOverlay(&opaquePass);
    opaquePass.end();
}

// void HelloTriangleApplication::createFramebuffers()
// {
//     auto swapchainExtent = window_->swapchain().extent();
//     Magnum::Vector3i framebufferSize(swapchainExtent.x, swapchainExtent.y, 1);
//
//     framebuffers_ = window_->depthViews() | ranges::views::transform([&](auto &depth) {
//                         auto &color = window_->colorView();
//
//                         return Vk::Framebuffer(
//                             ctx().device(),
//                             Vk::FramebufferCreateInfo{
//                                 pipeline_->mainRenderPass(), {color, depth}, framebufferSize});
//                     }) |
//                     ranges::to<std::vector<Vk::Framebuffer>>;
// }

void HelloTriangleApplication::createGeometry()
{
    struct Vertex {
        glm::vec3 position;
        glm::vec3 color;
    };

    auto &device = ctx().device();

    mesh_ = std::make_unique<Mesh>();

    KDGpu::UploadStagingBuffer vertex_staging_buffer;
    KDGpu::UploadStagingBuffer index_staging_buffer;

    // Create a buffer to hold triangle vertex data
    {
        const float r = 0.8f;

        const float pi = glm::pi<float>();
        const std::array vertexData = {
            Vertex{// Bottom-left, red
                   .position = {r * cosf(7.0f * pi / 6.0f), -r * sinf(7.0f * pi / 6.0f), 0.0f},
                   .color = {1.0f, 0.0f, 0.0f}},
            Vertex{// Bottom-right, green
                   .position = {r * cosf(11.0f * pi / 6.0f), -r * sinf(11.0f * pi / 6.0f), 0.0f},
                   .color = {0.0f, 1.0f, 0.0f}},
            Vertex{// Top, blue
                   .position = {0.0f, -r, 0.0f},
                   .color = {0.0f, 0.0f, 1.0f}}};

        const KDGpu::DeviceSize dataByteSize = vertexData.size() * sizeof(Vertex);
        const KDGpu::BufferOptions bufferOptions = {
            .label = "Vertex Buffer",
            .size = dataByteSize,
            .usage = KDGpu::BufferUsageFlagBits::VertexBufferBit |
                     KDGpu::BufferUsageFlagBits::TransferDstBit,
            .memoryUsage = KDGpu::MemoryUsage::GpuOnly};

        mesh_->vertexBuffer = device.createBuffer(bufferOptions);

        const KDGpu::BufferUploadOptions uploadOptions = {
            .destinationBuffer = mesh_->vertexBuffer,
            .dstStages = KDGpu::PipelineStageFlagBit::VertexAttributeInputBit,
            .dstMask = KDGpu::AccessFlagBit::VertexAttributeReadBit,
            .data = vertexData.data(),
            .byteSize = dataByteSize};

        vertex_staging_buffer = ctx().graphicsQueue().uploadBufferData(uploadOptions);
    }
    // Create a buffer to hold the geometry index data
    {
        std::array<uint32_t, 3> indexData = {0, 1, 2};
        const KDGpu::DeviceSize dataByteSize = indexData.size() * sizeof(uint32_t);
        const KDGpu::BufferOptions bufferOptions = {.label = "Index Buffer",
                                                    .size = dataByteSize,
                                                    .usage =
                                                        KDGpu::BufferUsageFlagBits::IndexBufferBit |
                                                        KDGpu::BufferUsageFlagBits::TransferDstBit,
                                                    .memoryUsage = KDGpu::MemoryUsage::GpuOnly};
        mesh_->indexBuffer = device.createBuffer(bufferOptions);
        const KDGpu::BufferUploadOptions uploadOptions = {
            .destinationBuffer = mesh_->indexBuffer,
            .dstStages = KDGpu::PipelineStageFlagBit::IndexInputBit,
            .dstMask = KDGpu::AccessFlagBit::IndexReadBit,
            .data = indexData.data(),
            .byteSize = dataByteSize};
        index_staging_buffer = ctx().graphicsQueue().uploadBufferData(uploadOptions);
    }
    // Ensure upload is finished.
    index_staging_buffer.fence.wait();
}
double HelloTriangleApplication::now() const
{
    return std::chrono::duration<double>(
               std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

double HelloTriangleApplication::getElapsedTimeSeconds() const { return now() - startupTime_; }
void HelloTriangleApplication::drawImguiControls()
{
    if (ImGui::Begin("Animation Params")) {
        ImGui::InputInt("Triangles", &ad.num_cubes, 1, 10000);

        ImGui::SliderFloat("r0", &ad.r0, -2.0f, 2.0f);
        ImGui::SliderFloat("rt", &ad.rt, -2.0f, 2.0f);
        ImGui::SliderFloat("ri", &ad.ri, -2.0f, 2.0f);
        ImGui::SliderFloat("rti", &ad.rti, -2.0f, 2.0f);
        ImGui::SliderFloat("s0", &ad.s0, -2.0f, 2.0f);
        ImGui::SliderFloat("st", &ad.st, -0.1f, 0.1f);
        ImGui::SliderFloat("si", &ad.si, -2.0f, 2.0f);
        ImGui::SliderFloat("c0", &ad.c0, -2.0f, 2.0f);
        ImGui::SliderFloat("cf0", &ad.cf0, -10.0f, 10.0f);
        ImGui::SliderFloat("cfi", &ad.cfi, -2.0f, 2.0f);
    }
    ImGui::End();
}
