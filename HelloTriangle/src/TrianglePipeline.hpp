//
// Created by j on 10/2/2022.
//

#pragma once

#include <filesystem>

class TrianglePipeline {
  public:
    TrianglePipeline(std::filesystem::path vertFile, std::filesystem::path fragFile);

  private:
    void createGraphicsPipeline(std::filesystem::path vertFile, std::filesystem::path fragFile);

};
