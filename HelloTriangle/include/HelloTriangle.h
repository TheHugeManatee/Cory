#pragma once

#include <vulkan/vulkan.hpp>

#include <Cory/Application.h>
#include <Cory/Buffer.h>
#include <Cory/Descriptor.h>
#include <Cory/Image.h>
#include <Cory/Mesh.h>
#include <Cory/Utils.h>
#include <Cory/VkUtils.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <memory>
#include <vector>

using namespace Cory;


struct CameraUBOData{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};


class HelloTriangleApplication : public Application {
  public:
    static constexpr uint32_t WIDTH{800};
    static constexpr uint32_t HEIGHT{600};

    static const int MAX_FRAMES_IN_FLIGHT{2};

    void run();

  private:
    void initVulkan();

    void mainLoop();
    void cleanup();

    void createGraphicsPipeline();
    void createRenderPass();
    void createCommandBuffers();

    void recreateSwapChain();
    void cleanupSwapChain();
    void createGeometry();

    void createUniformBuffers();
    Texture createTextureImage(std::string textureFilename, vk::Filter filter,
                                      vk::SamplerAddressMode addressMode);

    void drawFrame();

    void updateUniformBuffer(uint32_t imageIndex);

    void createDescriptorSets();


  private:
    vk::RenderPass m_renderPass;
    
    DescriptorSet m_descriptorSet;

    vk::UniquePipelineLayout m_pipelineLayout;
    vk::UniquePipeline m_graphicsPipeline;

    vk::UniqueCommandPool m_commandPool;
    std::vector<vk::UniqueCommandBuffer> m_commandBuffers;
    std::vector<UniformBuffer<CameraUBOData>> m_uniformBuffers;

    size_t m_currentFrame{};

    std::unique_ptr<Mesh> m_mesh;

    Texture m_texture;
    Texture m_texture2;


};
