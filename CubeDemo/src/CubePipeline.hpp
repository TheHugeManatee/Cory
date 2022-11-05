#pragma once

#include <Cory/Application/Common.hpp>
#include <Cory/Base/Common.hpp>
#include <Cory/Renderer/Common.hpp>

#include <glm/vec2.hpp>

#include <filesystem>

class CubePipeline : Cory::NoCopy {
  public:
    CubePipeline(Cory::Context &context,
                     const Cory::Window &window,
                     const Magnum::Vk::Mesh &mesh,
                     std::filesystem::path vertFile,
                     std::filesystem::path fragFile);
    ~CubePipeline();

    // this pipeline only has one renderpass
    Magnum::Vk::RenderPass &mainRenderPass() { return *mainRenderPass_; }
    Magnum::Vk::Pipeline &pipeline() { return *pipeline_; }
    Magnum::Vk::PipelineLayout &layout() { return *layout_; }

  private:
    void createGraphicsPipeline(const Cory::Window &window,
                                const Magnum::Vk::Mesh &mesh,
                                std::filesystem::path vertFile,
                                std::filesystem::path fragFile);

  private:
    Cory::Context &ctx_;

    Cory::ShaderHandle vertexShader_;
    Cory::ShaderHandle fragmentShader_;

    std::unique_ptr<Magnum::Vk::Pipeline> pipeline_;
    std::unique_ptr<Magnum::Vk::PipelineLayout> layout_;
    std::unique_ptr<Magnum::Vk::RenderPass> mainRenderPass_;
};
