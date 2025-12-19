#include <catch2/catch_test_macros.hpp>

#include <Cory/Renderer/SlangCompiler.hpp>

TEST_CASE("Shader Compilation", "[Cory/Renderer]")
{
    Cory::SlangCompiler compiler;

    SECTION("Compiling a simple compute shader")
    {
        Cory::ShaderSource source{R"(
            RWStructuredBuffer<float> result;
            [shader("compute")]
            [numthreads(1,1,1)]
            void computeMain(uint3 threadId : SV_DispatchThreadID)
            {
              result[threadId.x] = threadId.x;
            })",
                                  Gpu::ShaderStageFlagBits::ComputeBit,
                                  "TestShader.slang"};

        auto result = compiler.compileShader(source, "computeMain", false);

        if (!result) {
            FAIL(result.error());
        }
        REQUIRE(result.has_value());
        CHECK(result.value().size() > 0);
    }
    SECTION("Compiling an invalid compute shader")
    {
        Cory::ShaderSource source{R"(
            RWStructuredBuffer<float> result;
            [shader("compute")]
            [numthreads(1,1,1)]
            void computeMain(uint3 threadId : SV_DispatchThreadID)
            {
              result[threadId x] = threadId.x;
            })",
                                  Gpu::ShaderStageFlagBits::ComputeBit,
                                  "TestShader.slang"};
        auto result = compiler.compileShader(source, "computeMain", false);

        REQUIRE(!result);
        INFO(result.error());
        CHECK(result.error().size() > 0);
    }
    SECTION("Compiling a compute shader that requires a file that does not exist")
    {
        Cory::ShaderSource source{R"(
            #include "NonExistentFile";

            RWStructuredBuffer<float> result;
            [shader("compute")]
            [numthreads(1,1,1)]
            void computeMain(uint3 threadId : SV_DispatchThreadID)
            {
              result[threadId.x] = threadId.x;
            })",
                                  Gpu::ShaderStageFlagBits::ComputeBit,
                                  "TestShader.slang"};
        auto result = compiler.compileShader(source, "computeMain", false);

        REQUIRE(!result);
        INFO(result.error());
        CHECK(result.error().size() > 0);
    }
}
