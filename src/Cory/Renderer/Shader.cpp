#include <Cory/Renderer/Shader.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Base/Utils.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>

#include <KDGpu/shader_object_options.h>

#include "SlangCompiler.hpp"

namespace {

Gpu::ShaderStageFlags deduceNextStages(Gpu::ShaderStageFlagBits stage)
{
    using StageBit = Gpu::ShaderStageFlagBits;
    Gpu::ShaderStageFlags next;

    switch (stage) {
    case StageBit::VertexBit:
        next |= StageBit::TessellationControlBit;
        next |= StageBit::GeometryBit;
        next |= StageBit::FragmentBit;
        break;
    case StageBit::TessellationControlBit:
        next |= StageBit::TessellationEvaluationBit;
        break;
    case StageBit::TessellationEvaluationBit:
        next |= StageBit::GeometryBit;
        next |= StageBit::FragmentBit;
        break;
    case StageBit::GeometryBit:
        next |= StageBit::FragmentBit;
        break;
    default:
        break;
    }

    return next;
}

} // namespace

namespace Cory {

ShaderSource::ShaderSource(std::string source,
                           Gpu::ShaderStageFlagBits type,
                           std::filesystem::path filePath)
    : filename_{filePath}
    , source_{std::move(source)}
    , type_{type}
{
}

ShaderSource::ShaderSource(std::filesystem::path filePath,
                           Gpu::ShaderStageFlagBits type)
    : type_{type}
    , filename_{std::move(filePath)}
{
    auto fileBytes = readFile(filename_);
    source_ = std::string{fileBytes.begin(), fileBytes.end()};

    if (type_ == SHADER_TYPE_UNKNOWN) {
        type_ = Shader::deduceTypeFromPath(filename_);
    }
}

std::vector<uint32_t> Shader::CompileToSpv(const ShaderSource &source,
                                           bool optimize,
                                           std::string_view entryPoint)
{
    static SlangCompiler compiler;

    auto result = compiler.compileShader(source, entryPoint, optimize);
    if (!result.has_value()) {
        CO_CORE_ERROR("Failed to compile {}: {}", source.filePath().string(), result.error());
        return {};
    }

    return *result;
}

// default is an empty (invalid) shader
Shader::Shader()
    : source_{"", SHADER_TYPE_UNKNOWN, ""}
{
}

Shader::Shader(Context &ctx, ShaderSource source, std::string entryPoint)
    : ctx_{&ctx}
    , source_{std::move(source)}
    , type_{source_.type()}
    , entryPoint_{std::move(entryPoint)}
{
    spirvBinary_ = CompileToSpv(source_, false, entryPoint_);
    if (spirvBinary_.empty()) {
        throw std::runtime_error{"Could not compile shader source to SPIR-V"};
    }

    size_ = spirvBinary_.size() * sizeof(uint32_t);
    nextStages_ = deduceNextStages(type_);

    const auto layouts = ctx_->descriptors().layouts();
    std::string label;
    if (source_.filePath().empty()) {
        label = "ShaderObject";
    }
    else {
        label = source_.filePath().filename().string();
    }

    Gpu::ShaderObjectOptions options{
        .label = label,
        .stage = type_,
        .nextStage = nextStages_,
        .code = spirvBinary_,
        .entryPoint = entryPoint_,
        .bindGroupLayouts = layouts,
        .pushConstantRanges = {},
    };

    shaderObject_ = ctx_->device().createShaderObject(options);
    if (!shaderObject_.isValid()) {
        throw std::runtime_error{"Failed to create shader object"};
    }
}

bool Shader::valid() const
{
    return ctx_ && type_ != SHADER_TYPE_UNKNOWN && shaderObject_.isValid();
}

Gpu::ShaderModule Shader::createShaderModule() const
{
    CO_CORE_DEBUG_ASSERT(ctx_, "Shader has no context to create modules with");
    return ctx_->device().createShaderModule(spirvBinary_);
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
