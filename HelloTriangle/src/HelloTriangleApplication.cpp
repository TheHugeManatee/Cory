#include "HelloTriangleApplication.hpp"

#include "TrianglePipeline.hpp"

#include <Cory/Base/Log.hpp>
#include <Cory/Core/Context.hpp>
#include <Cory/Core/ResourceLocator.hpp>
#include <Cory/Cory.hpp>
#include <Cory/UI/SwapChain.hpp>
#include <Cory/UI/Window.hpp>

#include <GLFW/glfw3.h>

HelloTriangleApplication::HelloTriangleApplication()
{
    Cory::Init();

    Cory::ResourceLocator::addSearchPath(TRIANGLE_RESOURCE_DIR);

    CO_APP_INFO("Vulkan instance version is {}", Cory::queryVulkanInstanceVersion());
    static constexpr auto WINDOW_SIZE = glm::i32vec2{1024, 768};
    ctx_ = std::make_unique<Cory::Context>();
    window_ = std::make_unique<Cory::Window>(*ctx_, WINDOW_SIZE, "HelloTriangle");

    pipeline_ = std::make_unique<TrianglePipeline>(*ctx_,
                                                   window_->swapChain().extent(),
                                                   std::filesystem::path{"simple_shader.vert"},
                                                   std::filesystem::path{"simple_shader.frag"});
}

HelloTriangleApplication::~HelloTriangleApplication() = default;

void HelloTriangleApplication::run()
{
    while (!window_->shouldClose()) {
        glfwPollEvents();
    }
}
