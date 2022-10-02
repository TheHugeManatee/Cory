#include "HelloTriangleApplication.hpp"

#include "TrianglePipeline.hpp"

#include <Cory/Core/Context.hpp>
#include <Cory/Core/Log.hpp>
#include <Cory/Core/ResourceLocator.hpp>
#include <Cory/Cory.hpp>
#include <Cory/UI/Window.hpp>

#include <GLFW/glfw3.h>

HelloTriangleApplication::HelloTriangleApplication()
{
    Cory::Init();

    Cory::ResourceLocator::addSearchPath(TRIANGLE_RESOURCE_DIR);

    CO_APP_INFO("Vulkan instance version is {}", Cory::queryVulkanInstanceVersion());

    ctx_ = std::make_unique<Cory::Context>();
    window_ = std::make_unique<Cory::Window>(*ctx_, glm::i32vec2{1024, 768}, "HelloTriangle");
    pipeline_ =
        std::make_unique<TrianglePipeline>("simple_shader.vert.spv", "simple_shader.frag.spv");
}

HelloTriangleApplication::~HelloTriangleApplication() = default;

void HelloTriangleApplication::run()
{
    while (!window_->shouldClose()) {
        glfwPollEvents();
    }
}
