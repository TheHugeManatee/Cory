#include "Cory/vk/shader.h"

#include "vk/graphics_context.h"

#include "Log.h"
#include "Utils.h"

#include <shaderc/shaderc.hpp>

#include <fstream>
#include <mutex>

namespace cory::vk {

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

        std::string resolvedLocation = fmt::format("{}/Shaders/{}", RESOURCE_DIR, requested_source);

        auto ir = new shaderc_include_result;
        IncludeData *id = new IncludeData;
        ir->user_data = id;

        id->data = Cory::readFile(resolvedLocation);
        id->resourceName = resolvedLocation;

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

shaderc_shader_kind ShaderTypeToShaderKind(shader_type type)
{
    switch (type) {
    case shader_type::Vertex:
        return shaderc_shader_kind::shaderc_vertex_shader;
    case shader_type::Geometry:
        return shaderc_shader_kind::shaderc_geometry_shader;
    case shader_type::Fragment:
        return shaderc_shader_kind::shaderc_fragment_shader;
    case shader_type::Compute:
        return shaderc_shader_kind::shaderc_compute_shader;
    default:
        throw std::runtime_error("Unknown/Unrecognized shader type!");
    }
}

shader_source::shader_source(shader_type type, std::string source, std::filesystem::path filePath)
    : filename_{filePath}
    , source_{source}
    , type_{type}
{
}

shader_source::shader_source(std::filesystem::path filePath, shader_type type)
    : type_{type}
    , filename_{filePath}
{
    auto fileBytes = Cory::readFile(filePath);
    source_ = std::string{fileBytes.begin(), fileBytes.end()};

    if (type_ == shader_type::Unknown) {
        auto ext = filePath.extension();
        if (ext == ".vert")
            type_ = shader_type::Vertex;
        else if (ext == ".geom")
            type_ = shader_type::Geometry;
        else if (ext == ".frag")
            type_ = shader_type::Fragment;
        else if (ext == ".comp")
            type_ = shader_type::Compute;
    }
}

std::vector<uint32_t> shader::compile_to_spv(bool optimize)
{
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetIncluder(std::make_unique<FileIncludeHandler>());

    for (const auto [k, v] : source_->defines()) {
        options.AddMacroDefinition(k, v);
    }

    if (optimize) options.SetOptimizationLevel(shaderc_optimization_level_size);

    const auto &kind = ShaderTypeToShaderKind(source_->type());
    auto source_name = source_->filePath().filename().string();

    shaderc::SpvCompilationResult spvModule =
        compiler.CompileGlslToSpv(source_->source(), kind, source_name.c_str(), options);

    if (spvModule.GetCompilationStatus() != shaderc_compilation_status_success) {
        CO_CORE_ERROR(
            "Shader compilation error for {}: \n{}", source_name, spvModule.GetErrorMessage());

        compiler_message_ = spvModule.GetErrorMessage();

        return std::vector<uint32_t>();
    }
    // in case of warnings, save the compiler warning message
    if (spvModule.GetNumWarnings() != 0) {
        CO_CORE_INFO("Shader compilation for {} resulted in {} warnings: \n {}",
                     spvModule.GetNumWarnings(),
                     spvModule.GetErrorMessage());
        compiler_message_ = spvModule.GetErrorMessage();
    }

    return {spvModule.cbegin(), spvModule.cend()};
}

shader::shader(graphics_context &ctx, std::shared_ptr<shader_source> source)
    : ctx_{ctx}
    , source_{source}
    , type_{source_->type()}
    , compiled_{false}
{

    std::vector<uint32_t> spirvBinary = compile_to_spv();

    if (spirvBinary.empty()) return;

    VkShaderModuleCreateInfo createInfo{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    createInfo.codeSize = spirvBinary.size() * sizeof(uint32_t);
    createInfo.pCode = reinterpret_cast<const uint32_t *>(spirvBinary.data());

    VkShaderModule vk_module;
    vkCreateShaderModule(ctx.device(), &createInfo, nullptr, &vk_module);
    module_ = shader_module(make_shared_resource(vk_module, [dev = ctx.device()](VkShaderModule m) {
        vkDestroyShaderModule(dev, m, nullptr);
    }));

    compiled_ = true;

    // m_module = ctx.device->createShaderModuleUnique(createInfo);
}

// vk::PipelineShaderStageCreateInfo Shader::stageCreateInfo()
//{
//    vk::PipelineShaderStageCreateInfo shaderStageInfo{};
//    shaderStageInfo.stage = static_cast<vk::ShaderStageFlagBits>(m_type);
//    shaderStageInfo.module = *m_module;
//    // entry point -- means we can add multiple entry points in one module
//    shaderStageInfo.pName = "main";
//
//    return shaderStageInfo;
//}

std::string shader::preprocessShader()
{
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetIncluder(std::make_unique<FileIncludeHandler>());

    for (const auto &[k, v] : source_->defines()) {
        options.AddMacroDefinition(k, v);
    }

    const auto &kind = ShaderTypeToShaderKind(source_->type());
    auto source_name = source_->filePath().filename().string();

    shaderc::PreprocessedSourceCompilationResult result =
        compiler.PreprocessGlsl(source_->source(), kind, source_name.c_str(), options);

    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        CO_CORE_ERROR(result.GetErrorMessage());
        return "";
    }

    return {result.cbegin(), result.cend()};
}

std::string shader::compileToAssembly(bool optimize /*= false*/)
{
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetIncluder(std::make_unique<FileIncludeHandler>());

    for (const auto &[k, v] : source_->defines()) {
        options.AddMacroDefinition(k, v);
    }
    if (optimize) options.SetOptimizationLevel(shaderc_optimization_level_size);

    const auto &kind = ShaderTypeToShaderKind(source_->type());
    auto source_name = source_->filePath().filename().string();

    shaderc::AssemblyCompilationResult result =
        compiler.CompileGlslToSpvAssembly(source_->source(), kind, source_name.c_str(), options);

    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        CO_CORE_ERROR(result.GetErrorMessage());
        return "";
    }

    return {result.cbegin(), result.cend()};
}
} // namespace cory::vk

#ifndef DOCTEST_CONFIG_DISABLE

#include <doctest/doctest.h>

#include "Cory/vk/test_utils.h"

using namespace cory::vk;

TEST_SUITE_BEGIN("basic shaders");

SCENARIO("creating shaders")
{
    GIVEN("valid vertex shader glsl code")
    {
        graphics_context ctx = cory::vk::test_context();
        const char *vertex_shader_glsl = R"glsl(
        #version 450

        void main() 
        {
	        //const array of positions for the triangle
	        const vec3 positions[3] = vec3[3](
		        vec3(1.f,1.f, 0.0f),
		        vec3(-1.f,1.f, 0.0f),
		        vec3(0.f,-1.f, 0.0f)
	        );

	        //output the position of each vertex
	        gl_Position = vec4(positions[gl_VertexIndex], 1.0f);
        }
        )glsl";

        WHEN("creating the shader object")
        {
            auto vertex_shader_src = std::make_shared<shader_source>(
                cory::vk::shader_type::Vertex, vertex_shader_glsl, "vertex.glsl");

            shader vertex_shader(ctx, vertex_shader_src);

            THEN("the shader should be created without errors or warnings")
            {
                CHECK(vertex_shader.compiled());
                CHECK(vertex_shader.compiler_message().size() == 0);
            }
        }
    }

    GIVEN("faulty vertex shader glsl code")
    {
        graphics_context ctx = cory::vk::test_context();
        const char *faulty_vertex_glsl = R"glsl(
        #version 450

        void main() 
        {
	        gl_Position = vec4(positions[gl_VertexIndex], 1.0f);
        }
        )glsl";

        WHEN("creating the shader")
        {
            auto faulty_vertex_shader_src = std::make_shared<shader_source>(
                cory::vk::shader_type::Vertex, faulty_vertex_glsl, "broken_vertex.glsl");

            shader faulty_vertex_shader(ctx, faulty_vertex_shader_src);
            THEN("the created shader should be in an error state")
            {
                CHECK(faulty_vertex_shader.compiled() == false);
                CHECK(faulty_vertex_shader.compiler_message().size() > 0);
            }
        }
    }

    GIVEN("valid fragment shader glsl code")
    {
        graphics_context ctx = cory::vk::test_context();
        const char *fragment_shader_glsl = R"glsl(
        #version 450

        //output write
        layout (location = 0) out vec4 outFragColor;

        void main() 
        {
	        //return red
	        outFragColor = vec4(1.f,0.f,0.f,1.0f);
        }
        )glsl";

        WHEN("creating the shader")
        {
            auto fragment_shader_src = std::make_shared<shader_source>(
                shader_type::Fragment, fragment_shader_glsl, "fragment.glsl");

            shader fragment_shader(ctx, fragment_shader_src);
            THEN("the shader should compile without errors or warnings")
            {

                CHECK(fragment_shader.compiled());
                CHECK(fragment_shader.compiler_message().size() == 0);
            }
        }
    }
}

SCENARIO("using shader defines")
{
    GIVEN("a shader code using some defines")
    {
        graphics_context ctx = cory::vk::test_context();

        const char *fragment_shader_glsl = R"glsl(
        #version 450

        layout (location = 0) out vec4 outFragColor;

        void main() 
        {
        #ifdef NO_PI
	        outFragColor = vec4(1.f,0.f,0.f,1.0f);
        #else
            outFragColor = vec4(sin(PI/2), 0.f, 0.f, 1.0f);
        #endif
        }
        )glsl";

        WHEN("creating the shader without any defines")
        {
            auto fragment_shader_src = std::make_shared<shader_source>(
                shader_type::Fragment, fragment_shader_glsl, "fragment.glsl");

            THEN("the shader compilation should fail")
            {
                shader fragment_shader(ctx, fragment_shader_src);
                CHECK(!fragment_shader.compiled());
                CHECK(fragment_shader.compiler_message().size() > 0);
            }
        }
        WHEN("creating the shader with a define with no value")
        {
            auto fragment_shader_src = std::make_shared<shader_source>(
                shader_type::Fragment, fragment_shader_glsl, "fragment.glsl");
            fragment_shader_src->define("NO_PI");

            THEN("the shader compilation should succeed")
            {
                shader fragment_shader(ctx, fragment_shader_src);
                CHECK(fragment_shader.compiled());
                CHECK(fragment_shader.compiler_message().size() == 0);
            }
        }
        WHEN("creating the shader with a defined value")
        {
            auto fragment_shader_src = std::make_shared<shader_source>(
                shader_type::Fragment, fragment_shader_glsl, "fragment.glsl");
            fragment_shader_src->define("PI", "3.1415");

            THEN("the shader compilation should succeed")
            {
                shader fragment_shader(ctx, fragment_shader_src);
                CHECK(fragment_shader.compiled());
                CHECK(fragment_shader.compiler_message().size() == 0);
            }
        }
    }
}
TEST_SUITE_END();

#endif