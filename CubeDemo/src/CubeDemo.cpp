#include "CubeDemo.hpp"

#include "CubePipeline.hpp"

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

// Matrix corresponds to Translate * Ry * Rx * Rz * Scale
// Rotations correspond to Tait-bryan angles of Y(1), X(2), Z(3)
// https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
glm::mat4 makeTransform(glm::vec3 translation, glm::vec3 rotation, glm::vec3 scale)
{
    const float c3 = glm::cos(rotation.z);
    const float s3 = glm::sin(rotation.z);
    const float c2 = glm::cos(rotation.x);
    const float s2 = glm::sin(rotation.x);
    const float c1 = glm::cos(rotation.y);
    const float s1 = glm::sin(rotation.y);
    return glm::mat4{{
                         scale.x * (c1 * c3 + s1 * s2 * s3),
                         scale.x * (c2 * s3),
                         scale.x * (c1 * s2 * s3 - c3 * s1),
                         0.0f,
                     },
                     {
                         scale.y * (c3 * s1 * s2 - c1 * s3),
                         scale.y * (c2 * c3),
                         scale.y * (c1 * c3 * s2 + s1 * s3),
                         0.0f,
                     },
                     {
                         scale.z * (c2 * s1),
                         scale.z * (-s2),
                         scale.z * (c1 * c2),
                         0.0f,
                     },
                     {translation.x, translation.y, translation.z, 1.0f}};
}

struct PushConstants {
    glm::mat4 transform{1.0f};
    glm::vec4 color{1.0, 0.0, 0.0, 1.0};
    float blend;
};

static struct AnimationData {
    int num_cubes{1};
    float blend{0.5f};

    float r0{0.0f};
    float rt{-0.1f};
    float ri{0.0f};
    float rti{0.68f};
    float s0{0.5f};
    float st{0.0f};
    float si{0.0f};

    float c0{-0.75f};
    float cf0{2.0f};
    float cfi{-0.5f};

    glm::vec3 translation{0.0};
    glm::vec3 rotation{0.0};
} ad;

void animate(PushConstants &d, float t, float i)
{
    glm::mat4 m{1.0};
    // m = glm::translate(m, glm::vec3{0.0f, i - 0.5f, 0.0f});
    // m = glm::translate(m, glm::vec3{0.5f, 0.5f, 0.0f});
    m = glm::translate(m, ad.translation);
    glm::vec3 axis = glm::normalize(glm::vec3{0.0, 0.3, 1.0});
    float angle = ad.r0 + ad.rt * t + ad.ri * i + ad.rti * i * t;
    m = glm::rotate(m, angle, axis);

    float scale = ad.s0 + ad.st * t + ad.si * i;
    m = glm::scale(m, glm::vec3{scale});
    // m = glm::translate(m, glm::vec3{-0.5f, -0.5f, 0.0f});

    d.transform = makeTransform(ad.translation, ad.rotation, glm::vec3{scale});

    float colorFreq = 1.0f / (ad.cf0 + ad.cfi * i);
    float brightness = i + 0.2f * abs(sin(t + i));
    float r = ad.c0 * t * colorFreq;
    glm::vec4 start{0.8f, 0.2f, 0.2f, 1.0f};
    glm::mat4 cm = glm::rotate(
        glm::scale(glm::mat4{1.0f}, glm::vec3{brightness}), r, glm::vec3{1.0f, 1.0f, 1.0f});

    d.color = start * cm;
    d.blend = ad.blend;
}

CubeDemoApplication::CubeDemoApplication(int argc, char **argv)
    : mesh_{}
    , imguiLayer_{std::make_unique<Cory::ImGuiLayer>()}
    , startupTime_{now()}
{
    Cory::Init();

    CLI::App app{"CubeDemo"};
    app.add_option("-f,--frames", framesToRender_, "The number of frames to render");
    app.parse(argc, argv);

    Cory::ResourceLocator::addSearchPath(CUBEDEMO_RESOURCE_DIR);

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
    window_ = std::make_unique<Cory::Window>(*ctx_, WINDOW_SIZE, "CubeDemo", msaaSamples);

    auto recreateSizedResources = [&](glm::i32vec2) { createFramebuffers(); };

    createGeometry();
    pipeline_ = std::make_unique<CubePipeline>(*ctx_,
                                               *window_,
                                               *mesh_,
                                               std::filesystem::path{"cube.vert"},
                                               std::filesystem::path{"cube.frag"});
    createFramebuffers();
    recreateSizedResources(window_->dimensions());
    window_->onSwapchainResized.connect(recreateSizedResources);

    imguiLayer_->init(*window_, *ctx_);
}

CubeDemoApplication::~CubeDemoApplication()
{
    imguiLayer_->deinit(*ctx_);
    CO_APP_TRACE("Destroying CubeDemoApplication");
}

void CubeDemoApplication::run()
{
    while (!window_->shouldClose()) {
        glfwPollEvents();
        imguiLayer_->newFrame(*ctx_);
        // TODO process events?
        Cory::FrameContext frameCtx = window_->nextSwapchainImage();

        ImGui::DockSpaceOverViewport();
        ImGui::ShowDemoWindow();

        drawImguiControls();

        recordCommands(frameCtx);

        window_->submitAndPresent(std::move(frameCtx));

        // break if number of frames to render are reached
        if (framesToRender_ > 0 && frameCtx.frameNumber >= framesToRender_) { break; }


    }

    // wait until last frame is finished rendering
    ctx_->device()->DeviceWaitIdle(ctx_->device());
}

void CubeDemoApplication::recordCommands(Cory::FrameContext &frameCtx)
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

    // ctx_->device()->CmdSetCullMode(cmdBuffer, VkCullModeFlagBits::VK_CULL_MODE_NONE);

    PushConstants pushData{};

    for (int idx = 0; idx < ad.num_cubes; ++idx) {
        float i = ad.num_cubes == 1
                      ? 1.0f
                      : static_cast<float>(idx) / static_cast<float>(ad.num_cubes - 1);

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

    imguiLayer_->recordFrameCommands(*ctx_, frameCtx);

    cmdBuffer.end();
}

void CubeDemoApplication::createFramebuffers()
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

void CubeDemoApplication::createGeometry()
{
    mesh_ = std::make_unique<Vk::Mesh>(Cory::DynamicGeometry::createCube(*ctx_));
}
double CubeDemoApplication::now() const
{
    return std::chrono::duration<double>(
               std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

double CubeDemoApplication::getElapsedTimeSeconds() const { return now() - startupTime_; }
void CubeDemoApplication::drawImguiControls()
{
    if (ImGui::Begin("Animation Params")) {
        ImGui::InputInt("Cubes", &ad.num_cubes, 1, 10000);
        ImGui::SliderFloat("blend", &ad.blend, 0.0f, 1.0f);
        ImGui::SliderFloat3("translation", &ad.translation.x, -1.0f, 1.0f);
        ImGui::SliderFloat3("rotation", &ad.rotation.x, -glm::pi<float>(), glm::pi<float>());

        ImGui::SliderFloat("r0", &ad.r0, -2.0f, 2.0f);
        ImGui::SliderFloat("rt", &ad.rt, -2.0f, 2.0f);
        ImGui::SliderFloat("ri", &ad.ri, -2.0f, 2.0f);
        ImGui::SliderFloat("rti", &ad.rti, -2.0f, 2.0f);
        ImGui::SliderFloat("s0", &ad.s0, -2.0f, 2.0f);
        ImGui::SliderFloat("st", &ad.st, -0.1f, 0.1f);
        ImGui::SliderFloat("si", &ad.si, 0.0f, 2.0f);
        ImGui::SliderFloat("c0", &ad.c0, -2.0f, 2.0f);
        ImGui::SliderFloat("cf0", &ad.cf0, -10.0f, 10.0f);
        ImGui::SliderFloat("cfi", &ad.cfi, -2.0f, 2.0f);
    }
    ImGui::End();
}
