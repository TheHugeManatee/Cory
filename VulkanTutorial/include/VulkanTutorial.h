#pragma once

#include <vulkan/vulkan.hpp>

#include <Cory/Application.h>
#include <Cory/Buffer.h>
#include <Cory/Descriptor.h>
#include <Cory/Image.h>
#include <Cory/Mesh.h>
#include <Cory/Utils.h>
#include <Cory/VkUtils.h>

// temporary
#include <Cory/ImGuiLayer.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <memory>
#include <vector>

using namespace Cory;

struct CameraUBOData {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 proj;

  glm::mat4 modelInv;
  glm::mat4 viewInv;
  glm::mat4 projInv;

  glm::vec3 camPos;
  glm::vec3 camFocus;
};

class VulkanTutorialApplication : public Application {
public:
  static constexpr uint32_t WIDTH{800};
  static constexpr uint32_t HEIGHT{600};

  VulkanTutorialApplication();

  void drawSwapchainFrame(FrameUpdateInfo &fui) override;
  void createSwapchainDependentResources() override;
  void destroySwapchainDependentResources() override;

private:
  void init() override;
  void deinit() override;

  void createGraphicsPipeline();
  void createRenderPass();
  void createCommandBuffers();

  void createGeometry();

  void createUniformBuffers();
  Texture createTextureImage(std::string textureFilename, vk::Filter filter,
                             vk::SamplerAddressMode addressMode);

  void updateUniformBuffer(uint32_t imageIndex);

  void createDescriptorSets();

  void createFramebuffers(vk::RenderPass renderPass);

private:
  vk::RenderPass m_renderPass;

  DescriptorSet m_descriptorSet;

  vk::UniquePipelineLayout m_pipelineLayout;
  vk::UniquePipeline m_graphicsPipeline;

  std::vector<vk::Framebuffer> m_swapChainFramebuffers;

  vk::UniqueCommandPool m_commandPool;
  std::vector<vk::UniqueCommandBuffer> m_commandBuffers;
  std::vector<UniformBuffer<CameraUBOData>> m_uniformBuffers;

  std::unique_ptr<Mesh> m_mesh;

  Texture m_texture;
  Texture m_texture2;
};
