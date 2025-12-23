#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Renderer/Common.hpp>

#include <KDGpu/shader_module.h>
#include <KDGpu/shader_object.h>

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace Cory {

class ShaderSource {
  public:
    ShaderSource(std::string source,
                 Gpu::ShaderStageFlagBits type,
                 std::filesystem::path filePath = "Unknown");

    /**
     * Loads a shader from a file. If type is not specified, will try to guess the
     * type from the file extension:
     *  - *.vert: Vertex Shader
     *  - *.geom: Geometry Shader
     *  - *.frag: Fragment Shader
     *  - *.comp: Compute Shader
     */
    ShaderSource(std::filesystem::path filePath,
                 Gpu::ShaderStageFlagBits type = SHADER_TYPE_UNKNOWN);

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
    Gpu::ShaderStageFlagBits type_;
    std::map<std::string, std::string> macroDefinitions_;
};

class Shader : NoCopy {
  public:
    static CompilationResult CompileToSpv(const ShaderSource &source,
                                          bool optimize = true,
                                          std::string_view entryPoint = "main");

    Shader();
    Shader(Context &ctx, ShaderSource source, std::string entryPoint = "main");

    // movable!
    Shader(Shader &&rhs) = default;
    Shader &operator=(Shader &&rhs) = default;

    Gpu::ShaderObject &shaderObject() { return shaderObject_; }
    const Gpu::ShaderObject &shaderObject() const { return shaderObject_; }
    Gpu::Handle<Gpu::ShaderObject_t> shaderHandle() const { return shaderObject_.handle(); }
    Gpu::ShaderStageFlags nextStages() const { return nextStages_; }
    Gpu::ShaderModule createShaderModule() const;
    Gpu::ShaderStageFlagBits type() const { return type_; }
    const std::string &entryPoint() const { return entryPoint_; }
    bool valid() const;
    [[nodiscard]] CompilationError error() const { return error_; }

    // the size in bytes of the compiled shader module
    size_t size() const { return size_; }

    static Gpu::ShaderStageFlagBits deduceTypeFromPath(const std::filesystem::path &path);

  private:
    Context *ctx_{};
    ShaderSource source_;
    Gpu::ShaderStageFlagBits type_{};
    size_t size_{};
    std::vector<uint32_t> spirvBinary_;
    Gpu::ShaderObject shaderObject_;
    Gpu::ShaderStageFlags nextStages_{};
    std::string entryPoint_{"main"};
    CompilationError error_;
};
} // namespace Cory
