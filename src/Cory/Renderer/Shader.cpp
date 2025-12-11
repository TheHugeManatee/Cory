#include <Cory/Renderer/Shader.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Base/Utils.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/VulkanUtils.hpp>

#include <shaderc/shaderc.hpp>

#include <fstream>
#include <mutex>

namespace Cory {

class FileIncludeHandler : public shaderc::CompileOptions::IncluderInterface {
  public:
    struct IncludeData {
        std::vector<char> data;
        std::string resourceName;
    };

    // Handles shaderc_include_resolver_fn callbacks.
    shaderc_include_result *GetInclude(const char *requested_source,
                                       shaderc_include_type type,
                                       const char *requesting_source,
                                       size_t include_depth) override
    {
        CO_CORE_DEBUG("Include file {} from {}", requested_source, requesting_source);

        auto resolvedLocation = ResourceLocator::Locate(requested_source);

        auto ir = new shaderc_include_result;
        IncludeData *id = new IncludeData;
        ir->user_data = id;

        id->data = readFile(resolvedLocation);
        id->resourceName = resolvedLocation.string();

        ir->content = id->data.data();
        ir->content_length = id->data.size();
        ir->source_name = id->resourceName.data();
        ir->source_name_length = id->resourceName.size();

        return ir;
    };

    // Handles shaderc_include_result_release_fn callbacks.
    void ReleaseInclude(shaderc_include_result *data) override
    {
        delete (IncludeData *)data->user_data;
        delete data;
    };

    virtual ~FileIncludeHandler() = default;
};

shaderc_shader_kind ShaderTypeToShaderKind(Gpu::ShaderStageFlagBits type)
{
    switch (type) {
        using enum Gpu::ShaderStageFlagBits;
    case VertexBit:
        return shaderc_shader_kind::shaderc_vertex_shader;
    case GeometryBit:
        return shaderc_shader_kind::shaderc_geometry_shader;
    case FragmentBit:
        return shaderc_shader_kind::shaderc_fragment_shader;
    case ComputeBit:
        return shaderc_shader_kind::shaderc_compute_shader;
    default:
        throw std::runtime_error("Unknown/Unrecognized shader type!");
    }
}

ShaderSource::ShaderSource(std::string source,
                           Gpu::ShaderStageFlagBits type,
                           std::filesystem::path filePath)
    : filename_{filePath}
    , source_{std::move(source)}
    , type_{type}
{
}

ShaderSource::ShaderSource(std::filesystem::path filePath, Gpu::ShaderStageFlagBits type)
    : type_{type}
    , filename_{std::move(filePath)}
{
    auto fileBytes = readFile(filename_);
    source_ = std::string{fileBytes.begin(), fileBytes.end()};

    if (type_ == SHADER_TYPE_UNKNOWN) {
        auto ext = filename_.extension();
        if (ext == ".vert")
            type_ = Gpu::ShaderStageFlagBits::VertexBit;
        else if (ext == ".geom")
            type_ = Gpu::ShaderStageFlagBits::GeometryBit;
        else if (ext == ".frag")
            type_ = Gpu::ShaderStageFlagBits::FragmentBit;
        else if (ext == ".comp")
            type_ = Gpu::ShaderStageFlagBits::ComputeBit;
    }
}

std::vector<uint32_t> Shader::CompileToSpv(const ShaderSource &source, bool optimize)
{
    // static for now, we should really move this to a global static instance
    static shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetIncluder(std::make_unique<FileIncludeHandler>());

    for (const auto [k, v] : source.defines()) {
        options.AddMacroDefinition(k, v);
    }

    if (optimize) options.SetOptimizationLevel(shaderc_optimization_level_size);

    const auto &kind = ShaderTypeToShaderKind(source.type());
    auto source_name = source.filePath().filename().string();

    shaderc::SpvCompilationResult spvModule =
        compiler.CompileGlslToSpv(source.source(), kind, source_name.c_str(), options);

    if (spvModule.GetCompilationStatus() != shaderc_compilation_status_success) {
        CO_CORE_ERROR(
            "Failed to compile {}: {}", source.filePath().string(), spvModule.GetErrorMessage());
        return std::vector<uint32_t>();
    }

    return {spvModule.cbegin(), spvModule.cend()};
}

// default is an empty (invalid) shader
Shader::Shader()
    : source_{"", SHADER_TYPE_UNKNOWN, ""}
{
}

Shader::Shader(Context &ctx, ShaderSource source)
    : ctx_{&ctx}
    , source_{source}
    , type_{source_.type()}
{
    std::vector<uint32_t> spirvBinary = CompileToSpv(source, false);
    if (spirvBinary.empty()) {
        throw std::runtime_error{"Could not compile shader source to SPIR-V"};
    }

    // module_ = std::make_shared<Magnum::Vk::Shader>(ctx.device(), info);
    size_ = spirvBinary.size() * sizeof(uint32_t);
    // nameVulkanObject(
    //     ctx_->device(), *module_, fmt::format("SHDR_{}", source.filePath().filename().string()));
}

// vk::PipelineShaderStageCreateInfo Shader::stageCreateInfo()
//{
//     vk::PipelineShaderStageCreateInfo shaderStageInfo{};
//     shaderStageInfo.stage = static_cast<vk::ShaderStageFlagBits>(type_);
//     shaderStageInfo.module = *module_;
//     // entry point -- means we can add multiple entry points in one module
//     shaderStageInfo.pName = "main";
//
//     return shaderStageInfo;
// }

std::string Shader::preprocessShader()
{
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetIncluder(std::make_unique<FileIncludeHandler>());

    for (const auto [k, v] : source_.defines()) {
        options.AddMacroDefinition(k, v);
    }

    const auto &kind = ShaderTypeToShaderKind(source_.type());
    auto source_name = source_.filePath().filename().string();

    shaderc::PreprocessedSourceCompilationResult result =
        compiler.PreprocessGlsl(source_.source(), kind, source_name.c_str(), options);

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
    options.SetIncluder(std::make_unique<FileIncludeHandler>());

    for (const auto [k, v] : source_.defines()) {
        options.AddMacroDefinition(k, v);
    }
    if (optimize) options.SetOptimizationLevel(shaderc_optimization_level_size);

    const auto &kind = ShaderTypeToShaderKind(source_.type());
    auto source_name = source_.filePath().filename().string();

    shaderc::AssemblyCompilationResult result =
        compiler.CompileGlslToSpvAssembly(source_.source(), kind, source_name.c_str(), options);

    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        CO_CORE_ERROR(result.GetErrorMessage());
        return "";
    }

    return {result.cbegin(), result.cend()};
}
bool Shader::valid() const
{
    return ctx_ && type_ != SHADER_TYPE_UNKNOWN && module_ != nullptr;
}

} // namespace Cory
