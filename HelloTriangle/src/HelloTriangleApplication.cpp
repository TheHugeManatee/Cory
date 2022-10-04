#include "HelloTriangleApplication.hpp"

#include "TrianglePipeline.hpp"

#include <Cory/Application/Window.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Cory.hpp>
#include <Cory/RenderCore/Context.hpp>
#include <Cory/Renderer/Swapchain.hpp>

#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/Reference.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Vk/CommandBuffer.h>
#include <Magnum/Vk/Device.h>
#include <Magnum/Vk/FramebufferCreateInfo.h>
#include <Magnum/Vk/Queue.h>
#include <Magnum/Vk/RenderPass.h>

#include <GLFW/glfw3.h>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>

#include <math.h>

namespace Vk = Magnum::Vk;

HelloTriangleApplication::HelloTriangleApplication()
{
    Cory::Init();

    Cory::ResourceLocator::addSearchPath(TRIANGLE_RESOURCE_DIR);

    CO_APP_INFO("Vulkan instance version is {}", Cory::queryVulkanInstanceVersion());
    static constexpr auto WINDOW_SIZE = glm::i32vec2{1024, 768};
    ctx_ = std::make_unique<Cory::Context>();
    window_ = std::make_unique<Cory::Window>(*ctx_, WINDOW_SIZE, "HelloTriangle");

    pipeline_ = std::make_unique<TrianglePipeline>(*ctx_,
                                                   window_->swapchain(),
                                                   std::filesystem::path{"simple_shader.vert"},
                                                   std::filesystem::path{"simple_shader.frag"});

    createFramebuffers();
}

HelloTriangleApplication::~HelloTriangleApplication() = default;

void HelloTriangleApplication::run()
{
    while (!window_->shouldClose()) {
        glfwPollEvents();
        // TODO process events?
        auto frameCtx = window_->swapchain().nextImage();

        if (frameCtx.shouldRecreateSwapChain) {
            throw std::logic_error{"Swapchain recreation not implemented yet!"};
        }

        recordCommands(frameCtx);

        // todo refactor this somewhat
        std::vector<VkSemaphore> waitSemaphores{*frameCtx.acquired};
        std::vector<VkPipelineStageFlags> waitStages{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        std::vector<VkSemaphore> signalSemaphores{*frameCtx.rendered};
        Vk::SubmitInfo submitInfo{};
        submitInfo.setCommandBuffers({frameCtx.commandBuffer});
        submitInfo->pWaitSemaphores = waitSemaphores.data();
        submitInfo->waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
        submitInfo->pWaitDstStageMask = waitStages.data();
        submitInfo->pSignalSemaphores = signalSemaphores.data();
        submitInfo->signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());

        ctx_->graphicsQueue().submit({submitInfo}, *frameCtx.inFlight);
        // AAAAAAAAAAAAAAAAAAAAHHHHHHHHHHHHHH (todo: fix command buffer lifetime)
        frameCtx.inFlight->wait();

        window_->swapchain().present(frameCtx);
    }

    // wait until last frame is finished rendering
    ctx_->device()->DeviceWaitIdle(ctx_->device());
}
void HelloTriangleApplication::recordCommands(Cory::FrameContext &frameCtx)
{
    // do some color swirly thingy
    float t = (float)frameCtx.frameNumber / 10000.0f;
    Magnum::Color4 clearColor{sin(t) / 2.0f + 0.5f, cos(t) / 2.0f + 0.5f, 0.5f};

    Vk::CommandBuffer &cmdBuffer = frameCtx.commandBuffer;

    cmdBuffer.begin(Vk::CommandBufferBeginInfo{});
    cmdBuffer.bindPipeline(pipeline_->pipeline());
    cmdBuffer.beginRenderPass(
        // VK_SUBPASS_CONTENTS_INLINE is implicit somewhere in here I assume
        Vk::RenderPassBeginInfo{pipeline_->mainRenderPass(), frameBuffers_[frameCtx.index]}
            .clearColor(0, clearColor)
            .clearDepthStencil(1, 1.0, 0));

    // draw our non-existent triangle
    ctx_->device()->CmdDraw(cmdBuffer, 3, 1, 0, 0);

    cmdBuffer.endRenderPass();

    cmdBuffer.end();
}

void HelloTriangleApplication::createFramebuffers()
{
    auto swapchainExtent = window_->swapchain().extent();
    Magnum::Vector3i framebufferSize(swapchainExtent.x, swapchainExtent.y, 1);

    frameBuffers_ =
        ranges::views::zip(window_->swapchain().imageViews(), window_->swapchain().depthViews()) |
        ranges::views::transform([&](auto views) {
            auto &[colorView, depthView] = views;

            return Vk::Framebuffer(ctx_->device(),
                                   Vk::FramebufferCreateInfo{pipeline_->mainRenderPass(),
                                                             {colorView, depthView},
                                                             framebufferSize});
        }) |
        ranges::to<std::vector<Vk::Framebuffer>>;
}
