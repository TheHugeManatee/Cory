#pragma once

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <filesystem>
#include <map>
#include <string_view>
#include <vector>

namespace Cory {

class GraphicsContext;

enum class ShaderType {
    eUnknown = 0,
    eVertex = VK_SHADER_STAGE_VERTEX_BIT,
    eGeometry = VK_SHADER_STAGE_GEOMETRY_BIT,
    eFragment = VK_SHADER_STAGE_FRAGMENT_BIT,
    eCompute = VK_SHADER_STAGE_COMPUTE_BIT,
};

class ShaderSource {
  public:
    ShaderSource(std::string source, ShaderType type, std::filesystem::path filePath = "Unknown");

    /**
     * Loads a shader from a file. If type is not specified, will try to guess the type from the
     * file extension:
     *  - *.vert: Vertex Shader
     *  - *.geom: Geometry Shader
     *  - *.frag: Fragment Shader
     *  - *.comp: Compute Shader
     */
    ShaderSource(std::filesystem::path filePath, ShaderType type = ShaderType::eUnknown);

    void setDefinition(std::string defName, std::string defValue = "")
    {
        m_macroDefinitions[defName] = defValue;
    }
    void removeDefinition(std::string defName) { m_macroDefinitions.erase(defName); }

    const auto &source() const { return m_source; }
    auto type() const { return m_type; }
    const auto &defines() const { return m_macroDefinitions; }
    const auto &filePath() const { return m_filename; }

  private:
    std::filesystem::path m_filename{"Unknown"};
    std::string m_source;
    ShaderType m_type;
    std::map<std::string, std::string> m_macroDefinitions;
};

class Shader {
  public:
    static std::vector<uint32_t> CompileToSpv(const ShaderSource &source, bool optimize = true);

    Shader(GraphicsContext &ctx, ShaderSource source);

    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;

    Shader(Shader &&rhs)
        : m_module{std::move(rhs.m_module)}
        , m_type{rhs.m_type}
        , m_source{std::move(rhs.m_source)} {};


    Shader &operator=(Shader &&rhs)
    {
        m_module = std::move(rhs.m_module);
        m_type = rhs.m_type;
        m_source = std::move(rhs.m_source);
    };

    auto spvModule() { return m_module.get(); }
    auto type() { return m_type; }

    vk::PipelineShaderStageCreateInfo stageCreateInfo();

  private:
    ShaderSource m_source;
    ShaderType m_type;
    vk::UniqueShaderModule m_module;


    std::string preprocessShader();

    // Compiles a shader to SPIR-V assembly. Returns the assembly text
    // as a string.
    std::string compileToAssembly(bool optimize = false);

};
} // namespace Cory