#include <Cory/Renderer/Shader.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Base/Utils.hpp>
#include <Cory/Renderer/Context.hpp>

#include "SlangCompiler.hpp"

namespace Cory {

ShaderSource::ShaderSource(std::string source,
                           Gpu::ShaderStageFlagBits type,
                           std::filesystem::path filePath,
                           std::string entryPoint)
    : filename_{filePath}
    , source_{std::move(source)}
    , type_{type}
    , entryPoint_{std::move(entryPoint)}
{
}

ShaderSource::ShaderSource(std::filesystem::path filePath,
                           Gpu::ShaderStageFlagBits type,
                           std::string entryPoint)
    : type_{type}
    , filename_{std::move(filePath)}
    , entryPoint_{std::move(entryPoint)}
{
    auto fileBytes = readFile(filename_);
    source_ = std::string{fileBytes.begin(), fileBytes.end()};

    if (type_ == SHADER_TYPE_UNKNOWN) {
        type_ = Shader::deduceTypeFromPath(filename_);
    }
}

std::vector<uint32_t> Shader::CompileToSpv(const ShaderSource &source, bool optimize)
{
    static SlangCompiler compiler;

    auto result = compiler.compileShader(source, optimize);
    if (!result.has_value()) {
        CO_CORE_ERROR("Failed to compile {}: {}", source.filePath().string(), result.error());
        return {};
    }

    return *result;
}

// default is an empty (invalid) shader
Shader::Shader()
    : source_{"", SHADER_TYPE_UNKNOWN, "", ""}
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

    module_ = ctx_->device().createShaderModule(spirvBinary);
    size_ = spirvBinary.size() * sizeof(uint32_t);
    // nameVulkanObject(
    //     ctx_->device(), *module_, fmt::format("SHDR_{}", source.filePath().filename().string()));
}

bool Shader::valid() const
{
    return ctx_ && type_ != SHADER_TYPE_UNKNOWN && module_.isValid();
}

Gpu::ShaderStageFlagBits Shader::deduceTypeFromPath(const std::filesystem::path &path)
{
    auto ext = path.extension();
    if (ext == ".slang") {
        auto stem = path.stem();
        if (!stem.empty()) {
            ext = stem.extension();
        }
    }

    if (ext == ".vert") {
        return Gpu::ShaderStageFlagBits::VertexBit;
    }
    if (ext == ".geom") {
        return Gpu::ShaderStageFlagBits::GeometryBit;
    }
    if (ext == ".frag") {
        return Gpu::ShaderStageFlagBits::FragmentBit;
    }
    if (ext == ".comp") {
        return Gpu::ShaderStageFlagBits::ComputeBit;
    }
    return SHADER_TYPE_UNKNOWN;
}

} // namespace Cory
