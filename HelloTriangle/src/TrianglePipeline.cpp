#include "TrianglePipeline.hpp"

#include <Cory/Core/Log.hpp>
#include <Cory/Core/ResourceLocator.hpp>
#include <Cory/Core/Utils.hpp>

TrianglePipeline::TrianglePipeline(std::filesystem::path vertFile, std::filesystem::path fragFile)
{
    createGraphicsPipeline(std::move(vertFile), std::move(fragFile));
}

void TrianglePipeline::createGraphicsPipeline(std::filesystem::path vertFile,
                                              std::filesystem::path fragFile)
{
    auto vertCode = Cory::readFile(Cory::ResourceLocator::locate(vertFile));
    auto fragCode = Cory::readFile(Cory::ResourceLocator::locate(fragFile));

    CO_APP_INFO("Vertex shader code size: {}", vertCode.size());
    CO_APP_INFO("Fragment shader code size: {}", fragCode.size());
}
