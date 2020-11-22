#pragma once

#include <vulkan/vulkan.hpp>

#include <Cory/Application.h>
#include <Cory/Buffer.h>
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
    void createVertexBuffers(const std::vector<Vertex> &vertices);
    void createIndexBuffer(const std::vector<uint16_t> &indices);
    void createUniformBuffers();
    void createDescriptorSetLayout();
    Texture createTextureImage(std::string textureFilename, vk::Filter filter,
                                      vk::SamplerAddressMode addressMode);

    void drawFrame();

    void updateUniformBuffer(uint32_t imageIndex);

    void createDescriptorPool();
    void createDescriptorSets();


  private:
    vk::RenderPass m_renderPass;
    vk::UniqueDescriptorSetLayout m_descriptorSetLayout;
    vk::UniqueDescriptorPool m_descriptorPool;
    std::vector<vk::DescriptorSet> m_descriptorSets;
    vk::UniquePipelineLayout m_pipelineLayout;
    vk::UniquePipeline m_graphicsPipeline;

    vk::UniqueCommandPool m_commandPool;
    std::vector<vk::UniqueCommandBuffer> m_commandBuffers;
    std::vector<Buffer> m_uniformBuffers;

    size_t m_currentFrame{};

    uint32_t m_numVertices;
    Buffer m_vertexBuffer;
    Buffer m_indexBuffer;

    Texture m_texture;
    Texture m_texture2;


};
