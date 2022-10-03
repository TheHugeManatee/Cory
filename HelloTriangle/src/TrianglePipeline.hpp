//
// Created by j on 10/2/2022.
//

#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/UI/Shader.hpp>

#include <glm/vec2.hpp>

#include <filesystem>

namespace Magnum::Vk {
class Pipeline;
}

namespace Cory {
class Context;
}

class TrianglePipeline : Cory::NoCopy {
  public:
    TrianglePipeline(Cory::Context &context,
                     glm::vec2 viewportDimensions,
                     std::filesystem::path vertFile,
                     std::filesystem::path fragFile);
    ~TrianglePipeline();

  private:
    void createGraphicsPipeline(glm::vec2 viewportDimensions,
                                std::filesystem::path vertFile,
                                std::filesystem::path fragFile);

    Cory::Context &ctx_;

    Cory::Shader vertexShader_;
    Cory::Shader fragmentShader_;

    std::unique_ptr<Magnum::Vk::Pipeline> pipeline_;
};
