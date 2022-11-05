#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Renderer/Common.hpp>

#include <Magnum/Vk/Shader.h>
#include <Magnum/Vk/Vulkan.h>

#include <cstdint>
#include <filesystem>
#include <map>
#include <string_view>
#include <vector>

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
        macroDefinitions_[defName] = defValue;
    }
    void removeDefinition(std::string defName) { macroDefinitions_.erase(defName); }

    const auto &source() const { return source_; }
    auto type() const { return type_; }
    const auto &defines() const { return macroDefinitions_; }
    const auto &filePath() const { return filename_; }

  private:
    std::filesystem::path filename_{"Unknown"};
    std::string source_;
    ShaderType type_;
    std::map<std::string, std::string> macroDefinitions_;
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