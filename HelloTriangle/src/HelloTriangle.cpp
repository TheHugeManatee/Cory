#include "HelloTriangle.h"

#include "Cory/Mesh.h"
#include "Cory/Shader.h"
#include "Cory/VkBuilders.h"
#include "Cory/VkUtils.h"

#include <glm.h>
#include <spdlog/spdlog.h>
#include <tiny_obj_loader.h>
#include <vk_mem_alloc.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <ranges>
#include <set>

void HelloTriangleApplication::run()
{
    spdlog::set_level(spdlog::level::trace);

    requestLayers({"VK_LAYER_KHRONOS_validation"});
    requestExtensions({VK_KHR_SWAPCHAIN_EXTENSION_NAME});

    initWindow({WIDTH, HEIGHT});

    initVulkan();

    mainLoop();

    cleanup();

    if (enableValidationLayers) {
        m_ctx.instance->destroyDebugUtilsMessengerEXT(m_debugMessenger, nullptr, m_ctx.dl);
    }
}

void HelloTriangleApplication::initVulkan()
{
    setupInstance();
    setupDebugMessenger();

    createSurface();

    pickPhysicalDevice();
    createLogicalDevice();

    createMemoryAllocator();

    createCommandPools();

    m_swapChain = std::make_unique<SwapChain>(m_ctx, m_window, m_surface);
    createSyncObjects(MAX_FRAMES_IN_FLIGHT);

    createColorResources();
    createDepthResources();

    // app resources
    m_texture = createTextureImage(RESOURCE_DIR "/viking_room.png", vk::Filter::eLinear,
                                   vk::SamplerAddressMode::eRepeat);
    m_texture2 = createTextureImage(RESOURCE_DIR "/sunglasses.png", vk::Filter::eLinear,
                                    vk::SamplerAddressMode::eClampToBorder);
    createGeometry();

    // per swapchain dependent resources
    createRenderPass();

    createFramebuffers(m_renderPass);


    createUniformBuffers();
    createDescriptorSets();
    createGraphicsPipeline();

    createCommandBuffers();
}

void HelloTriangleApplication::mainLoop()
{
    spdlog::info("Entering main loop.");
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        drawFrame();
    }

    m_ctx.device->waitIdle();

    spdlog::info("Leaving main loop.");
}

void HelloTriangleApplication::cleanup()
{
    spdlog::info("Cleaning up Vulkan and GLFW..");

    cleanupSwapChain();
    m_swapChain = {};

    m_ctx.instance->destroySurfaceKHR(m_surface);

    m_mesh = {}; // deinit the mesh data

    m_texture.destroy(m_ctx);
    m_texture2.destroy(m_ctx);

    vmaDestroyAllocator(m_ctx.allocator);

    glfwDestroyWindow(m_window);
    glfwTerminate();

    spdlog::info("Application shut down.");
}

void HelloTriangleApplication::createGraphicsPipeline()
{
    //****************** Shaders ******************
    const auto vertShaderCode = readFile(RESOURCE_DIR "/default-vert.spv");
    const auto fragShaderCode = readFile(RESOURCE_DIR "/manatee.spv");

    // start pipeline initialization
    PipelineBuilder creator;
    std::vector<Shader> shaders;
    shaders.emplace_back(m_ctx, vertShaderCode, ShaderType::eVertex);
    shaders.emplace_back(m_ctx, fragShaderCode, ShaderType::eFragment);
    creator.setShaders(std::move(shaders));

    creator.setVertexInput(*m_mesh);
    creator.setViewport(m_swapChain->extent());
    creator.setDefaultRasterizer();
    creator.setMultisampling(m_msaaSamples);
    creator.setDefaultDepthStencil();
    creator.setAttachmentBlendStates({Cory::VkDefaults::AttachmentBlendDisabled()});
    creator.setDefaultDynamicStates();

    // pipeline layout
    auto pipelineLayoutInfo = Cory::VkDefaults::PipelineLayout(m_descriptorSet.layout());
    m_pipelineLayout = m_ctx.device->createPipelineLayoutUnique(pipelineLayoutInfo);
    creator.setPipelineLayout(*m_pipelineLayout);

    creator.setRenderPass(m_renderPass);

    // finally, create the pipeline
    m_graphicsPipeline = creator.create(m_ctx);
}

void HelloTriangleApplication::createRenderPass()
{
    RenderPassBuilder builder;

    builder.addColorAttachment(m_swapChain->format(), m_msaaSamples);
    builder.addDepthAttachment(findDepthFormat(m_ctx.physicalDevice), m_msaaSamples);
    builder.addResolveAttachment(m_swapChain->format());

    m_renderPass = builder.create(m_ctx);
}

void HelloTriangleApplication::createCommandBuffers()
{
    // we need one command buffer per frame buffer
    m_commandBuffers.resize(m_swapChainFramebuffers.size());
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.commandPool = *m_ctx.permanentCmdPool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary; // _SECONDARY cannot be directly submitted
                                                        // but can be called from other cmd buffer
    allocInfo.commandBufferCount = (uint32_t)m_commandBuffers.size();
    m_commandBuffers = m_ctx.device->allocateCommandBuffersUnique(allocInfo);

    for (size_t i = 0; i < m_commandBuffers.size(); i++) {
        auto cmdBuf = *m_commandBuffers[i];

        vk::CommandBufferBeginInfo beginInfo{};
        beginInfo.flags = {};
        // ONE_TIME_SUBMIT for transient cmdbuffers that are rerecorded every frame
        beginInfo.pInheritanceInfo = nullptr; // Optional

        cmdBuf.begin(beginInfo);

        // start render pass
        vk::RenderPassBeginInfo renderPassInfo{};
        renderPassInfo.renderPass = m_renderPass;
        renderPassInfo.framebuffer = m_swapChainFramebuffers[i];
        renderPassInfo.renderArea.offset = vk::Offset2D{0, 0};
        renderPassInfo.renderArea.extent =
            m_swapChain->extent(); // should match size of attachments

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
        cmdBuf.bindIndexBuffer(m_mesh->indexBuffer().buffer(), 0, m_mesh->indexType());

        // bind the descriptor sets
        cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *m_pipelineLayout, 0,
                                  {m_descriptorSet.descriptorSet(i)}, {});

        // draw the vertices
        cmdBuf.drawIndexed(m_mesh->numVertices(), 1, 0, 0, 0);

        cmdBuf.endRenderPass();
        cmdBuf.end();
    }
}

void HelloTriangleApplication::createGeometry()
{
    //     auto [vertices, indices] = primitives::doublequad();
    //     m_numVertices = static_cast<uint32_t>(indices.size());

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                          RESOURCE_DIR "/viking_room.obj")) {

        throw std::runtime_error(fmt::format("Could not load 3D model: {} {}", warn, err));
    }

    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    std::unordered_map<Vertex, uint16_t, Vertex::hasher> uniqueVertices;
    for (const auto &shape : shapes) {

        for (const auto &index : shape.mesh.indices) {
            Vertex v{};
            v.pos = {attrib.vertices[3 * index.vertex_index + 0],
                     attrib.vertices[3 * index.vertex_index + 1],
                     attrib.vertices[3 * index.vertex_index + 2]};
            v.texCoord = {attrib.texcoords[2 * index.texcoord_index + 0],
                          1.0f - attrib.texcoords[2 * index.texcoord_index + 1]};
            v.color = {1.0f, 1.0f, 1.0f};

            if (uniqueVertices.count(v) == 0) {
                uniqueVertices[v] = static_cast<uint16_t>(vertices.size());
                vertices.push_back(v);
            }

            indices.push_back(uniqueVertices[v]);
        }
    }

    m_mesh = std::make_unique<Mesh>(m_ctx, vertices, indices, vk::PrimitiveTopology::eTriangleList);
}

void HelloTriangleApplication::createUniformBuffers()
{
    m_uniformBuffers.resize(m_swapChain->size());

    for (size_t i{}; i < m_swapChain->size(); ++i) {
        m_uniformBuffers[i].create(m_ctx);
        //, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, DeviceMemoryUsage::eCpuOnly);
    }
}

void HelloTriangleApplication::updateUniformBuffer(uint32_t imageIndex)
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time =
        std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    CameraUBOData &ubo = m_uniformBuffers[imageIndex].data();
    ubo.model =
        glm::rotate(glm::mat4(1.f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                           glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f),
                                m_swapChain->extent().width / (float)m_swapChain->extent().height,
                                0.1f, 10.0f);
    ubo.proj[1][1] *= -1; // NOTE: we flip this bc/ glm is written for OpenGL which has Y inverted.
                          // otherwise image will be upside down :)

    m_uniformBuffers[imageIndex].update(m_ctx);
}

void HelloTriangleApplication::createDescriptorSets()
{
    m_descriptorSet.create(m_ctx, static_cast<uint32_t>(m_swapChain->size()), 1, 2);

    std::vector<std::vector<const UniformBufferBase *>> uniformBuffers(m_swapChain->size());
    std::vector<std::vector<const Texture *>> samplers(m_swapChain->size());
    for (int i = 0; i < m_swapChain->size(); ++i) {
        uniformBuffers[i].push_back(&m_uniformBuffers[i]);
        samplers[i] = {&m_texture, &m_texture2};
    }
    m_descriptorSet.setDescriptors(m_ctx, uniformBuffers, samplers);
}

void HelloTriangleApplication::createFramebuffers(vk::RenderPass renderPass)
{
    m_swapChainFramebuffers.resize(m_swapChain->views().size());

    for (size_t i{0}; i < m_swapChain->views().size(); ++i) {
        std::array<vk::ImageView, 3> attachments = {m_renderTarget.view(), m_depthBuffer.view(),
                                                    m_swapChain->views()[i]};

        vk::FramebufferCreateInfo framebufferInfo{};
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = m_swapChain->extent().width;
        framebufferInfo.height = m_swapChain->extent().height;
        framebufferInfo.layers = 1;

        m_swapChainFramebuffers[i] = m_ctx.device->createFramebuffer(framebufferInfo);
    }
}

void HelloTriangleApplication::drawSwapchainFrame(FrameUpdateInfo &fui)
{
    updateUniformBuffer(fui.swapChainImageIdx);

    // execute command buffer with that image as attachment
    vk::SubmitInfo submitInfo{};

    // vkQueueSubmit allows to wait for a specific semaphore, which in our case waits until the
    // image is signaled available
    vk::Semaphore waitSemaphores[] = {fui.imageAvailableSemaphore};
    vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &*m_commandBuffers[fui.swapChainImageIdx];

    // vkQueueSubmit allows to signal other semaphore(s) when the rendering is finished
    vk::Semaphore signalSemaphores[] = {fui.renderFinishedSemaphore};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    m_ctx.graphicsQueue.submit(submitInfo, fui.imageInFlightFence);
}

void HelloTriangleApplication::createSwapchainData()
{
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers(m_renderPass);
    createUniformBuffers();
    createDescriptorSets();
    createCommandBuffers();
}

void HelloTriangleApplication::destroySwapchainData() {
    for (auto framebuffer : m_swapChainFramebuffers) {
        m_ctx.device->destroyFramebuffer(framebuffer);
    }

    m_ctx.device->destroyRenderPass(m_renderPass);

    for (auto &buffer : m_uniformBuffers) {
        buffer.destroy(m_ctx);
    }
}

Texture HelloTriangleApplication::createTextureImage(std::string textureFilename, vk::Filter filter,
                                                     vk::SamplerAddressMode addressMode)
{
    stbi_image image(textureFilename);
    Texture texture;

    if (!image.data) {
        throw std::runtime_error("Could not load texture image from file!");
    }

    Buffer stagingBuffer;
    stagingBuffer.create(m_ctx, image.size(), vk::BufferUsageFlagBits::eTransferSrc,
                         DeviceMemoryUsage::eCpuOnly);

    stagingBuffer.upload(m_ctx, image.data, image.size());

    uint32_t mipLevels =
        static_cast<uint32_t>(std::floor(std::log2(std::max(image.width, image.height)))) + 1;
    texture.create(m_ctx, {image.width, image.height, 1}, mipLevels, vk::ImageType::e2D,
                   vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal, filter, addressMode,
                   vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled |
                       vk::ImageUsageFlagBits::eTransferSrc,
                   DeviceMemoryUsage::eGpuOnly);

    texture.transitionLayout(m_ctx, vk::ImageLayout::eTransferDstOptimal);
    stagingBuffer.copyTo(m_ctx, texture);
    stagingBuffer.destroy(m_ctx);

    texture.generateMipmaps(m_ctx, vk::ImageLayout::eShaderReadOnlyOptimal,
                            vk::AccessFlagBits::eShaderRead);
    // texture.transitionLayout(m_ctx, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    return texture;
}
