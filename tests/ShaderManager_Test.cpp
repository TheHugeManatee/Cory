#include <catch2/catch_test_macros.hpp>

#include <Cory/Renderer/Shader.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include "TestUtils.hpp"

static_assert(!std::copyable<Cory::ShaderManager>, "ShaderManager is not designed to be copyable");
static_assert(std::movable<Cory::ShaderManager>, "ShaderManager is designed to be movable");

static constexpr auto testVertexShader = R"(
struct VSInput  {   [[vk::location(0)]] float3 position : POSITION; };
struct VSOutput {   float4 position : SV_Position;                  };

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = float4(input.position.xy, 0.0, 1.0);
    return output;
})";

static constexpr auto testInvalidVertexShader = R"(
struct VSInput  {   [[vk::location(0)]] float3 position : POSITION; };
struct VSOutput {   float4 position : SV_Position;                  };

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = float4(input.position, 0.0, 1.0);
    return output;
})";

using namespace Cory;
TEST_CASE("ShaderManager", "[Cory/Renderer]")
{
    testing::VulkanTester t;

    ShaderManager mgr;
    mgr.setContext(t.ctx());

    CHECK(mgr.shadersInUse() == 0);

    SECTION("Direct shader creation/destruction")
    {
        ShaderHandle shader = mgr.createShader(
            testVertexShader, Gpu::ShaderStageFlagBits::VertexBit, "testVertexShader.vert");
        CHECK(mgr.shadersInUse() == 1);
        CHECK(mgr[shader].valid());
        CHECK(mgr[shader].size() > 0);
        CHECK(mgr[shader].type() == Gpu::ShaderStageFlagBits::VertexBit);

        CHECK_THROWS(mgr.createShader(testInvalidVertexShader,
                                      Gpu::ShaderStageFlagBits::VertexBit,
                                      "testInvalidVertexShader.vert"));

        ShaderHandle invalidHandle;
        CHECK_THROWS(mgr[invalidHandle]);

        mgr.release(shader);
        CHECK_THROWS(mgr[shader]);
        CHECK(mgr.shadersInUse() == 0);
    }
    SECTION("Per-frame lifetime tracking")
    {
        const uint64_t lastFrameInUse = 3;
        ShaderHandle shader = mgr.createShader(
            testVertexShader, Gpu::ShaderStageFlagBits::VertexBit, "testVertexShader.vert");
        CHECK(mgr.shadersInUse() == 1);

        mgr.release(shader, lastFrameInUse);
        CHECK_NOTHROW(mgr[shader]);
        CHECK(mgr.shadersInUse() == 1);

        // Has not been released yet at this point
        mgr.clearDeferredReleases(lastFrameInUse + Cory::MAX_FRAMES_IN_FLIGHT - 1);
        CHECK_NOTHROW(mgr[shader]);
        CHECK(mgr.shadersInUse() == 1);

        // Can be and has been released now
        mgr.clearDeferredReleases(lastFrameInUse + Cory::MAX_FRAMES_IN_FLIGHT);
        CHECK_THROWS(mgr[shader]);
        CHECK(mgr.shadersInUse() == 0);
    }
}