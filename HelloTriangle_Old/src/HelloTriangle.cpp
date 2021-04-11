#include "HelloTriangle.h"

#include "Cory/Log.h"
#include "Cory/Mesh.h"
#include "Cory/Profiling.h"
#include "Cory/Shader.h"
#include "Cory/VkBuilders.h"
#include "Cory/VkUtils.h"

#include <glm.h>
#include <tiny_obj_loader.h>
#include <vk_mem_alloc.h>

#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <optick/optick.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <ranges>
#include <set>

HelloTriangleApplication::HelloTriangleApplication()
{
  Cory::Log::SetAppLevel(spdlog::level::trace);
  Cory::Log::SetCoreLevel(spdlog::level::debug);

  requestLayers({"VK_LAYER_KHRONOS_validation"});
  requestExtensions({VK_KHR_SWAPCHAIN_EXTENSION_NAME});

  setInitialWindowSize(WIDTH, HEIGHT);
}

void HelloTriangleApplication::init() { createGeometry(); }

void HelloTriangleApplication::deinit()
{
  m_mesh = {}; // deinit the mesh data
}

void HelloTriangleApplication::createSwapchainDependentResources()
{
  createRenderPass();
  createFramebuffers(m_renderPass);
  createUniformBuffers();
  createDescriptorSets();
  createGraphicsPipeline();
  createCommandBuffers();
}

void HelloTriangleApplication::destroySwapchainDependentResources()
{
  for (auto framebuffer : m_swapChainFramebuffers) {
    ctx().device->destroyFramebuffer(framebuffer);
  }

  ctx().device->destroyRenderPass(m_renderPass);

  for (auto &buffer : m_uniformBuffers) {
    buffer.destroy(ctx());
  }
}

void HelloTriangleApplication::drawSwapchainFrame(FrameUpdateInfo &fui)
{
  Cory::ScopeTimer("Draw");
  OPTICK_EVENT()

  updateUniformBuffer(fui.swapChainImageIdx);

  // execute command buffer with that image as attachment
  vk::SubmitInfo submitInfo{};

  // vkQueueSubmit allows to wait for a specific semaphore, which in our case
  // waits until the image is signaled available
  vk::Semaphore waitSemaphores[] = {fui.imageAvailableSemaphore};
  vk::PipelineStageFlags waitStages[] = {
      vk::PipelineStageFlagBits::eColorAttachmentOutput};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &*m_commandBuffers[fui.swapChainImageIdx];

  // vkQueueSubmit allows to signal other semaphore(s) when the rendering is
  // finished
  vk::Semaphore signalSemaphores[] = {fui.renderFinishedSemaphore};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  ctx().graphicsQueue.submit(submitInfo, fui.imageInFlightFence);
}

void HelloTriangleApplication::createGraphicsPipeline()
{

  // start pipeline initialization
  PipelineBuilder creator;

  {
    ScopeTimer timer("Shader Compilation");
    std::vector<Shader> shaders;
    Shader vertexShader(ctx(), {RESOURCE_DIR "/Shaders/default.vert"});
    Shader fragmentShader(ctx(), {RESOURCE_DIR "/Shaders/triangle.frag"});
    shaders.emplace_back(std::move(vertexShader));
    shaders.emplace_back(std::move(fragmentShader));
    creator.setShaders(std::move(shaders));
  }

  creator.setVertexInput(*m_mesh);
  creator.setViewport(swapChain().extent());
  creator.setDefaultRasterizer();
  creator.setMultisampling(msaaSamples());
  creator.setDefaultDepthStencil();
  creator.setAttachmentBlendStates(
      {Cory::VkDefaults::AttachmentBlendDisabled()});
  creator.setDefaultDynamicStates();

  // pipeline layout
  auto pipelineLayoutInfo =
      Cory::VkDefaults::PipelineLayout(m_descriptorSet.layout());
  m_pipelineLayout =
      ctx().device->createPipelineLayoutUnique(pipelineLayoutInfo);
  creator.setPipelineLayout(*m_pipelineLayout);

  creator.setRenderPass(m_renderPass);

  // finally, create the pipeline
  m_graphicsPipeline = creator.create(ctx());
}

void HelloTriangleApplication::createRenderPass()
{
  RenderPassBuilder builder;

  vk::AttachmentDescription colorAttachmentDesc;
  colorAttachmentDesc.format = swapChain().format();
  colorAttachmentDesc.samples = msaaSamples();
  colorAttachmentDesc.loadOp = vk::AttachmentLoadOp::eClear;
  colorAttachmentDesc.storeOp = vk::AttachmentStoreOp::eStore;
  colorAttachmentDesc.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  colorAttachmentDesc.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  colorAttachmentDesc.initialLayout = vk::ImageLayout::eUndefined;
  colorAttachmentDesc.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
  auto colorAttach = builder.addColorAttachment(colorAttachmentDesc);

  auto depthAttach = builder.addDepthAttachment(
      findDepthFormat(ctx().physicalDevice), msaaSamples());

  // builder.addDefaultSubpass();
  vk::SubpassDescription geometrySubpass{};
  geometrySubpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
  geometrySubpass.colorAttachmentCount = 1;
  geometrySubpass.pColorAttachments = &colorAttach;
  geometrySubpass.pDepthStencilAttachment = &depthAttach;
  geometrySubpass.pResolveAttachments = nullptr;
  builder.addSubpass(geometrySubpass);

  builder.addPreviousFrameSubpassDepencency();

  m_renderPass = builder.create(ctx());
}

void HelloTriangleApplication::createCommandBuffers()
{
  ScopeTimer("Command Buffers");

  // we need one command buffer per frame buffer
  m_commandBuffers.resize(m_swapChainFramebuffers.size());
  vk::CommandBufferAllocateInfo allocInfo{};
  allocInfo.commandPool = *ctx().permanentCmdPool;
  // _SECONDARY cannot be directly submitted but can be called from other cmd
  // buffer
  allocInfo.level = vk::CommandBufferLevel::ePrimary;
  allocInfo.commandBufferCount = (uint32_t)m_commandBuffers.size();
  m_commandBuffers = ctx().device->allocateCommandBuffersUnique(allocInfo);

  for (size_t i = 0; i < m_commandBuffers.size(); i++) {
    auto cmdBuf = *m_commandBuffers[i];

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = {};
    // ONE_TIME_SUBMIT for transient cmdbuffers that are rerecorded every frame
    // Optional
    beginInfo.pInheritanceInfo = nullptr;

    cmdBuf.begin(beginInfo);

    // start render pass
    vk::RenderPassBeginInfo renderPassInfo{};
    renderPassInfo.renderPass = m_renderPass;
    renderPassInfo.framebuffer = m_swapChainFramebuffers[i];
    renderPassInfo.renderArea.offset = vk::Offset2D{0, 0};
    // should match size of attachments
    renderPassInfo.renderArea.extent = swapChain().extent();

    // defines what is used for VK_ATTACHMENT_LOAD_OP_CLEAR
    std::array<vk::ClearValue, 2> clearColors;
    clearColors[0].color.setFloat32({0.2f, 0.2f, 0.2f, 1.0f});
    clearColors[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearColors.size());
    renderPassInfo.pClearValues = clearColors.data();

    cmdBuf.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
    // bind graphics pipeline
    cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_graphicsPipeline);

    // bind the vertex and index buffers
    cmdBuf.bindVertexBuffers(0, {m_mesh->vertexBuffer().buffer()}, {0});
    cmdBuf.bindIndexBuffer(m_mesh->indexBuffer().buffer(), 0,
                           m_mesh->indexType());

    // bind the descriptor sets
    cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                              *m_pipelineLayout, 0,
                              {m_descriptorSet.descriptorSet(i)}, {});

    // draw the vertices
    cmdBuf.drawIndexed(m_mesh->numVertices(), 1, 0, 0, 0);

    cmdBuf.endRenderPass();
    cmdBuf.end();
  }
}

void HelloTriangleApplication::createGeometry()
{
  CO_APP_INFO("Loading mesh...");
  Cory::ScopeTimer("Geometry");
  auto [vertices, indices] = primitives::doublequad();

  m_mesh = std::make_unique<Mesh>(ctx(), vertices, indices,
                                  vk::PrimitiveTopology::eTriangleList);

  CO_APP_INFO("Mesh loading finished. {} vertices, {}", vertices.size(),
              indices.size());
}

void HelloTriangleApplication::createUniformBuffers()
{
  m_uniformBuffers.resize(swapChain().size());

  for (size_t i{}; i < swapChain().size(); ++i) {
    m_uniformBuffers[i].create(ctx());
  }
}

void HelloTriangleApplication::updateUniformBuffer(uint32_t imageIndex)
{
  static auto startTime = std::chrono::high_resolution_clock::now();

  CameraUBOData &ubo = m_uniformBuffers[imageIndex].data();

  ubo.model = glm::identity<glm::mat4>();
  ubo.view = cameraManipulator.getMatrix();
  ubo.proj = glm::perspective(glm::radians(45.0f),
                              swapChain().extent().width /
                                  (float)swapChain().extent().height,
                              0.1f, 10.0f);

  ubo.modelInv = glm::inverse(ubo.model);
  ubo.viewInv = glm::inverse(ubo.view);
  ubo.projInv = glm::inverse(ubo.proj);

  ubo.camPos = cameraManipulator.getCameraPosition();
  ubo.camFocus = cameraManipulator.getCenterPosition();

  // NOTE: we flip this bc/ glm is written for OpenGL which has Y
  // inverted. otherwise image will be upside down :)
  ubo.proj[1][1] *= -1;

  m_uniformBuffers[imageIndex].update(ctx());
}

void HelloTriangleApplication::createDescriptorSets()
{
  m_descriptorSet.create(ctx(), static_cast<uint32_t>(swapChain().size()), 1,
                         0);

  std::vector<std::vector<const UniformBufferBase *>> uniformBuffers(
      swapChain().size());
  std::vector<std::vector<const Texture *>> samplers(swapChain().size());
  for (int i = 0; i < swapChain().size(); ++i) {
    uniformBuffers[i].push_back(&m_uniformBuffers[i]);
  }
  m_descriptorSet.setDescriptors(ctx(), uniformBuffers, samplers);
}

void HelloTriangleApplication::createFramebuffers(vk::RenderPass renderPass)
{
  m_swapChainFramebuffers.resize(swapChain().views().size());

  for (size_t i{0}; i < swapChain().views().size(); ++i) {
    std::array<vk::ImageView, 2> attachments = {colorBuffer().view(),
                                                depthBuffer().view()};

    vk::FramebufferCreateInfo framebufferInfo{};
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = swapChain().extent().width;
    framebufferInfo.height = swapChain().extent().height;
    framebufferInfo.layers = 1;

    m_swapChainFramebuffers[i] =
        ctx().device->createFramebuffer(framebufferInfo);
  }
}