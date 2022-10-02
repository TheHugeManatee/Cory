//
// Created by j on 10/2/2022.
//

#pragma once

#include <filesystem>

#include <Cory/Core/Common.hpp>
#include <Cory/UI/Shader.hpp>

namespace Cory {
class Context;
}

class TrianglePipeline : Cory::NoCopy {
  public:
    TrianglePipeline(Cory::Context &context,
                     std::filesystem::path vertFile,
                     std::filesystem::path fragFile);

  private:
    void createGraphicsPipeline(std::filesystem::path vertFile, std::filesystem::path fragFile);

    Cory::Context &ctx_;

    Cory::Shader vertexShader_;
    Cory::Shader fragmentShader_;
};
