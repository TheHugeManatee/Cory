#include "TrianglePipeline.hpp"

#include <Cory/Core/Context.hpp>
#include <Cory/Core/Log.hpp>
#include <Cory/Core/ResourceLocator.hpp>
#include <Cory/Core/Utils.hpp>

TrianglePipeline::TrianglePipeline(Cory::Context &context,
                                   std::filesystem::path vertFile,
                                   std::filesystem::path fragFile)
    : ctx_{context}
{
    createGraphicsPipeline(std::move(vertFile), std::move(fragFile));
}

void TrianglePipeline::createGraphicsPipeline(std::filesystem::path vertFile,
                                              std::filesystem::path fragFile)
{
    vertexShader_ = Cory::Shader(ctx_, Cory::ShaderSource{Cory::ResourceLocator::locate(vertFile)});
    fragmentShader_ = Cory::Shader(ctx_, Cory::ShaderSource{Cory::ResourceLocator::locate(fragFile)});
    CO_APP_INFO("Vertex shader code size: {}", vertexShader_.size());
    CO_APP_INFO("Fragment shader code size: {}", fragmentShader_.size());
}
