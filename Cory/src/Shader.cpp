#include "Shader.h"

#include "Context.h"
#include "Log.h"
#include "Utils.h"

#include <shaderc/shaderc.hpp>

#include <mutex>

namespace Cory {

shaderc_shader_kind ShaderTypeToShaderKind(ShaderType type)
{
  switch (type) {
  case ShaderType::eVertex:
    return shaderc_shader_kind::shaderc_vertex_shader;
  case ShaderType::eGeometry:
    return shaderc_shader_kind::shaderc_geometry_shader;
  case ShaderType::eFragment:
    return shaderc_shader_kind::shaderc_fragment_shader;
  case ShaderType::eCompute:
    return shaderc_shader_kind::shaderc_compute_shader;
  default:
    throw std::runtime_error("Unknown/Unrecognized shader type!");
  }
}

ShaderSource::ShaderSource(std::string source, ShaderType type,
                           std::filesystem::path filePath)
    : m_filename{filePath}
    , m_source{source}
    , m_type{type}
{
}

ShaderSource::ShaderSource(std::filesystem::path filePath, ShaderType type)
    : m_type{type}
    , m_filename{filePath}
{
  auto fileBytes = readFile(filePath);
  m_source = std::string{fileBytes.begin(), fileBytes.end()};

  if (m_type == ShaderType::eUnknown) {
    auto ext = filePath.extension();
    if (ext == ".vert")
      m_type = ShaderType::eVertex;
    else if (ext == ".geom")
      m_type = ShaderType::eGeometry;
    else if (ext == ".frag")
      m_type = ShaderType::eFragment;
    else if (ext == ".comp")
      m_type = ShaderType::eCompute;
  }
}

std::vector<uint32_t> Shader::CompileToSpv(const ShaderSource &source,
                                           bool optimize)
{
  shaderc::Compiler compiler;
  shaderc::CompileOptions options;

  for (const auto [k, v] : source.defines()) {
    options.AddMacroDefinition(k, v);
  }

  if (optimize)
    options.SetOptimizationLevel(shaderc_optimization_level_size);

  const auto &kind = ShaderTypeToShaderKind(source.type());
  auto source_name = source.filePath().filename().string();

  shaderc::SpvCompilationResult spvModule = compiler.CompileGlslToSpv(
      source.source(), kind, source_name.c_str(), options);

  if (spvModule.GetCompilationStatus() != shaderc_compilation_status_success) {
    CO_CORE_ERROR(spvModule.GetErrorMessage());
    return std::vector<uint32_t>();
  }

  return {spvModule.cbegin(), spvModule.cend()};
}

Shader::Shader(GraphicsContext &ctx, ShaderSource source)
    : m_source{source}
    , m_type{m_source.type()}
{

  std::vector<uint32_t> spirvBinary = CompileToSpv(source);

  vk::ShaderModuleCreateInfo createInfo{};
  createInfo.codeSize = spirvBinary.size() * sizeof(uint32_t);
  createInfo.pCode = reinterpret_cast<const uint32_t *>(spirvBinary.data());

  m_module = ctx.device->createShaderModuleUnique(createInfo);
}

vk::PipelineShaderStageCreateInfo Shader::stageCreateInfo()
{
  vk::PipelineShaderStageCreateInfo shaderStageInfo{};
  shaderStageInfo.stage = static_cast<vk::ShaderStageFlagBits>(m_type);
  shaderStageInfo.module = *m_module;
  // entry point -- means we can add multiple entry points in one module
  shaderStageInfo.pName = "main";

  return shaderStageInfo;
}

std::string Shader::preprocessShader()
{
  shaderc::Compiler compiler;
  shaderc::CompileOptions options;

  for (const auto [k, v] : m_source.defines()) {
    options.AddMacroDefinition(k, v);
  }

  const auto &kind = ShaderTypeToShaderKind(m_source.type());
  auto source_name = m_source.filePath().filename().string();

  shaderc::PreprocessedSourceCompilationResult result = compiler.PreprocessGlsl(
      m_source.source(), kind, source_name.c_str(), options);

  if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
    CO_CORE_ERROR(result.GetErrorMessage());
    return "";
  }

  return {result.cbegin(), result.cend()};
}

std::string Shader::compileToAssembly(bool optimize /*= false*/)
{
  shaderc::Compiler compiler;
  shaderc::CompileOptions options;

  for (const auto [k, v] : m_source.defines()) {
    options.AddMacroDefinition(k, v);
  }
  if (optimize)
    options.SetOptimizationLevel(shaderc_optimization_level_size);

  const auto &kind = ShaderTypeToShaderKind(m_source.type());
  auto source_name = m_source.filePath().filename().string();

  shaderc::AssemblyCompilationResult result = compiler.CompileGlslToSpvAssembly(
      m_source.source(), kind, source_name.c_str(), options);

  if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
    CO_CORE_ERROR(result.GetErrorMessage());
    return "";
  }

  return {result.cbegin(), result.cend()};
}

} // namespace Cory
