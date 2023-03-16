#include "HelloTriangleApplication.hpp"

#include "TrianglePipeline.hpp"

#include <Cory/Application/DynamicGeometry.hpp>
#include <Cory/Application/ImGuiLayer.hpp>
#include <Cory/Application/Window.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Base/Profiling.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Cory.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/Swapchain.hpp>

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/Reference.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Vk/BufferCreateInfo.h>
#include <Magnum/Vk/CommandBuffer.h>
#include <Magnum/Vk/Device.h>
#include <Magnum/Vk/DeviceProperties.h>
#include <Magnum/Vk/FramebufferCreateInfo.h>
#include <Magnum/Vk/Mesh.h>
#include <Magnum/Vk/PipelineLayout.h>
#include <Magnum/Vk/Queue.h>
#include <Magnum/Vk/RenderPass.h>
#include <Magnum/Vk/VertexFormat.h>

#include <CLI/App.hpp>
#include <CLI/CLI.hpp>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat2x2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <imgui.h>

#include <gsl/gsl>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>

#include <array>
#include <chrono>

namespace Vk = Magnum::Vk;

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
    , imguiLayer_{std::make_unique<Cory::ImGuiLayer>()}
    , startupTime_{now()}
{
    Cory::Init();

    CLI::App app{"HelloTriangle"};
    app.add_option("-f,--frames", framesToRender_, "The number of frames to render");
    app.parse(argc, argv);

    Cory::ResourceLocator::addSearchPath(TRIANGLE_RESOURCE_DIR);

    ctx_ = std::make_unique<Cory::Context>();
    // determine msaa sample count to use - for simplicity, we use either 8 or one sample
    const auto &limits = ctx_->physicalDevice().properties().properties.limits;
    VkSampleCountFlags counts =
        limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts;
    // 2 samples are guaranteed to be supported, but we'd rather have 8
    int msaaSamples = counts & VK_SAMPLE_COUNT_8_BIT ? 8 : 2;
    CO_APP_INFO("MSAA sample count: {}", msaaSamples);

    CO_APP_INFO("Vulkan instance version is {}", Cory::queryVulkanInstanceVersion());
    static constexpr auto WINDOW_SIZE = glm::i32vec2{1024, 1024};
    window_ = std::make_unique<Cory::Window>(*ctx_, WINDOW_SIZE, "HelloTriangle", msaaSamples);

    auto recreateSizedResources = [&](Cory::SwapchainResizedEvent) { createFramebuffers(); };

    createGeometry();
    pipeline_ = std::make_unique<TrianglePipeline>(*ctx_,
                                                   *window_,
                                                   *mesh_,
                                                   std::filesystem::path{"simple_shader.vert"},
                                                   std::filesystem::path{"simple_shader.frag"});
    createFramebuffers();
    recreateSizedResources({window_->dimensions()});
    window_->onSwapchainResized.connect(recreateSizedResources);

    imguiLayer_->init(*window_, *ctx_);
}

HelloTriangleApplication::~HelloTriangleApplication()
{
    imguiLayer_->deinit(*ctx_);
    CO_APP_TRACE("Destroying HelloTriangleApplication");
}

void HelloTriangleApplication::run()
{
    while (!window_->shouldClose()) {
        glfwPollEvents();
        imguiLayer_->newFrame(*ctx_);
        // TODO process events?
        Cory::FrameContext frameCtx = window_->nextSwapchainImage();

        ImGui::ShowDemoWindow();

        drawImguiControls();

        recordCommands(frameCtx);

        window_->submitAndPresent(frameCtx);

        // break if number of frames to render are reached
        if (framesToRender_ > 0 && frameCtx.frameNumber >= framesToRender_) { break; }
    }

    // wait until last frame is finished rendering
    ctx_->device()->DeviceWaitIdle(ctx_->device());
}

void HelloTriangleApplication::recordCommands(Cory::FrameContext &frameCtx)
{
    // do some color swirly thingy
    auto t = gsl::narrow_cast<float>(getElapsedTimeSeconds());
    // Magnum::Color4 clearColor{sin(t) / 2.0f + 0.5f, cos(t) / 2.0f + 0.5f, 0.5f};
    Magnum::Color4 clearColor{0.0f, 0.0f, 0.0f, 1.0f};

    Vk::CommandBuffer &cmdBuffer = *frameCtx.commandBuffer;

    cmdBuffer.begin(Vk::CommandBufferBeginInfo{});
    cmdBuffer.bindPipeline(pipeline_->pipeline());
    cmdBuffer.beginRenderPass(
        Vk::RenderPassBeginInfo{pipeline_->mainRenderPass(), framebuffers_[frameCtx.index]}
            .clearColor(0, clearColor)
            .clearDepthStencil(1, 1.0, 0));

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(window_->dimensions().x);
    viewport.height = static_cast<float>(window_->dimensions().y);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{{0, 0},
                     {static_cast<uint32_t>(window_->dimensions().x),
                      static_cast<uint32_t>(window_->dimensions().y)}};
    ctx_->device()->CmdSetViewport(cmdBuffer, 0, 1, &viewport);
    ctx_->device()->CmdSetScissor(cmdBuffer, 0, 1, &scissor);

    PushConstants pushData{};

    for (int idx = 0; idx < ad.num_cubes; ++idx) {
        float i = static_cast<float>(idx) / static_cast<float>(ad.num_cubes - 1);

        animate(pushData, t, i);

        ctx_->device()->CmdPushConstants(cmdBuffer,
                                         pipeline_->layout(),
                                         VkShaderStageFlagBits::VK_SHADER_STAGE_ALL,
                                         0,
                                         sizeof(pushData),
                                         &pushData);

        // draw our triangle mesh
        cmdBuffer.draw(*mesh_);
    }

    cmdBuffer.endRenderPass();

    imguiLayer_->recordFrameCommands(*ctx_, frameCtx.index, *frameCtx.commandBuffer);

    cmdBuffer.end();
}

void HelloTriangleApplication::createFramebuffers()
{
    auto swapchainExtent = window_->swapchain().extent();
    Magnum::Vector3i framebufferSize(swapchainExtent.x, swapchainExtent.y, 1);

    framebuffers_ = window_->depthViews() | ranges::views::transform([&](auto &depth) {
                        auto &color = window_->colorView();

                        return Vk::Framebuffer(
                            ctx_->device(),
                            Vk::FramebufferCreateInfo{
                                pipeline_->mainRenderPass(), {color, depth}, framebufferSize});
                    }) |
                    ranges::to<std::vector<Vk::Framebuffer>>;
}

void HelloTriangleApplication::createGeometry()
{
    mesh_ = std::make_unique<Vk::Mesh>(Cory::DynamicGeometry::createTriangle(*ctx_));
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
