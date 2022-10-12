#include <Cory/Application/ImGuiLayer.hpp>

#include <Cory/Application/Window.hpp>
#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/RenderCore/Context.hpp>
#include <Cory/RenderCore/SingleShotCommandBuffer.hpp>
#include <Cory/RenderCore/VulkanUtils.hpp>
#include <Cory/Renderer/Swapchain.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <Magnum/Math/Color.h>
#include <Magnum/Vk/Device.h>
#include <Magnum/Vk/DeviceProperties.h>
#include <Magnum/Vk/FramebufferCreateInfo.h>
#include <Magnum/Vk/Instance.h>
#include <Magnum/Vk/Pipeline.h>
#include <Magnum/Vk/Queue.h>
#include <Magnum/Vk/RenderPassCreateInfo.h>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>

#include <kdbindings/signal.h>

namespace Vk = Magnum::Vk;

namespace Cory {

namespace {

[[nodiscard]] BasicVkObjectWrapper<VkDescriptorPool> createImguiDescriptorPool(Context &ctx)
{
    VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
                                         {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

    VkDescriptorPoolCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1000 * IM_ARRAYSIZE(pool_sizes),
        .poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes),
        .pPoolSizes = pool_sizes,
    };
    VkDescriptorPool descriptorPool;
    ctx.device()->CreateDescriptorPool(ctx.device(), &createInfo, nullptr, &descriptorPool);
    return {descriptorPool, [&device = ctx.device()](auto *ptr) {
                device->DestroyDescriptorPool(device, ptr, nullptr);
            }};
}

Vk::RenderPass createImguiRenderpass(Context &ctx, Vk::PixelFormat format, int32_t msaaSamples)
{
    return Vk::RenderPass(
        ctx.device(),
        Vk::RenderPassCreateInfo{}
            .setAttachments(
                // color
                {Vk::AttachmentDescription{
                     format,
                     {Vk::AttachmentLoadOperation::Load, Vk::AttachmentLoadOperation::DontCare},
                     {Vk::AttachmentStoreOperation::Store, Vk::AttachmentStoreOperation::DontCare},
                     Vk::ImageLayout::ColorAttachment, // initialLayout
                     Vk::ImageLayout::ColorAttachment, // finalLayout
                     msaaSamples},
                 // resolve
                 Vk::AttachmentDescription{
                     format,
                     {Vk::AttachmentLoadOperation::Clear, Vk::AttachmentLoadOperation::DontCare},
                     {Vk::AttachmentStoreOperation::Store, Vk::AttachmentStoreOperation::DontCare},
                     Vk::ImageLayout::Undefined,                       // initialLayout
                     Vk::ImageLayout{VK_IMAGE_LAYOUT_PRESENT_SRC_KHR}, // finalLayout
                     1}})
            .addSubpass(Vk::SubpassDescription{}.setColorAttachments(
                {Vk::AttachmentReference{0, Vk::ImageLayout::ColorAttachment}},
                {Vk::AttachmentReference{1, Vk::ImageLayout::ColorAttachment}}))
            .setDependencies({Vk::SubpassDependency{
                Vk::SubpassDependency::External,          // srcSubpass
                0,                                        // dstSubpass
                Vk::PipelineStage::ColorAttachmentOutput, // srcStages
                Vk::PipelineStage::ColorAttachmentOutput, // dstStages
                Vk::Access::ColorAttachmentWrite,         // srcAccess
                Vk::Access::ColorAttachmentWrite          // dstAccess
            }}));
}

std::vector<Vk::Framebuffer>
createFramebuffers(Context &ctx, Window &window, Vk::RenderPass &renderPass)
{
    const auto swapchainExtent = window.dimensions();
    const Magnum::Vector3i framebufferSize(swapchainExtent.x, swapchainExtent.y, 1);

    return window.swapchain().imageViews() | ranges::views::transform([&](auto &swapchainImage) {
               auto &colorView = window.colorView();

               return Vk::Framebuffer(ctx.device(),
                                      Vk::FramebufferCreateInfo{renderPass,
                                                                {colorView, swapchainImage},
                                                                framebufferSize});
           }) |
           ranges::_to_::to<std::vector<Vk::Framebuffer>>;
}

} // namespace

struct ImGuiLayer::Private {
    Vk::RenderPass renderPass{Corrade::NoCreate};
    BasicVkObjectWrapper<VkDescriptorPool> descriptorPool;
    std::vector<Vk::Framebuffer> framebuffers;
    Magnum::Color4 clearValue{};
    KDBindings::ConnectionHandle handle{};
};

void check_vk_result(VkResult err)
{
    if (err == 0) { return; }
    CO_CORE_ERROR("[ImGui] Vulkan Error: VkResult = {}\n", err);
    if (err < 0) { abort(); }
}

ImGuiLayer::ImGuiLayer()
    : data_{std::make_unique<Private>()}
{
    data_->clearValue = {0.0f, 0.0f, 0.0f, 0.0f};
}

ImGuiLayer::~ImGuiLayer()
{
    data_->handle.disconnect();
}

void ImGuiLayer::init(Window &window, Context &ctx)
{
    data_->descriptorPool = createImguiDescriptorPool(ctx);

    auto recreateSizedResources = [&](glm::i32vec2 s) {
        data_->renderPass = createImguiRenderpass(ctx, window.colorFormat(), window.sampleCount());
        data_->framebuffers = createFramebuffers(ctx, window, data_->renderPass);
    };
    recreateSizedResources({});
    data_->handle = window.onSwapchainResized.connect(recreateSizedResources);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable
    // Keyboard Controls io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; //
    // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsClassic();

    ImGui_ImplGlfw_InitForVulkan(window.handle(), true);

    ImGui_ImplVulkan_InitInfo info{};
    info.Instance = ctx.instance();
    info.PhysicalDevice = ctx.physicalDevice();
    info.Device = ctx.device();
    info.QueueFamily = ctx.graphicsQueueFamily();
    info.Queue = ctx.graphicsQueue();
    info.PipelineCache = nullptr; // TODO?
    info.DescriptorPool = data_->descriptorPool;
    info.Allocator = nullptr;
    info.MinImageCount = 2;
    info.ImageCount = static_cast<uint32_t>(window.swapchain().size());
    info.Subpass = 0;
    info.CheckVkResultFn = check_vk_result;
    info.MSAASamples = (VkSampleCountFlagBits)window.sampleCount();

    info.MinImageCount = 2;
    ImGui_ImplVulkan_Init(&info, data_->renderPass);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can
    // also load multiple fonts and use ImGui::PushFont()/PopFont() to select
    // them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you
    // need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please
    // handle those errors in your application (e.g. use an assertion, or display
    // an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored
    // into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which
    // ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string
    // literal you need to write a double backslash \\ !
    // io.Fonts->AddFontDefault();
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    // ImFont* font =
    // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f,
    // NULL, io.Fonts->GetGlyphRangesJapanese()); IM_ASSERT(font != NULL);

    // Upload Fonts
    {
        // Use any command queue
        // VkCommandPool command_pool = wd->Frames[wd->FrameIndex].CommandPool;
        // VkCommandBuffer command_buffer =
        // wd->Frames[wd->FrameIndex].CommandBuffer;

        // err = vkResetCommandPool(g_Device, command_pool, 0);
        // check_vk_result(err);
        /*VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(command_buffer, &begin_info);
        check_vk_result(err);*/
        {
            SingleShotCommandBuffer cmdBuff(ctx);
            ImGui_ImplVulkan_CreateFontsTexture(cmdBuff);
        }

        // VkSubmitInfo end_info = {};
        // end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        // end_info.commandBufferCount = 1;
        // end_info.pCommandBuffers = &command_buffer;
        // err = vkEndCommandBuffer(command_buffer);
        // check_vk_result(err);
        // err = vkQueueSubmit(g_Queue, 1, &end_info, VK_NULL_HANDLE);
        // check_vk_result(err);

        // todo: probably not needed, the SingleShotCommandBuffer already syncs
        ctx.device()->DeviceWaitIdle(ctx.device());

        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }
}

void ImGuiLayer::deinit(Context &ctx)
{
    // free all buffers before destroying the imgui context
    data_.reset();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiLayer::newFrame(Context &ctx)
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::recordFrameCommands(Context &ctx, FrameContext &frameCtx)
{
    // Rendering
    ImGui::Render();

    // TODO: VK_SUBPASS_CONTENTS_INLINE?
    frameCtx.commandBuffer->beginRenderPass(
        Vk::RenderPassBeginInfo{data_->renderPass, data_->framebuffers[frameCtx.index]}
            .clearColor(0, data_->clearValue)
            .clearDepthStencil(1, 0.0f, 0));
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *frameCtx.commandBuffer);
    frameCtx.commandBuffer->endRenderPass();
}

} // namespace Cory
