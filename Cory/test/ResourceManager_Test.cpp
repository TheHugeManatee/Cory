#include <catch2/catch_test_macros.hpp>

#include <Cory/Renderer/ResourceManager.hpp>
#include <Cory/Renderer/Shader.hpp>

#include <Magnum/Vk/Buffer.h>

#include "TestUtils.hpp"

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

    ResourceManager mgr;
    mgr.setContext(t.ctx());

    auto rsrcInUse = mgr.resourcesInUse();
    CHECK(rsrcInUse[ResourceType::Buffer] == 0);
    CHECK(rsrcInUse[ResourceType::Shader] == 0);

    SECTION("Buffers")
    {
        BufferHandle buffer = mgr.createBuffer("Test Buffer",
                                               1024,
                                               Cory::BufferUsageBits::StorageBuffer,
                                               MemoryFlagBits::HostCoherent);
        CHECK(mgr.resourcesInUse()[ResourceType::Buffer] == 1);

        CHECK(mgr[buffer].dedicatedMemory().size() == 1024);

        BufferHandle invalidHandle;
        CHECK_THROWS(mgr[invalidHandle]);

        mgr.release(buffer);
        CHECK_THROWS(mgr[buffer]);
        CHECK(mgr.resourcesInUse()[ResourceType::Buffer] == 0);
    }

    SECTION("Shaders")
    {
        ShaderHandle shader =
            mgr.createShader(testVertexShader, Cory::ShaderType::eVertex, "testVertexShader.vert");
        CHECK(mgr.resourcesInUse()[ResourceType::Shader] == 1);
        CHECK(mgr[shader].valid());
        CHECK(mgr[shader].size() > 0);
        CHECK(mgr[shader].type() == Cory::ShaderType::eVertex);

        CHECK_THROWS(mgr.createShader(
            testInvalidVertexShader, Cory::ShaderType::eVertex, "testInvalidVertexShader.vert"));

        ShaderHandle invalidHandle;
        CHECK_THROWS(mgr[invalidHandle]);

        mgr.release(shader);
        CHECK_THROWS(mgr[shader]);
        CHECK(mgr.resourcesInUse()[ResourceType::Shader] == 0);
    }
}