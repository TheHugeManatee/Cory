#include "ImGuiLayer.h"

#include "Context.h"
#include "Log.h"
#include "VkUtils.h"
#include "GLFW/glfw3.h"

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

ImGuiLayer::ImGuiLayer() {}

void ImGuiLayer::Init(GLFWwindow *window, GraphicsContext &ctx, uint32_t queueFamily,
                      vk::Queue queue,
                      uint32_t minImageCount, vk::RenderPass renderPass)
{
    vk::DescriptorPoolSize pool_sizes[] = {{vk::DescriptorType::eSampler, 1000},
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

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsClassic();

    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo info{};
    info.Instance = *ctx.instance;
    info.PhysicalDevice = ctx.physicalDevice;
    info.Device = *ctx.device;
    info.QueueFamily = queueFamily;
    info.Queue = queue;
    info.PipelineCache = nullptr; // TODO?
    info.DescriptorPool = m_descriptorPool;
    info.Allocator = nullptr;
    info.MinImageCount = minImageCount;
    info.ImageCount = minImageCount;
    info.CheckVkResultFn = check_vk_result;

    info.MinImageCount = minImageCount;
    ImGui_ImplVulkan_Init(&info, renderPass);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple
    // fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the
    // font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in
    // your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture
    // when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below
    // will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to
    // write a double backslash \\ !
    // io.Fonts->AddFontDefault();
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    // ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL,
    // io.Fonts->GetGlyphRangesJapanese()); IM_ASSERT(font != NULL);

    // Upload Fonts
    {
        // Use any command queue
        SingleTimeCommandBuffer cmdBuff(ctx);
        // VkCommandPool command_pool = wd->Frames[wd->FrameIndex].CommandPool;
        // VkCommandBuffer command_buffer = wd->Frames[wd->FrameIndex].CommandBuffer;

        // err = vkResetCommandPool(g_Device, command_pool, 0);
        // check_vk_result(err);
        /*VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(command_buffer, &begin_info);
        check_vk_result(err);*/

        ImGui_ImplVulkan_CreateFontsTexture(cmdBuff.buffer());

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

void ImGuiLayer::Deinit(GraphicsContext &ctx)
{
    ctx.device->destroyDescriptorPool(m_descriptorPool);

        ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}



void ImGuiLayer::NewFrame(GraphicsContext &ctx) {
    ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();


    static bool show_demo_window{true};
    ImGui::ShowDemoWindow(&show_demo_window);
}

void ImGuiLayer::EndFrame(GraphicsContext &ctx) {
    // Rendering
    ImGui::Render();
    //ImDrawData *draw_data = ImGui::GetDrawData();
    //const bool is_minimized =
    //    (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
    //if (!is_minimized) {
    //    memcpy(&wd->ClearValue.color.float32[0], &clear_color, 4 * sizeof(float));
    //    FrameRender(wd, draw_data);
    //    FramePresent(wd);
    //}


    //********************************
    //    TODO NEXT STEPS
    //     - create a render pass that has a proper configuration
    //     - probably needs to have a subpass for imgui
    //     - call ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

}

} // namespace Cory
