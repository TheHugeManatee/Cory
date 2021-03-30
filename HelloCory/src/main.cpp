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

    auto render_pass = rpb.create();

    auto &framebuffers = render_pass.swapchain_framebuffers();

    for (uint32_t frame_number = 0; frame_number < 10000; frame_number++) {
        // poll input events
        glfwPollEvents();
        // TODO: process events?

        // TODO: update resources and re-record command buffers

        // draw frame
        {
            // acquire next image
            auto frame_ctx = ctx.swapchain()->next_image();
            if (frame_ctx.should_recreate_swapchain) {
                // TODO recreate swapchain
                continue;
            }

            // issue command buffers
            ctx.record(
                   [&](cory::vk::command_buffer &c) {
                       // make a clear-color from frame number. This will flash with a 120*pi frame
                       // period.
                       VkClearValue clearValue;
                       float flash = abs(sin(frame_number / 220.f));
                       float flash2 = abs(cos(frame_number / 220.f));
                       clearValue.color = {{flash, flash2, 0.0f, 1.0f}};

                       render_pass.begin(c, framebuffers[frame_ctx.index], {clearValue});
                       render_pass.end(c);
                   },
                   ctx.graphics_queue())
                .name(fmt::format("command buffer #{}", frame_number))
                .submit({{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, frame_ctx.acquired}},
                        {frame_ctx.rendered},
                        frame_ctx.in_flight);

            // present the frame
            ctx.swapchain()->present(frame_ctx);
        }
    }

    // synchronize for the last frame to prevent resource destruction before the rendering of the
    // last frame is finished
    vkDeviceWaitIdle(ctx.device());

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