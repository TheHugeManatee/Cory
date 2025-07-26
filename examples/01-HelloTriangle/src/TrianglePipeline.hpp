//
// Created by j on 10/2/2022.
//

#pragma once

#include <Cory/Application/Common.hpp>
#include <Cory/Base/Common.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/KDGpuFwd.hpp>

#include <KDGpu/buffer.h>

#include <glm/vec2.hpp>

#include <filesystem>

struct Mesh {
    KDGpu::Buffer vertexBuffer;
    KDGpu::Buffer indexBuffer;
};

class TrianglePipeline : Cory::NoCopy {
  public:
    TrianglePipeline(Cory::Context &context,
                     const Cory::Window &window,
                     const Mesh &mesh,
                     std::filesystem::path vertFile,
                     std::filesystem::path fragFile);
    ~TrianglePipeline();

    // this pipeline only has one renderpass
    KDGpu::RenderPass &mainRenderPass();
    KDGpu::GraphicsPipeline &pipeline();
    KDGpu::PipelineLayout &layout();

  private:
    void createGraphicsPipeline(const Cory::Window &window,
                                const Mesh &mesh,
                                std::filesystem::path vertFile,
                                std::filesystem::path fragFile);

    struct PrivateData;
    std::unique_ptr<PrivateData> data_;
};
