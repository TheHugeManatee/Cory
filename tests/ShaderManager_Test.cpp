#include <catch2/catch_test_macros.hpp>

#include <Cory/Renderer/Shader.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include "TestUtils.hpp"

static_assert(!std::copyable<Cory::ShaderManager>, "ShaderManager is not designed to be copyable");
static_assert(std::movable<Cory::ShaderManager>, "ShaderManager is designed to be movable");

static constexpr auto testVertexShader = R"(
#version 450

layout(location = 0) in vec3 inPosition;

void main() {
    gl_Position = vec4(inPosition.xy, 0.0, 1.0);
})";

static constexpr auto testInvalidVertexShader = R"(
#version 450

void main() {
    gl_Position = vec4(inPosition.xy, 0.0, 1.0);
})";

using namespace Cory;
TEST_CASE("ResourceManager", "[Cory/Renderer]")
{
    testing::VulkanTester t;

    ShaderManager mgr;
    mgr.setContext(t.ctx());

    CHECK(mgr.shadersInUse() == 0);

    SECTION("Shaders")
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
}