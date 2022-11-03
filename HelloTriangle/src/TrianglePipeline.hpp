//
// Created by j on 10/2/2022.
//

#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Renderer/Shader.hpp>

#include <glm/vec2.hpp>

#include <filesystem>

namespace Magnum::Vk {
class Pipeline;
class RenderPass;
class PipelineLayout;
} // namespace Magnum::Vk

namespace Cory {
class Context;
class Window;
} // namespace Cory

class TrianglePipeline : Cory::NoCopy {
  public:
    TrianglePipeline(Cory::Context &context,
                     const Cory::Window &window,
                     const Magnum::Vk::Mesh &mesh,
                     std::filesystem::path vertFile,
                     std::filesystem::path fragFile);
    ~TrianglePipeline();

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

    Cory::Shader vertexShader_;
    Cory::Shader fragmentShader_;

    std::unique_ptr<Magnum::Vk::Pipeline> pipeline_;
    std::unique_ptr<Magnum::Vk::PipelineLayout> layout_;
    std::unique_ptr<Magnum::Vk::RenderPass> mainRenderPass_;
};
