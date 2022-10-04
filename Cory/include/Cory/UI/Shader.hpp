#pragma once

#include <Cory/Base/Common.hpp>

#include <Magnum/Vk/Shader.h>
#include <MagnumExternal/Vulkan/flextVk.h>

#include <cstdint>
#include <filesystem>
#include <map>
#include <string_view>
#include <vector>

namespace Cory {

class Context;

enum class ShaderType {
    eUnknown = 0,
    eVertex = VK_SHADER_STAGE_VERTEX_BIT,
    eGeometry = VK_SHADER_STAGE_GEOMETRY_BIT,
    eFragment = VK_SHADER_STAGE_FRAGMENT_BIT,
    eCompute = VK_SHADER_STAGE_COMPUTE_BIT,
};
} // namespace Cory
DECLARE_ENUM_BITFIELD(Cory::ShaderType);

namespace Cory {

class ShaderSource {
  public:
    ShaderSource(std::string source, ShaderType type, std::filesystem::path filePath = "Unknown");

    /**
     * Loads a shader from a file. If type is not specified, will try to guess the
     * type from the file extension:
     *  - *.vert: Vertex Shader
     *  - *.geom: Geometry Shader
     *  - *.frag: Fragment Shader
     *  - *.comp: Compute Shader
     */
    ShaderSource(std::filesystem::path filePath, ShaderType type = ShaderType::eUnknown);

    // copyable
    ShaderSource(const ShaderSource &rhs) = default;
    ShaderSource &operator=(const ShaderSource &rhs) = default;
    // movable
    ShaderSource(ShaderSource &&rhs) = default;
    ShaderSource &operator=(ShaderSource &&rhs) = default;

    void setDefinition(std::string defName, std::string defValue = "")
    {
        m_macroDefinitions[defName] = defValue;
    }
    void removeDefinition(std::string defName) { m_macroDefinitions.erase(defName); }

    const auto &source() const { return source_; }
    auto type() const { return type_; }
    const auto &defines() const { return m_macroDefinitions; }
    const auto &filePath() const { return m_filename; }

  private:
    std::filesystem::path m_filename{"Unknown"};
    std::string source_;
    ShaderType type_;
    std::map<std::string, std::string> m_macroDefinitions;
};

class Shader {
  public:
    static std::vector<uint32_t> CompileToSpv(const ShaderSource &source, bool optimize = true);

    Shader();
    Shader(Context &ctx, ShaderSource source);

    // copyable!
    Shader(const Shader &rhs) = default;
    Shader &operator=(const Shader &rhs) = default;
    // movable!
    Shader(Shader &&rhs) = default;
    Shader &operator=(Shader &&rhs) = default;

    Magnum::Vk::Shader &module() { return *module_; }
    ShaderType type() const { return type_; }
    bool valid() const;

    // vk::PipelineShaderStageCreateInfo stageCreateInfo();

    // the size in bytes of the compiled shader module
    size_t size() { return size_; }

  private:
    Context *ctx_{};
    ShaderSource source_;
    ShaderType type_{};
    size_t size_{};
    std::shared_ptr<Magnum::Vk::Shader> module_;

    std::string preprocessShader();

    // Compiles a shader to SPIR-V assembly. Returns the assembly text
    // as a string.
    std::string compileToAssembly(bool optimize = false);
};
} // namespace Cory