#include "HelloTriangleApplication.hpp"

#include "TrianglePipeline.hpp"

#include <Cory/Application/ImGuiLayer.hpp>
#include <Cory/Application/Window.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Base/Profiling.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Cory.hpp>
#include <Cory/RenderCore/Context.hpp>
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
#include <Magnum/Vk/Queue.h>
#include <Magnum/Vk/RenderPass.h>
#include <Magnum/Vk/VertexFormat.h>

#include <GLFW/glfw3.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <imgui.h>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>

#include <math.h>

#include <array>

namespace Vk = Magnum::Vk;

HelloTriangleApplication::HelloTriangleApplication()
    : mesh_{}
    , imguiLayer_{std::make_unique<Cory::ImGuiLayer>()}
{
    Cory::Init();

    Cory::ResourceLocator::addSearchPath(TRIANGLE_RESOURCE_DIR);

    ctx_ = std::make_unique<Cory::Context>();
    // determine msaa sample count to use - for simplicity, we use either 8 or one sample
    const auto &limits = ctx_->physicalDevice().properties().properties.limits;
    VkSampleCountFlags counts =
        limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts;
    int msaaSamples = counts & VK_SAMPLE_COUNT_8_BIT ? 8 : 1;
    CO_APP_INFO("MSAA sample count: {}", msaaSamples);

    CO_APP_INFO("Vulkan instance version is {}", Cory::queryVulkanInstanceVersion());
    static constexpr auto WINDOW_SIZE = glm::i32vec2{1024, 768};
    window_ = std::make_unique<Cory::Window>(*ctx_, WINDOW_SIZE, "HelloTriangle", msaaSamples);

    auto recreatePipeline = [&](glm::i32vec2) {
        pipeline_ = std::make_unique<TrianglePipeline>(*ctx_,
                                                       *window_,
                                                       *mesh_,
                                                       std::filesystem::path{"simple_shader.vert"},
                                                       std::filesystem::path{"simple_shader.frag"});
        createFramebuffers();
    };

    createGeometry();
    recreatePipeline(window_->dimensions());
    // we currently don't use dynamic pipeline state so we have to
    // recreate the full pipeline when swapchain has been resized
    window_->onSwapchainResized(recreatePipeline);

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
        auto frameCtx = window_->nextSwapchainImage();

        ImGui::ShowDemoWindow();

        recordCommands(frameCtx);

        window_->submitAndPresent(std::move(frameCtx));
    }

    // wait until last frame is finished rendering
    ctx_->device()->DeviceWaitIdle(ctx_->device());
}
void HelloTriangleApplication::recordCommands(Cory::FrameContext &frameCtx)
{
    // do some color swirly thingy
    float t = (float)frameCtx.frameNumber / 10000.0f;
    Magnum::Color4 clearColor{sin(t) / 2.0f + 0.5f, cos(t) / 2.0f + 0.5f, 0.5f};

    Vk::CommandBuffer &cmdBuffer = *frameCtx.commandBuffer;

    cmdBuffer.begin(Vk::CommandBufferBeginInfo{});
    cmdBuffer.bindPipeline(pipeline_->pipeline());
    cmdBuffer.beginRenderPass(
        Vk::RenderPassBeginInfo{pipeline_->mainRenderPass(), framebuffers_[frameCtx.index]}
            .clearColor(0, clearColor)
            .clearDepthStencil(1, 1.0, 0));

    // draw our triangle mesh
    cmdBuffer.draw(*mesh_);

    cmdBuffer.endRenderPass();

    imguiLayer_->recordFrameCommands(*ctx_, frameCtx);

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
#pragma pack(push, 1)
    // note: currently can't use glm types because we use
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 tex;
        glm::vec4 col;
    };
#pragma pack(pop)
    static_assert(sizeof(Vertex) == 10 * sizeof(float));

    uint32_t binding = 0;
    mesh_ = std::make_unique<Vk::Mesh>(
        Vk::MeshLayout{Vk::MeshPrimitive::Triangles}
            .addBinding(binding, sizeof(Vertex))
            .addAttribute(0, binding, Vk::VertexFormat::Vector3, 0)
            .addAttribute(1, binding, Vk::VertexFormat::Vector3, 3 * sizeof(float))
            .addAttribute(2, binding, Vk::VertexFormat::Vector4, 6 * sizeof(float)));

    // set up the fixed mesh - note that the 'data' variable manages
    // the lifetime of the memory mapping
    {
        const uint64_t numVertices = 3;
        Vk::Buffer vertices{
            ctx_->device(),
            Vk::BufferCreateInfo{Vk::BufferUsage::VertexBuffer, numVertices * sizeof(Vertex)},
            Magnum::Vk::MemoryFlag::HostCoherent | Magnum::Vk::MemoryFlag::HostVisible};

        Corrade::Containers::Array<char, Vk::MemoryMapDeleter> data =
            vertices.dedicatedMemory().map();

        auto &view = reinterpret_cast<std::array<Vertex, 3> &>(*data.data());
        view[0] = Vertex{{0.0f, -0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}};
        view[1] = Vertex{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}};
        view[2] = Vertex{{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}};
        mesh_->addVertexBuffer(0, std::move(vertices), 0).setCount(numVertices);
    }
}
