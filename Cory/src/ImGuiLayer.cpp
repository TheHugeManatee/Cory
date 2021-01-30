#include "ImGuiLayer.h"

#include "Context.h"
#include "Log.h"
#include "VkBuilders.h"
#include "VkUtils.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

namespace Cory {

void check_vk_result(VkResult err)
{
  if (err == 0)
    return;
  CO_CORE_ERROR("[ImGui] Error: VkResult = {}\n", err);
  if (err < 0)
    abort();
}

ImGuiLayer::ImGuiLayer()
{
  m_clearValue.color.setFloat32({0.0f, 0.0f, 0.0f, 0.0f});
}

void ImGuiLayer::init(GLFWwindow *window, GraphicsContext &ctx,
                      vk::SampleCountFlagBits msaaSamples,
                      vk::ImageView renderedImage, SwapChain &swapChain)
{
  createImguiRenderpass(swapChain.format(), msaaSamples, ctx);
  createImguiDescriptorPool(ctx);

  { // create frame buffer for imgui
    VkImageView attachment[2];
    VkFramebufferCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.renderPass = m_renderPass;
    info.attachmentCount = 2;
    info.pAttachments = attachment;
    info.width = swapChain.extent().width;
    info.height = swapChain.extent().height;
    info.layers = 1;

    m_framebuffers.resize(swapChain.size());
    for (size_t i = 0; i < swapChain.size(); i++) {
      attachment[0] = renderedImage;
      attachment[1] = swapChain.views()[i];
      m_framebuffers[i] = ctx.device->createFramebuffer(info);
    }

    m_targetRect.offset = vk::Offset2D{0, 0};
    m_targetRect.extent = swapChain.extent();
  }

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

  ImGui_ImplGlfw_InitForVulkan(window, true);

  ImGui_ImplVulkan_InitInfo info{};
  info.Instance = *ctx.instance;
  info.PhysicalDevice = ctx.physicalDevice;
  info.Device = *ctx.device;
  info.QueueFamily = *ctx.queueFamilyIndices.graphicsFamily;
  info.Queue = ctx.graphicsQueue;
  info.PipelineCache = nullptr; // TODO?
  info.DescriptorPool = m_descriptorPool;
  info.Allocator = nullptr;
  info.MinImageCount = 2;
  info.ImageCount = static_cast<uint32_t>(swapChain.size());
  info.Subpass = 0;
  info.CheckVkResultFn = check_vk_result;
  info.MSAASamples = (VkSampleCountFlagBits)msaaSamples;

  info.MinImageCount = 2;
  ImGui_ImplVulkan_Init(&info, m_renderPass);

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
      SingleTimeCommandBuffer cmdBuff(ctx);
      ImGui_ImplVulkan_CreateFontsTexture(cmdBuff.buffer());
    }

    // VkSubmitInfo end_info = {};
    // end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    // end_info.commandBufferCount = 1;
    // end_info.pCommandBuffers = &command_buffer;
    // err = vkEndCommandBuffer(command_buffer);
    // check_vk_result(err);
    // err = vkQueueSubmit(g_Queue, 1, &end_info, VK_NULL_HANDLE);
    // check_vk_result(err);

    ctx.device->waitIdle();

    ImGui_ImplVulkan_DestroyFontUploadObjects();
  }
}

void ImGuiLayer::createImguiDescriptorPool(GraphicsContext &ctx)
{
  vk::DescriptorPoolSize pool_sizes[] = {
      {vk::DescriptorType::eSampler, 1000},
      {vk::DescriptorType::eCombinedImageSampler, 1000},
      {vk::DescriptorType::eSampledImage, 1000},
      {vk::DescriptorType::eStorageImage, 1000},
      {vk::DescriptorType::eUniformTexelBuffer, 1000},
      {vk::DescriptorType::eStorageTexelBuffer, 1000},
      {vk::DescriptorType::eUniformBuffer, 1000},
      {vk::DescriptorType::eStorageBuffer, 1000},
      {vk::DescriptorType::eUniformBufferDynamic, 1000},
      {vk::DescriptorType::eStorageBufferDynamic, 1000},
      {vk::DescriptorType::eInputAttachment, 1000}};

  vk::DescriptorPoolCreateInfo createInfo;
  createInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
  createInfo.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
  createInfo.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
  createInfo.pPoolSizes = pool_sizes;
  m_descriptorPool = ctx.device->createDescriptorPool(createInfo);
}

void ImGuiLayer::createImguiRenderpass(vk::Format format,
                                       vk::SampleCountFlagBits msaaSamples,
                                       GraphicsContext &ctx)
{
  RenderPassBuilder builder;

  // input attachment - previously rendered image
  vk::AttachmentDescription attachment = {};
  attachment.format = format;
  attachment.samples = msaaSamples;
  attachment.loadOp = vk::AttachmentLoadOp::eLoad;
  attachment.storeOp = vk::AttachmentStoreOp::eStore;
  attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  attachment.initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
  attachment.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
  auto color_attachment = builder.addColorAttachment(attachment);

  // add resolve attachment
  auto resolveAttach = builder.addResolveAttachment(format);

  auto imguiPass = builder.addDefaultSubpass();

  vk::SubpassDependency dependency = {};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = imguiPass;
  dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  dependency.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
  dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
  builder.addSubpassDependency(dependency);

  // vk::RenderPassCreateInfo info = {};
  // info.attachmentCount = 1;
  // info.pAttachments = &attachment;
  // info.subpassCount = 1;
  // info.pSubpasses = &subpass;
  // info.dependencyCount = 1;
  // info.pDependencies = &dependency;
  // m_renderPass = ctx.device->createRenderPass(info);
  m_renderPass = builder.create(ctx);
}

void ImGuiLayer::deinit(GraphicsContext &ctx)
{
  ctx.device->destroyDescriptorPool(m_descriptorPool);
  ctx.device->destroyRenderPass(m_renderPass);
  for (auto &fb : m_framebuffers)
    ctx.device->destroyFramebuffer(fb);

  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

void ImGuiLayer::newFrame(GraphicsContext &ctx)
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void ImGuiLayer::drawFrame(GraphicsContext &ctx, uint32_t currentFrameIdx)
{
  auto cmdBuf = SingleTimeCommandBuffer(ctx);
  // Rendering
  ImGui::Render();

  vk::ClearValue clearValues[] = {m_clearValue,
                                  vk::ClearDepthStencilValue(0, 0)};
  vk::RenderPassBeginInfo info = {};
  info.renderPass = m_renderPass;
  info.framebuffer = m_framebuffers[currentFrameIdx];
  info.renderArea = m_targetRect;
  info.clearValueCount = 2;
  info.pClearValues = clearValues;

  cmdBuf->beginRenderPass(info, vk::SubpassContents::eInline);
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuf.buffer());
  cmdBuf->endRenderPass();
}

} // namespace Cory
