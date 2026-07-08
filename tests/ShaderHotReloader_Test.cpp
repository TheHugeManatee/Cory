#include <catch2/catch_test_macros.hpp>

#include <Cory/Base/FileWatchManager.hpp>
#include <Cory/Renderer/ShaderHotReloader.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include <Cory/Testing/TestUtils.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;

namespace {
constexpr auto kVertexShaderA = R"(
struct VSInput  { [[vk::location(0)]] float3 position : POSITION; };
struct VSOutput { float4 position : SV_Position; };

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position.xy, 0.0, 1.0);
    return output;
})";

constexpr auto kVertexShaderB = R"(
struct VSInput  { [[vk::location(0)]] float3 position : POSITION; };
struct VSOutput { float4 position : SV_Position; };

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position.xy * 0.5, 0.0, 1.0);
    return output;
})";

void writeShaderFile(const fs::path &filePath, std::string_view source)
{
    std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
    REQUIRE(out.is_open());
    out << source;
    out.flush();
    REQUIRE(out.good());
}

} // namespace

TEST_CASE("ShaderHotReloader initializes, reloads from disk, and cleans up", "[Cory/Renderer]")
{
    using namespace std::chrono_literals;

    Cory::testing::VulkanTester tester;
    auto &ctx = tester.ctx();
    auto &shaderManager = ctx.shaders();

    const auto baselineShaders = shaderManager.shadersInUse();
    const auto tmpDir = fs::temp_directory_path() / "cory_shader_hot_reloader_tests";
    fs::create_directories(tmpDir);
    const auto shaderPath = tmpDir / "hot_reload_test.vert.slang";

    writeShaderFile(shaderPath, kVertexShaderA);

    Cory::ShaderHandle shaderHandle{};
    {
        Cory::ShaderHotReloader reloader;
        reloader.initialize(ctx);
        reloader.addShader({
            .path = shaderPath,
            .stage = Gpu::ShaderStageFlagBits::VertexBit,
            .label = "hot_reload_test.vert.slang",
            .shaderHandle = &shaderHandle,
        });

        REQUIRE(shaderHandle);
        REQUIRE(shaderManager.shadersInUse() == baselineShaders + 1);
        REQUIRE(shaderManager[shaderHandle].valid());

        const auto originalHandle = shaderHandle;
        writeShaderFile(shaderPath, kVertexShaderB);

        const auto deadline = std::chrono::steady_clock::now() + 2s;
        bool reloaded = false;
        while (std::chrono::steady_clock::now() < deadline) {
            ctx.fileWatchManager().processPendingEvents();
            reloader.processPendingReloads(42);
            if (shaderHandle != originalHandle) {
                reloaded = true;
                break;
            }
            std::this_thread::sleep_for(5ms);
        }

        REQUIRE(reloaded);
        REQUIRE(shaderManager[shaderHandle].valid());
    }

    ctx.fileWatchManager().processPendingEvents();
    shaderManager.clearDeferredReleases(42 + Cory::MAX_FRAMES_IN_FLIGHT);
    REQUIRE(shaderManager.shadersInUse() == baselineShaders);

    std::error_code ec;
    fs::remove(shaderPath, ec);
}
