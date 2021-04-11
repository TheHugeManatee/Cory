#pragma once

#include "utils.h"

#include <vulkan/vulkan.h>

#include <filesystem>
#include <map>
#include <string>

namespace cory::vk {

using shader_module = basic_vk_wrapper<VkShaderModule>;
class graphics_context;

enum class shader_type {
    Unknown = 0,
    Vertex = VK_SHADER_STAGE_VERTEX_BIT,
    Geometry = VK_SHADER_STAGE_GEOMETRY_BIT,
    Fragment = VK_SHADER_STAGE_FRAGMENT_BIT,
    Compute = VK_SHADER_STAGE_COMPUTE_BIT,
};

class shader_source {
  public:
    shader_source(shader_type type, std::string source, std::filesystem::path filePath = "Unknown");

    /**
     * Loads a shader from a file. If type is not specified, will try to guess the
     * type from the file extension:
     *  - *.vert: Vertex Shader
     *  - *.geom: Geometry Shader
     *  - *.frag: Fragment Shader
     *  - *.comp: Compute Shader
     */
    shader_source(std::filesystem::path filePath, shader_type type = shader_type::Unknown);

    void define(std::string defName, std::string defValue = "")
    {
        definitions_[defName] = defValue;
    }
    void undefine(std::string defName) { definitions_.erase(defName); }

    auto type() const { return type_; }
    const auto &source() const { return source_; }
    const auto &defines() const { return definitions_; }
    const auto &filePath() const { return filename_; }

  private:
    std::filesystem::path filename_{"Unknown"};
    std::string source_;
    shader_type type_;
    std::map<std::string, std::string> definitions_;
};

class shader {
  public:
    shader(graphics_context &ctx, std::shared_ptr<shader_source> source);

    shader(const shader &) = delete;
    shader &operator=(const shader &) = delete;

    [[nodiscard]] auto spvModule() const noexcept { return module_.get(); }
    [[nodiscard]] auto type() const noexcept { return type_; }

    [[nodiscard]] bool compiled() const noexcept { return compiled_; };
    [[nodiscard]] std::string_view compiler_message() const noexcept { return compiler_message_; }
    // vk::PipelineShaderStageCreateInfo stageCreateInfo();

  private:
    std::vector<uint32_t> compile_to_spv(bool optimize = true);
    std::string preprocessShader();

    // Compiles a shader to SPIR-V assembly. Returns the assembly text
    // as a string.
    std::string compileToAssembly(bool optimize = false);

    graphics_context &ctx_;
    std::shared_ptr<shader_source> source_;
    shader_type type_;
    shader_module module_;

    bool compiled_;
    std::string compiler_message_;
};

} // namespace cory::vk