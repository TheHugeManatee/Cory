#include <catch2/catch_test_macros.hpp>

#include <Cory/Base/FmtUtils.hpp>

TEST_CASE("Formatting glm::vec", "[Cory/Base/Formatting]")
{
    SECTION("2 components")
    {
        glm::vec2 v1{1.0f, 2.5f};
        CHECK(fmt::format("{:.2f}", v1) == "(1.00,2.50)");
        CHECK(fmt::format("{:.0f}", v1) == "(1,2)");

        glm::u32vec2 v2{10, 20};
        CHECK(fmt::format("{}", v2) == "(10,20)");
    }

    SECTION("3 components")
    {
        glm::vec3 v1{1.0f, 2.5f, 3.3f};
        CHECK(fmt::format("{:.2f}", v1) == "(1.00,2.50,3.30)");
        CHECK(fmt::format("{:.0f}", v1) == "(1,2,3)");

        glm::u32vec3 v2{10, 20, 30};
        CHECK(fmt::format("{}", v2) == "(10,20,30)");
    }

    SECTION("4 components")
    {
        glm::vec4 v1{1.0f, 2.5f, 3.3f, 44.444};
        CHECK(fmt::format("{:.2f}", v1) == "(1.00,2.50,3.30,44.44)");
        CHECK(fmt::format("{:.0f}", v1) == "(1,2,3,44)");

        glm::u32vec4 v2{10, 20, 30, 444};
        CHECK(fmt::format("{}", v2) == "(10,20,30,444)");
    }
}