#include "HelloTriangle.h"

#include "Cory/Mesh.h"
#include "Cory/VkUtils.h"

#include <glm.h>
#include <spdlog/spdlog.h>
#include <tiny_obj_loader.h>
#include <vk_mem_alloc.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <set>

static std::vector<char> readFile(const std::string &filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error(fmt::format("failed to open file {}", filename));
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}


void HelloTriangleApplication::run()
{
    spdlog::set_level(spdlog::level::trace);

    requestLayers({"VK_LAYER_KHRONOS_validation"});
    requestExtensions({VK_KHR_SWAPCHAIN_EXTENSION_NAME});

    initWindow({WIDTH, HEIGHT});

    initVulkan();

    mainLoop();

    cleanup();
}

void HelloTriangleApplication::initVulkan()
{
    setupInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();

    createMemoryAllocator();

    createTransientCommandPool();

    createSwapChain();
    createImageViews();
    createRenderPass();

    createColorResources();
    createDepthResources();
    createFramebuffers();
    createSyncObjects();

    m_texture = createTextureImage(RESOURCE_DIR "/viking_room.png", vk::Filter::eLinear,
                                   vk::SamplerAddressMode::eRepeat);
    m_texture2 = createTextureImage(RESOURCE_DIR "/sunglasses.png", vk::Filter::eLinear,
                                    vk::SamplerAddressMode::eClampToBorder);
    createGeometry();

    createDescriptorSetLayout();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createGraphicsPipeline();

    createAppCommandPool();
    createCommandBuffers();
}

void HelloTriangleApplication::createTransientCommandPool()
{
    // create a second command pool for transient operations

    vk::CommandPoolCreateInfo poolInfo{};
    auto queueFamilyIndices = findQueueFamilies(m_ctx.physicalDevice, m_surface);
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eTransient;

    m_ctx.transientCmdPool = m_ctx.device->createCommandPoolUnique(poolInfo);
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

    m_vertexBuffer.destroy(m_ctx);
    m_indexBuffer.destroy(m_ctx);

    m_texture.destroy(m_ctx);
    m_texture2.destroy(m_ctx);

    m_ctx.instance->destroySurfaceKHR(m_surface);

    vmaDestroyAllocator(m_ctx.allocator);

    if (enableValidationLayers) {
        m_ctx.instance->destroyDebugUtilsMessengerEXT(m_debugMessenger, nullptr, m_ctx.dl);
    }

    glfwDestroyWindow(m_window);
    glfwTerminate();

    spdlog::info("Application shut down.");
}

void HelloTriangleApplication::createSurface()
{
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(*m_ctx.instance, m_window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Could not create window surface!");
    }
    m_surface = vk::SurfaceKHR(surface);
}

void HelloTriangleApplication::setupDebugMessenger()
{
    if (!enableValidationLayers)
        return;

    vk::DebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    m_debugMessenger = m_ctx.instance->createDebugUtilsMessengerEXT(createInfo, nullptr, m_ctx.dl);
}

void HelloTriangleApplication::pickPhysicalDevice()
{
    auto devices = m_ctx.instance->enumeratePhysicalDevices();
    if (devices.empty()) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }
    spdlog::info("Found {} vulkan devices", devices.size());

    for (const auto &device : devices) {
        if (isDeviceSuitable(device)) {
            m_ctx.physicalDevice = device;
            m_msaaSamples = getMaxUsableSampleCount();
            break;
        }
    }

    if (m_ctx.physicalDevice == vk::PhysicalDevice()) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

SwapChainSupportDetails HelloTriangleApplication::querySwapChainSupport(vk::PhysicalDevice device,
                                                                        vk::SurfaceKHR surface)
{
    SwapChainSupportDetails details;
    details.capabilities = device.getSurfaceCapabilitiesKHR(surface);
    details.formats = device.getSurfaceFormatsKHR(surface);
    details.presentModes = device.getSurfacePresentModesKHR(surface);

    return details;
}

vk::SurfaceFormatKHR HelloTriangleApplication::chooseSwapSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR> &availableFormats)
{
    // BRGA8 and SRGB are the preferred formats
    for (const auto &availableFormat : availableFormats) {
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
            availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

vk::PresentModeKHR HelloTriangleApplication::chooseSwapPresentMode(
    const std::vector<vk::PresentModeKHR> &availablePresentModes)
{
    for (const auto &availableMode : availablePresentModes) {
        if (availableMode == vk::PresentModeKHR::eMailbox) {
            return availableMode;
        }
    }
    return availablePresentModes[0];
}

vk::Extent2D
HelloTriangleApplication::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities)
{
    // for high DPI, the extent between pixel size and screen coordinates might not be the same.
    // in this case, we need to compute a proper viewport extent from the GLFW screen coordinates
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }

    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);

    vk::Extent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                                    capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                                     capabilities.maxImageExtent.height);
    return actualExtent;
}

void HelloTriangleApplication::createSwapChain()
{
    const SwapChainSupportDetails swapChainSupport =
        querySwapChainSupport(m_ctx.physicalDevice, m_surface);

    const vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    const vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    const vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    // we use one more image as a buffer to avoid stalls when waiting for the next image to become
    // available
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo;

    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1; // this might be 2 if we are developing stereoscopic stuff
    createInfo.imageUsage =
        vk::ImageUsageFlagBits::eColorAttachment; // for off-screen rendering, it is possible to use
                                                  // VK_IMAGE_USAGE_TRANSFER_DST_BIT instead

    QueueFamilyIndices indices = findQueueFamilies(m_ctx.physicalDevice, m_surface);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    // if the swap and present queues are different, the swap chain images have to be shareable
    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode =
            vk::SharingMode::eExclusive; // exclusive has better performance
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha =
        vk::CompositeAlphaFlagBitsKHR::eOpaque; // whether the alpha channel should be used to
                                                // composite on top of other windows

    createInfo.presentMode = presentMode;
    createInfo.clipped =
        VK_TRUE; // false would force pixels to be rendered even if they are occluded. might be
                 // important if the buffer is read back somehow (screen shots etc?)

    createInfo.oldSwapchain = nullptr; // old swap chain, required when resizing etc.

    m_swapChain = m_ctx.device->createSwapchainKHR(createInfo);

    m_swapChainImages = m_ctx.device->getSwapchainImagesKHR(m_swapChain);
    m_swapChainExtent = extent;
    m_swapChainImageFormat = surfaceFormat.format;
}

void HelloTriangleApplication::createImageViews()
{
    m_swapChainImageViews.resize(m_swapChainImages.size());

    for (size_t i = 0; i < m_swapChainImages.size(); ++i) {
        vk::ImageViewCreateInfo createInfo{};
        createInfo.image = m_swapChainImages[i];
        createInfo.viewType = vk::ImageViewType::e2D;
        createInfo.format = m_swapChainImageFormat;
        createInfo.components.r = vk::ComponentSwizzle::eIdentity;
        createInfo.components.g = vk::ComponentSwizzle::eIdentity;
        createInfo.components.b = vk::ComponentSwizzle::eIdentity;
        createInfo.components.a = vk::ComponentSwizzle::eIdentity;

        // for stereographic, we could create separate image views for the array layers here
        createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        m_swapChainImageViews[i] = m_ctx.device->createImageView(createInfo);
    }
}

vk::UniqueShaderModule HelloTriangleApplication::createShaderModule(const std::vector<char> &code)
{
    vk::ShaderModuleCreateInfo createInfo{};
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

    auto shaderModule = m_ctx.device->createShaderModuleUnique(createInfo);

    return shaderModule;
}

void HelloTriangleApplication::createGraphicsPipeline()
{
    //****************** Shaders ******************
    auto vertShaderCode = readFile(RESOURCE_DIR "/default-vert.spv");
    auto fragShaderCode = readFile(RESOURCE_DIR "/manatee.spv");

    auto vertShaderModule = createShaderModule(vertShaderCode);
    auto fragShaderModule = createShaderModule(fragShaderCode);

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertShaderStageInfo.module = *vertShaderModule;
    // entry point -- means we can add multiple entry points in one module - yays!
    vertShaderStageInfo.pName = "main";

    // Note: pSpecializationInfo can be used to set compile time constants - kinda like macros in an
    // online compilation?

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragShaderStageInfo.module = *fragShaderModule;
    fragShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    //****************** Vertex Input ******************
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
    inputAssembly.primitiveRestartEnable =
        false; // allows to break primitive lists with 0xFFFF index

    //****************** Viewport & Scissor ******************
    vk::Viewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)m_swapChainExtent.width;
    viewport.height = (float)m_swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vk::Rect2D scissor{};
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent = m_swapChainExtent;

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    //****************** Rasterizer ******************
    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.depthClampEnable = false; // depth clamp: depth is clamped for fragments instead
                                         // of discarding them. might be useful for shadow maps?
    rasterizer.rasterizerDiscardEnable = false; // completely disable rasterizer/framebuffer output
    rasterizer.polygonMode = vk::PolygonMode::eFill; // _LINE and _POINT are alternatives, but
                                                     // require enabling GPU feature
    rasterizer.lineWidth = 1.0f;                     // >1.0 requires 'wideLines' GPU feature
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = false;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    //****************** Multisampling ******************
    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sampleShadingEnable = false;
    multisampling.rasterizationSamples = m_msaaSamples;
    multisampling.minSampleShading = 0.2f; // controls how smooth the msaa
    multisampling.pSampleMask = nullptr;   // ?
    multisampling.alphaToCoverageEnable = false;
    multisampling.alphaToOneEnable = false;

    //****************** Depth and Stencil ******************
    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = vk::CompareOp::eLess;
    depthStencil.depthBoundsTestEnable = false;
    depthStencil.minDepthBounds = 0.0f;
    depthStencil.maxDepthBounds = 1.0f;
    depthStencil.stencilTestEnable = false;
    depthStencil.front = vk::StencilOpState{};
    depthStencil.back = vk::StencilOpState{};

    //****************** Color Blending ******************
    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    colorBlendAttachment.blendEnable = false;
    colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eOne;
    colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eZero;
    colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
    colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

    // note: you can only do EITHER color blending per attachment, or logic blending. enabling logic
    // blending will override/disable the blend ops above
    vk::PipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.logicOpEnable = false;
    colorBlending.logicOp = vk::LogicOp::eCopy;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    //****************** Dynamic State ******************
    // Specifies which pipeline states can be changed dynamically without recreating the pipeline
    vk::DynamicState dynamicStates[] = {vk::DynamicState::eViewport, vk::DynamicState::eLineWidth};
    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    //****************** Pipeline Layout ******************
    // stores/manages shader uniform values
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &*m_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    m_pipelineLayout = m_ctx.device->createPipelineLayoutUnique(pipelineLayoutInfo);

    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = nullptr; // skipped for now --why tho?
    pipelineInfo.layout = *m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = nullptr; // note: vulkan can have "base" and "derived"
                                               // pipelines when they are similar
    pipelineInfo.basePipelineIndex = -1;

    auto [result, pipeline] =
        m_ctx.device->createGraphicsPipelineUnique(nullptr, pipelineInfo).asTuple();
    switch (result) {
    case vk::Result::eSuccess:
        m_graphicsPipeline = std::move(pipeline);
        break;
    case vk::Result::ePipelineCompileRequiredEXT:
        throw std::runtime_error(fmt::format("Could not create pipeline: {}", result));
        break;
    default:
        assert(false); // should never happen
    }
}

void HelloTriangleApplication::createRenderPass()
{
    vk::AttachmentDescription colorAttachment{};
    colorAttachment.format = m_swapChainImageFormat;
    colorAttachment.samples = m_msaaSamples;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear; // care about color
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare; // don't care about stencil
    colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
    colorAttachment.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::AttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat(m_ctx.physicalDevice);
    depthAttachment.samples = m_msaaSamples;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
    depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    depthAttachment.initialLayout = vk::ImageLayout::eUndefined;
    depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::AttachmentDescription colorAttachmentResolve{};
    colorAttachmentResolve.format = m_swapChainImageFormat;
    colorAttachmentResolve.samples = vk::SampleCountFlagBits::e1;
    colorAttachmentResolve.loadOp = vk::AttachmentLoadOp::eDontCare;
    colorAttachmentResolve.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachmentResolve.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    colorAttachmentResolve.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    colorAttachmentResolve.initialLayout = vk::ImageLayout::eUndefined;
    colorAttachmentResolve.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    //****************** Subpasses ******************
    // describe which layout each attachment should be transitioned to
    vk::AttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::AttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::AttachmentReference colorAttachmentResolveRef{};
    colorAttachmentResolveRef.attachment = 2;
    colorAttachmentResolveRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::SubpassDescription subpass{};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    subpass.pResolveAttachments = &colorAttachmentResolveRef;
    // NOTE: the order of attachments directly corresponds to the 'layout(location=0) out vec4
    // color' index in the fragment shader pInputAttachments: attachments that are read from a
    // shader pResolveAttachments: attachments used for multisampling color attachments
    // pDepthStencilAttachment: attachment for depth and stencil data
    // pPreserveAttachments: attachments that are not currently used by the subpass but for which
    // the data needs to be preserved.

    //****************** Render Pass ******************
    std::array<vk::AttachmentDescription, 3> attachments = {colorAttachment, depthAttachment,
                                                            colorAttachmentResolve};
    vk::RenderPassCreateInfo renderPassInfo{};
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    //****************** Subpass dependencies ******************
    // this sets up the render pass to wait for the STAGE_COLOR_ATTACHMENT_OUTPUT stage to ensure
    // the images are available and the swap chain is not still reading the image
    vk::SubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                              vk::PipelineStageFlagBits::eEarlyFragmentTests;
    dependency.srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead; // not sure here..
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                              vk::PipelineStageFlagBits::eEarlyFragmentTests;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                               vk::AccessFlagBits::eDepthStencilAttachmentWrite;

    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    m_renderPass = m_ctx.device->createRenderPass(renderPassInfo);
}

void HelloTriangleApplication::createFramebuffers()
{
    m_swapChainFramebuffers.resize(m_swapChainImageViews.size());

    for (size_t i{0}; i < m_swapChainImageViews.size(); ++i) {
        std::array<vk::ImageView, 3> attachments = {m_renderTarget.view(), m_depthBuffer.view(),
                                                    m_swapChainImageViews[i]};

        vk::FramebufferCreateInfo framebufferInfo{};
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = m_swapChainExtent.width;
        framebufferInfo.height = m_swapChainExtent.height;
        framebufferInfo.layers = 1;

        m_swapChainFramebuffers[i] = m_ctx.device->createFramebuffer(framebufferInfo);
    }
}

void HelloTriangleApplication::createAppCommandPool()
{
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_ctx.physicalDevice, m_surface);

    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    poolInfo.flags = vk::CommandPoolCreateFlagBits(
        0); // for re-recording of command buffers, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
            // or VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT might be necessary

    m_commandPool = m_ctx.device->createCommandPoolUnique(poolInfo);
}

void HelloTriangleApplication::createCommandBuffers()
{
    // we need one command buffer per frame buffer
    m_commandBuffers.resize(m_swapChainFramebuffers.size());
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.commandPool = *m_commandPool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary; // _SECONDARY cannot be directly submitted
                                                        // but can be called from other cmd buffer
    allocInfo.commandBufferCount = (uint32_t)m_commandBuffers.size();
    m_commandBuffers = m_ctx.device->allocateCommandBuffersUnique(allocInfo);

    // begin all command buffers
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
        renderPassInfo.renderArea.extent = m_swapChainExtent; // should match size of attachments

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
        cmdBuf.bindVertexBuffers(0, {m_vertexBuffer.buffer()}, {0});
        cmdBuf.bindIndexBuffer(m_indexBuffer.buffer(), 0, vk::IndexType::eUint16);

        // bind the descriptor sets
        cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *m_pipelineLayout, 0,
                                  {m_descriptorSets[i]}, {});

        // draw the vertices
        cmdBuf.drawIndexed(m_numVertices, 1, 0, 0, 0);

        cmdBuf.endRenderPass();
        cmdBuf.end();
    }
}

void HelloTriangleApplication::createSyncObjects()
{
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    m_imagesInFlight.resize(m_swapChainImages.size());

    vk::SemaphoreCreateInfo semaphoreInfo{};
    vk::FenceCreateInfo fenceInfo{};
    fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

    for (size_t i{}; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_imageAvailableSemaphores[i] = m_ctx.device->createSemaphoreUnique(semaphoreInfo);
        m_renderFinishedSemaphores[i] = m_ctx.device->createSemaphoreUnique(semaphoreInfo);
        m_inFlightFences[i] = m_ctx.device->createFenceUnique(fenceInfo);
    }
}

void HelloTriangleApplication::recreateSwapChain()
{
    // window might be minimized
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);
    if (width == height == 0)
        spdlog::info("Window minimized");
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(m_window, &width, &height);
        glfwWaitEvents();
    }
    spdlog::info("Framebuffer resized");

    vkDeviceWaitIdle(*m_ctx.device);

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createColorResources();
    createDepthResources();
    createFramebuffers();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
}

void HelloTriangleApplication::cleanupSwapChain()
{
    m_depthBuffer.destroy(m_ctx);
    m_renderTarget.destroy(m_ctx);

    for (auto framebuffer : m_swapChainFramebuffers) {
        vkDestroyFramebuffer(*m_ctx.device, framebuffer, nullptr);
    }

    vkDestroyRenderPass(*m_ctx.device, m_renderPass, nullptr);

    for (auto imageView : m_swapChainImageViews) {
        vkDestroyImageView(*m_ctx.device, imageView, nullptr);
    }

    vkDestroySwapchainKHR(*m_ctx.device, m_swapChain, nullptr);

    for (auto &buffer : m_uniformBuffers) {
        buffer.destroy(m_ctx);
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
    m_numVertices = static_cast<uint32_t>(indices.size());

    createVertexBuffers(vertices);
    createIndexBuffer(indices);
}

void HelloTriangleApplication::createVertexBuffers(const std::vector<Vertex> &vertices)
{
    vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    Buffer stagingBuffer;
    stagingBuffer.create(m_ctx, bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                         DeviceMemoryUsage::eCpuOnly);

    stagingBuffer.upload(m_ctx, vertices.data(), bufferSize);

    m_vertexBuffer.create(m_ctx, bufferSize,
                          vk::BufferUsageFlagBits::eTransferDst |
                              vk::BufferUsageFlagBits::eVertexBuffer,
                          DeviceMemoryUsage::eGpuOnly);

    stagingBuffer.copyTo(m_ctx, m_vertexBuffer, bufferSize);

    stagingBuffer.destroy(m_ctx);
}

void HelloTriangleApplication::createIndexBuffer(const std::vector<uint16_t> &indices)
{
    vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    Buffer stagingBuffer;
    stagingBuffer.create(m_ctx, bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                         DeviceMemoryUsage::eCpuOnly);

    stagingBuffer.upload(m_ctx, indices.data(), bufferSize);

    m_indexBuffer.create(m_ctx, bufferSize,
                         vk::BufferUsageFlagBits::eTransferDst |
                             vk::BufferUsageFlagBits::eIndexBuffer,
                         DeviceMemoryUsage::eGpuOnly);

    stagingBuffer.copyTo(m_ctx, m_indexBuffer, bufferSize);

    stagingBuffer.destroy(m_ctx);
}

void HelloTriangleApplication::createUniformBuffers()
{
    vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

    m_uniformBuffers.resize(m_swapChainImages.size());

    for (size_t i{}; i < m_swapChainImages.size(); ++i) {
        m_uniformBuffers[i].create(m_ctx, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
                                   DeviceMemoryUsage::eCpuOnly);
    }
}

void HelloTriangleApplication::drawFrame()
{
    // fences to sync per-frame draw resources
    auto perFrameFenceResult =
        m_ctx.device->waitForFences({*m_inFlightFences[m_currentFrame]}, true, UINT64_MAX);
    if (perFrameFenceResult != vk::Result::eSuccess) {
        throw std::runtime_error(fmt::format("failed to wait for inFlightFences[{}], error: {}",
                                             m_currentFrame, perFrameFenceResult));
    }

    // acquire image
    auto [result, imageIndex] = m_ctx.device->acquireNextImageKHR(
        m_swapChain, UINT64_MAX, *m_imageAvailableSemaphores[m_currentFrame], nullptr);

    if (result == vk::Result::eErrorOutOfDateKHR) {
        recreateSwapChain();
        return;
    }

    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
        throw std::runtime_error("failed to acquire swap chain image");
    }

    // Check if a previous frame is using this image (i.e. there is its fence to wait on)
    if (m_imagesInFlight[imageIndex] != vk::Fence{}) {
        result = m_ctx.device->waitForFences({m_imagesInFlight[imageIndex]}, true, UINT64_MAX);
        if (result != vk::Result::eSuccess) {
            throw std::runtime_error(fmt::format("failed to wait for inFlightFences[{}], error: {}",
                                                 imageIndex, result));
        }
    }
    // Mark the image as now being in use by this frame
    m_imagesInFlight[imageIndex] = *m_inFlightFences[m_currentFrame];

    updateUniformBuffer(imageIndex);

    // execute command buffer with that image as attachment
    vk::SubmitInfo submitInfo{};

    // vkQueueSubmit allows to wait for a specific semaphore, which in our case waits until the
    // image is signaled available
    vk::Semaphore waitSemaphores[] = {*m_imageAvailableSemaphores[m_currentFrame]};
    vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &*m_commandBuffers[imageIndex];

    // vkQueueSubmit allows to signal other semaphore(s) when the rendering is finished
    vk::Semaphore signalSemaphores[] = {*m_renderFinishedSemaphores[m_currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    // make sure to reset the frame-respective fence
    m_ctx.device->resetFences(*m_inFlightFences[m_currentFrame]);

    m_ctx.graphicsQueue.submit(submitInfo, *m_inFlightFences[m_currentFrame]);

    // return image to the swap chain
    vk::PresentInfoKHR presentInfo{};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores; // wait for queue to finish

    vk::SwapchainKHR swapChains[] = {m_swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults =
        nullptr; // can be used to check every individual swap chain is successful

    try {
        result = m_ctx.presentQueue.presentKHR(presentInfo);
    }
    catch (vk::OutOfDateKHRError) {
        m_framebufferResized = false;
        recreateSwapChain();
    }

    if (result == vk::Result::eSuboptimalKHR || m_framebufferResized) {
        m_framebufferResized = false;
        recreateSwapChain();
    }
    else if (result != vk::Result::eSuccess) {
        throw std::runtime_error("failed to present swap chain image");
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void HelloTriangleApplication::updateUniformBuffer(uint32_t imageIndex)
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time =
        std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo;
    ubo.model =
        glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                           glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj =
        glm::perspective(glm::radians(45.0f),
                         m_swapChainExtent.width / (float)m_swapChainExtent.height, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1; // NOTE: we flip this bc/ glm is written for OpenGL which has Y inverted.
                          // otherwise image will be upside down :)

    m_uniformBuffers[imageIndex].upload(m_ctx, &ubo, sizeof(ubo));
}

void HelloTriangleApplication::createDescriptorPool()
{
    std::array<vk::DescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = vk::DescriptorType::eUniformBuffer;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(m_swapChainImages.size());
    poolSizes[1].type = vk::DescriptorType::eCombinedImageSampler;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(m_swapChainImages.size());

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(m_swapChainImages.size());
    // enables creation and freeing of individual descriptor sets -- we don't care for that right
    // now
    // poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    m_descriptorPool = m_ctx.device->createDescriptorPoolUnique(poolInfo);
}

void HelloTriangleApplication::createDescriptorSets()
{
    std::vector<vk::DescriptorSetLayout> layouts(m_swapChainImages.size(), *m_descriptorSetLayout);

    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool = *m_descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(m_swapChainImages.size());
    allocInfo.pSetLayouts = layouts.data();

    // NOTE: descriptor sets are freed implicitly when the pool is freed.
    m_descriptorSets = m_ctx.device->allocateDescriptorSets(allocInfo);

    // populate every descriptor
    for (size_t i{}; i < m_swapChainImages.size(); ++i) {
        vk::DescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_uniformBuffers[i].buffer();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject); // access range, could be VK_WHOLE_SIZE

        std::array<vk::DescriptorImageInfo, 2> imageInfos;
        imageInfos[0].imageView = m_texture.view();
        imageInfos[0].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        imageInfos[0].sampler = m_texture.sampler();

        imageInfos[1].imageView = m_texture2.view();
        imageInfos[1].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        imageInfos[1].sampler = m_texture2.sampler();

        std::array<vk::WriteDescriptorSet, 2> descriptorWrites;
        descriptorWrites[0].dstSet = m_descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;
        descriptorWrites[0].pImageInfo = nullptr;
        descriptorWrites[0].pTexelBufferView = nullptr;
        descriptorWrites[0].pNext = nullptr;

        descriptorWrites[1].dstSet = m_descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        descriptorWrites[1].descriptorCount = static_cast<uint32_t>(imageInfos.size());
        descriptorWrites[1].pBufferInfo = nullptr;
        descriptorWrites[1].pImageInfo = imageInfos.data();
        descriptorWrites[1].pTexelBufferView = nullptr;
        descriptorWrites[1].pNext = nullptr;

        m_ctx.device->updateDescriptorSets(descriptorWrites, {});
    }
}

void HelloTriangleApplication::createColorResources()
{
    m_renderTarget.create(m_ctx, {m_swapChainExtent.width, m_swapChainExtent.height, 1},
                          m_swapChainImageFormat, m_msaaSamples);
}

void HelloTriangleApplication::createDepthResources()
{

    vk::Format depthFormat = findDepthFormat(m_ctx.physicalDevice);

    m_depthBuffer.create(m_ctx, {m_swapChainExtent.width, m_swapChainExtent.height, 1}, depthFormat,
                         m_msaaSamples);
    m_depthBuffer.transitionLayout(m_ctx, vk::ImageLayout::eDepthStencilAttachmentOptimal);
}

void HelloTriangleApplication::createMemoryAllocator()
{
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_1;
    allocatorInfo.physicalDevice = m_ctx.physicalDevice;
    allocatorInfo.device = *m_ctx.device;
    allocatorInfo.instance = *m_ctx.instance;

    vmaCreateAllocator(&allocatorInfo, &m_ctx.allocator);
}

bool HelloTriangleApplication::isDeviceSuitable(const vk::PhysicalDevice &device)
{
    auto properties = device.getProperties();
    auto features = device.getFeatures();

    spdlog::info("Found vulkan device: {}", properties.deviceName);
    // spdlog::info("  {} Driver {}, API {}", deviceProperties, deviceProperties.driverVersion,
    // deviceProperties.apiVersion);

    auto qfi = findQueueFamilies(device, m_surface);
    spdlog::info("  Queue Families: Graphics {}, Compute {}, Transfer {}, Present {}",
                 qfi.graphicsFamily.has_value(), qfi.computeFamily.has_value(),
                 qfi.transferFamily.has_value(), qfi.presentFamily.has_value());

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        auto swapChainDetails = querySwapChainSupport(device, m_surface);
        swapChainAdequate =
            !swapChainDetails.formats.empty() && !swapChainDetails.presentModes.empty();
    }

    // supported features
    vk::PhysicalDeviceFeatures supportedFeatures = device.getFeatures();

    return qfi.graphicsFamily.has_value() && qfi.presentFamily.has_value() && extensionsSupported &&
           swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

vk::SampleCountFlagBits HelloTriangleApplication::getMaxUsableSampleCount()
{
    auto physicalDeviceProperties = m_ctx.physicalDevice.getProperties();

    vk::SampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
                                  physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & vk::SampleCountFlagBits::e64) {
        return vk::SampleCountFlagBits::e64;
    }
    if (counts & vk::SampleCountFlagBits::e32) {
        return vk::SampleCountFlagBits::e32;
    }
    if (counts & vk::SampleCountFlagBits::e16) {
        return vk::SampleCountFlagBits::e16;
    }
    if (counts & vk::SampleCountFlagBits::e8) {
        return vk::SampleCountFlagBits::e8;
    }
    if (counts & vk::SampleCountFlagBits::e4) {
        return vk::SampleCountFlagBits::e4;
    }
    if (counts & vk::SampleCountFlagBits::e2) {
        return vk::SampleCountFlagBits::e2;
    }

    return vk::SampleCountFlagBits::e1;
}

Texture HelloTriangleApplication::createTextureImage(std::string textureFilename,
                                                            vk::Filter filter,
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

void HelloTriangleApplication::createDescriptorSetLayout()
{
    vk::DescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags =
        vk::ShaderStageFlagBits::eVertex |
        vk::ShaderStageFlagBits::eFragment;        // or VK_SHADER_STAGE_ALL_GRAPHICS
    uboLayoutBinding.pImmutableSamplers = nullptr; // idk, something with image sampling

    vk::DescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 1;
    samplerBinding.descriptorCount = 2;
    samplerBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    samplerBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
    samplerBinding.pImmutableSamplers = nullptr;

    std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerBinding};

    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    m_descriptorSetLayout = m_ctx.device->createDescriptorSetLayoutUnique(layoutInfo);
}
