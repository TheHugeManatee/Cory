#include "CubeDemo.hpp"

#include "CubePipeline.hpp"

#include <Cory/Application/DynamicGeometry.hpp>
#include <Cory/Application/ImGuiLayer.hpp>
#include <Cory/Application/Window.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Base/Math.hpp>
#include <Cory/Base/Profiling.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Cory.hpp>
#include <Cory/ImGui/Inputs.hpp>
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
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <imgui.h>

#include <gsl/gsl>
#include <gsl/narrow>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>

#include <chrono>

namespace Vk = Magnum::Vk;

struct PushConstants {
    glm::mat4 modelTransform{1.0f};
    glm::vec4 color{1.0, 0.0, 0.0, 1.0};
    float blend;
};

static struct AnimationData {
    int num_cubes{200};
    float blend{0.8f};

    float ti{1.5f};
    float tsi{2.0f};
    float tsf{100.0f};

    float r0{0.0f};
    float rt{-0.1f};
    float ri{1.3f};
    float rti{0.05f};

    float s0{0.05f};
    float st{0.0f};
    float si{0.4f};

    float c0{-0.75f};
    float cf0{2.0f};
    float cfi{-0.5f};

    glm::vec3 translation{0.0, 0.0f, 2.5f};
    glm::vec3 rotation{0.0f};
} ad;

void animate(PushConstants &d, float t, float i)
{

    float angle = ad.r0 + ad.rt * t + ad.ri * i + ad.rti * i * t;
    float scale = ad.s0 + ad.st * t + ad.si * i;

    float tsf = ad.tsf / 2.0f + ad.tsf * sin(t / 10.0f);
    glm::vec3 translation{sin(i * tsf) * i * ad.tsi, cos(i * tsf) * i * ad.tsi, i * ad.ti};

    d.modelTransform = Cory::makeTransform(ad.translation + translation,
                                           ad.rotation + glm::vec3{0.0f, angle, angle / 2.0f},
                                           glm::vec3{scale});

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

    camera_.setMode(Cory::CameraManipulator::Mode::Fly);
    camera_.setWindowSize(window_->dimensions());
    camera_.setLookat({0.0f, 3.0f, 2.5f}, {0.0f, 4.0f, 2.0f}, {0.0f, 1.0f, 0.0f});
    setupCameraCallbacks();

    // create and initialize descriptor sets for each frame in flight
    globalUbo_ = std::make_unique<Cory::UniformBufferObject<CubeUBO>>(
        *ctx_, window_->swapchain().maxFramesInFlight());
    for (gsl::index i = 0; i < globalUbo_->instances(); ++i) {
        auto set = pipeline_->allocateDescriptorSet();

        auto bufferInfo = globalUbo_->descriptorInfo(i);
        VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = set,
            .dstBinding = 0, // TODO?
            .descriptorCount = 1,
            .descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &bufferInfo,
        };
        ctx_->device()->UpdateDescriptorSets(ctx_->device(), 1, &write, 0, nullptr);

        descriptorSets_.push_back(std::move(set));
    }
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

    ctx_->device()->CmdSetCullMode(cmdBuffer, VkCullModeFlagBits::VK_CULL_MODE_BACK_BIT);
    PushConstants pushData{};

    float fovy = glm::radians(70.0f);
    glm::vec2 wndSize = window_->dimensions();
    float aspect = wndSize.x / wndSize.y;
    glm::mat4 viewMatrix = camera_.getViewMatrix();
    glm::mat4 projectionMatrix = Cory::makePerspective(fovy, aspect, 0.1f, 10.0f);
    glm::mat4 viewProjection = projectionMatrix * viewMatrix;

    CubeUBO &ubo = (*globalUbo_)[frameCtx.index];
    ubo.view = viewMatrix;
    ubo.projection = projectionMatrix;
    ubo.viewProjection = viewProjection;
    // need explicit flush otherwise the mapped memory is not synced to the GPU
    globalUbo_->flush(frameCtx.index);

    const std::vector sets{descriptorSets_[frameCtx.index].handle()};
    ctx_->device()->CmdBindDescriptorSets(cmdBuffer,
                                          VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS,
                                          pipeline_->layout(),
                                          0,
                                          gsl::narrow<uint32_t>(sets.size()),
                                          sets.data(),
                                          0,
                                          nullptr);

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
    namespace CoImGui = Cory::ImGui;
    if (ImGui::Begin("Animation Params")) {
        if (ImGui::Button("Restart")) { startupTime_ = now(); }

        CoImGui::Input("Cubes", ad.num_cubes, 1, 10000);
        CoImGui::Slider("blend", ad.blend, 0.0f, 1.0f);
        CoImGui::Slider("translation", ad.translation, -3.0f, 3.0f);
        CoImGui::Slider("rotation", ad.rotation, -glm::pi<float>(), glm::pi<float>());

        CoImGui::Slider("ti", ad.ti, 0.0f, 10.0f);
        CoImGui::Slider("tsi", ad.tsi, 0.0f, 10.0f);
        CoImGui::Slider("tsf", ad.tsf, 0.0f, 250.0f);

        CoImGui::Slider("r0", ad.r0, -2.0f, 2.0f);
        CoImGui::Slider("rt", ad.rt, -2.0f, 2.0f);
        CoImGui::Slider("ri", ad.ri, -2.0f, 2.0f);
        CoImGui::Slider("rti", ad.rti, -2.0f, 2.0f);
        CoImGui::Slider("s0", ad.s0, 0.0f, 2.0f);
        CoImGui::Slider("st", ad.st, -0.1f, 0.1f);
        CoImGui::Slider("si", ad.si, 0.0f, 2.0f);
        CoImGui::Slider("c0", ad.c0, -2.0f, 2.0f);
        CoImGui::Slider("cf0", ad.cf0, -10.0f, 10.0f);
        CoImGui::Slider("cfi", ad.cfi, -2.0f, 2.0f);
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
}
void CubeDemoApplication::setupCameraCallbacks()
{
    window_->onSwapchainResized.connect([this](glm::i32vec2 size) { camera_.setWindowSize(size); });

    window_->onMouseMoved.connect([this](glm::vec2 pos) {
        if (ImGui::GetIO().WantCaptureMouse) { return; }
        auto wnd = window_->handle();
        Cory::CameraManipulator::MouseButton mouseButton =
            (glfwGetMouseButton(wnd, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
                ? Cory::CameraManipulator::MouseButton::Left
            : (glfwGetMouseButton(wnd, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS)
                ? Cory::CameraManipulator::MouseButton::Middle
            : (glfwGetMouseButton(wnd, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
                ? Cory::CameraManipulator::MouseButton::Right
                : Cory::CameraManipulator::MouseButton::None;
        if (mouseButton != Cory::CameraManipulator::MouseButton::None) {
            Cory::CameraManipulator::ModifierFlags modifiers;
            if (glfwGetKey(wnd, GLFW_KEY_LEFT_ALT) == GLFW_PRESS) {
                modifiers.set(Cory::CameraManipulator::ModifierFlagBits::Alt);
            }
            if (glfwGetKey(wnd, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
                modifiers.set(Cory::CameraManipulator::ModifierFlagBits::Ctrl);
            }
            if (glfwGetKey(wnd, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
                modifiers.set(Cory::CameraManipulator::ModifierFlagBits::Shift);
            }

            camera_.mouseMove(glm::ivec2(pos), mouseButton, modifiers);
        }
    });
    window_->onMouseButton.connect([this](Cory::Window::MouseButtonData data) {
        if (ImGui::GetIO().WantCaptureMouse) { return; }
        camera_.setMousePosition(data.position);
    });
    window_->onMouseScrolled.connect([this](Cory::Window::ScrollData data) {
        if (ImGui::GetIO().WantCaptureMouse) { return; }
        camera_.wheel(static_cast<int32_t>(data.scrollDelta.y));
    });
}
