#include <Cory/Log.h>

#include <Cory/vk/command_buffer.h>
#include <Cory/vk/enum_utils.h>
#include <Cory/vk/graphics_context.h>
#include <Cory/vk/instance.h>
#include <Cory/vk/render_pass.h>
#include <Cory/vk/utils.h>

#include <Cory/utils/container.h>

#include <GLFW/glfw3.h>
#include <fmt/ranges.h>
#include <vulkan/vulkan.h>

#include <cstdlib>
#include <optional>

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

int main_main()
{
    Cory::Log::Init();
    // Cory::Log::GetCoreLogger()->set_level(spdlog::level::trace);

    // initialize glfw - this has to be done early
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(1024, 768, "Hello Cory", nullptr, nullptr);

    // collect all required extensions
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    CO_CORE_INFO("GLFW requires {} instance extensions", glfwExtensionCount);
    std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // set up the ApplicationInfo
    VkApplicationInfo app_info{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                               .pApplicationName = "CoryAPITester",
                               .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                               .pEngineName = "Cory",
                               .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                               .apiVersion = VK_API_VERSION_1_2};

    // create the instance
    cory::vk::instance instance =
        cory::vk::instance_builder()
            .application_info(app_info)
            .enabled_extensions(extensions)
            .next(cory::vk::debug_utils_messenger_builder()
                      .message_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
                      .message_type(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
                      .user_callback(cory::vk::default_debug_callback)
                      .ptr())
            .create();

    // list/pick physical device
    const auto devices = instance.physical_devices();
    std::optional<cory::vk::physical_device_info> pickedDevice;
    for (const auto &info : devices) {
        if (!pickedDevice && info.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            pickedDevice = info;
        }
    }

    // initialize the surface
    auto surface = [&, instance_ptr = instance.get()]() -> cory::vk::surface {
        VkSurfaceKHR surface;
        VK_CHECKED_CALL(glfwCreateWindowSurface(instance.get(), window, nullptr, &surface),
                        "Could not create window surface");
        // return a shared_ptr with custom deallocator to destroy the surface
        return cory::vk::make_shared_resource(surface, [instance_ptr](VkSurfaceKHR s) {
            vkDestroySurfaceKHR(instance_ptr, s, nullptr);
        });
    }();

    // create a context
    cory::vk::graphics_context ctx(instance, pickedDevice->device, surface, nullptr);

    // initialize a render pass

    cory::vk::render_pass_builder rpb(ctx);
    auto color_att0 = rpb.add_color_attachment(cory::vk::attachment_description_builder()
                                                   .format(ctx.default_color_format())
                                                   .load_op(VK_ATTACHMENT_LOAD_OP_CLEAR)
                                                   .store_op(VK_ATTACHMENT_STORE_OP_STORE)
                                                   .final_layout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR),
                                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    rpb.add_default_subpass();

    VkAttachmentDescription color_attachment = {};
    // the attachment will have the format needed by the swapchain
    color_attachment.format = ctx.default_color_format();
    // 1 sample, we won't be doing MSAA
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    // we Clear when this attachment is loaded
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // we keep the attachment stored when the renderpass ends
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    // we don't care about stencil
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    // we don't know or care about the starting layout of the attachment
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // after the renderpass ends, the image has to be on a layout ready for display
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref = {};
    // attachment number will index into the pAttachments array in the parent renderpass itself
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // we are going to create 1 subpass, which is the minimum you can do
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

    // connect the color attachment to the info
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    // connect the subpass to the info
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;

    VkRenderPass vk_render_pass;
    VK_CHECKED_CALL(vkCreateRenderPass(ctx.device(), &render_pass_info, nullptr, &vk_render_pass),
                    "could not create render pass");
    auto render_pass = rpb.create();
    
    auto &framebuffers = render_pass.swapchain_framebuffers();

    return EXIT_SUCCESS;
}

int main()
{
    try {
        main_main();
    }
    catch (std::runtime_error &err) {
        CO_APP_ERROR("runtime_error: {}", err.what());
    }
}