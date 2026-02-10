#include <Cory/Base/Math.hpp>

#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>

#include <Cory/Base/FmtUtils.hpp>

TEST_CASE("Spherical <> Cartesian conversion")
{
    using namespace Cory;
    // the computations are pretty imprecise so we need a large epsilon
    static constexpr float EPSILON = 1e-6f;

    std::vector<std::pair<glm::vec3, glm::vec3>> spherical_cartesian_pairs = {
        {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}},
        {{1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}},

        {{1.0, glm::radians(90.0f), 0.0}, {1.0, 0.0, 0.0}},
        {{1.0, glm::radians(180.0f), 0.0}, {0.0, 0.0, -1.0}},
        {{1.0, glm::radians(270.0f), 0.0}, {-1.0, 0.0, 0.0}},
        {{1.0, glm::radians(360.0f), 0.0}, {0.0, 0.0, 1.0}},

        {{1.0, glm::radians(90.0f), glm::radians(90.0f)}, {0.0, 1.0, 0.0}},
        {{1.0, glm::radians(90.0f), glm::radians(180.0f)}, {-1.0, 0.0, 0.0}},
        {{1.0, glm::radians(90.0f), glm::radians(270.0f)}, {0.0, -1.0, 0.0}},
        {{1.0, glm::radians(90.0f), glm::radians(360.0f)}, {1.0, 0.0, 0.0}},

        {{1.0, glm::radians(180.0f), glm::radians(90.0f)}, {0.0, 0.0, -1.0}},
        {{1.0, glm::radians(180.0f), glm::radians(180.0f)}, {0.0, 0.0, -1.0}},
        {{1.0, glm::radians(180.0f), glm::radians(270.0f)}, {0.0, 0.0, -1.0}},
        {{1.0, glm::radians(180.0f), glm::radians(360.0f)}, {0.0, 0.0, -1.0}},

        {{1.0, glm::radians(270.0f), glm::radians(90.0f)}, {0.0, -1.0, 0.0}},
        {{1.0, glm::radians(270.0f), glm::radians(180.0f)}, {1.0, 0.0, 0.0}},
        {{1.0, glm::radians(270.0f), glm::radians(270.0f)}, {0.0, 1.0, 0.0}},
        {{1.0, glm::radians(270.0f), glm::radians(360.0f)}, {-1.0, 0.0, 0.0}},
    };

    auto check_spherical_to_cartesian = [](glm::vec3 spherical, glm::vec3 cartesian) {
        auto computed_cartesian = sphericalToCartesian(spherical);
        CAPTURE(fmt::format("{}", spherical));
        CAPTURE(fmt::format("{}", cartesian));
        CAPTURE(fmt::format("{}", computed_cartesian));
        CHECK(glm::all(glm::epsilonEqual(cartesian, computed_cartesian, EPSILON)));
    };

    for (auto [spherical, cartesian] : spherical_cartesian_pairs) {
        check_spherical_to_cartesian(spherical, cartesian);
    }

    std::vector<std::pair<glm::vec3, glm::vec3>> cartesian_spherical_pairs = {
        {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
        {{0.0f, 0.0f, 1.0f}, {1.0, 0.0, 0.0}},
        {{1.0f, 0.0f, 0.0f}, {1.0, glm::radians(90.0f), 0.0}},
        {{0.0f, 1.0f, 0.0f}, {1.0, glm::radians(90.0f), glm::radians(90.0f)}},

        {{-1.0f, 0.0f, 0.0f}, {1.0, glm::radians(90.0f), 0.0}},
        {{0.0f, 1.0f, 0.0f}, {1.0, glm::radians(90.0f), glm::radians(90.0f)}},
        {{0.0f, 0.0f, 1.0f}, {1.0, 0.0, 0.0}},

    };

    auto check_cartesian_to_spherical = [](glm::vec3 spherical, glm::vec3 cartesian) {
        // can only really check the round trip here
        auto computed_spherical = cartesianToSpherical(cartesian);
        auto computed_cartesian = sphericalToCartesian(computed_spherical);
        CAPTURE(fmt::format("{}", spherical));
        CAPTURE(fmt::format("{}", cartesian));
        CAPTURE(fmt::format("{}", computed_spherical));
        CAPTURE(fmt::format("{}", computed_cartesian));
        CHECK(glm::all(glm::epsilonEqual(cartesian, computed_cartesian, EPSILON)));
    };

    for (auto [spherical, cartesian] : spherical_cartesian_pairs) {
        check_cartesian_to_spherical(spherical, cartesian);
    }
}

TEST_CASE("Transform decomposition round-trip")
{
    using namespace Cory;
    constexpr float epsilon = 1e-4f;

    auto requireMatrixClose = [&](const glm::mat4 &lhs, const glm::mat4 &rhs) {
        for (int col = 0; col < 4; ++col) {
            CAPTURE(col);
            CHECK(glm::all(glm::epsilonEqual(glm::vec4{lhs[col]}, glm::vec4{rhs[col]}, epsilon)));
        }
    };

    const std::vector<DecomposedTransform> transforms = {
        {
            .translation = {0.0f, 0.0f, 0.0f},
            .orientation = eulerYXZToQuaternion({0.0f, 0.0f, 0.0f}),
            .scale = {1.0f, 1.0f, 1.0f},
        },
        {
            .translation = {1.0f, -2.0f, 3.5f},
            .orientation = eulerYXZToQuaternion(glm::vec3{
                glm::radians(22.0f),
                glm::radians(-35.0f),
                glm::radians(75.0f),
            }),
            .scale = {2.0f, 0.5f, 1.25f},
        },
        {
            .translation = {-4.0f, 1.0f, 0.25f},
            .orientation = eulerYXZToQuaternion(glm::vec3{
                glm::radians(-48.0f),
                glm::radians(11.0f),
                glm::radians(-93.0f),
            }),
            .scale = {-1.5f, 2.0f, 0.75f},
        },
    };

    for (const auto &original : transforms) {
        const auto matrix =
            makeTransform(original.translation, original.orientation, original.scale);
        auto decomposed = decomposeTransform(matrix, original.scale);
        REQUIRE(decomposed.has_value());
        CHECK(glm::all(glm::epsilonEqual(original.translation, decomposed->translation, epsilon)));
        CHECK(glm::all(glm::epsilonEqual(original.scale, decomposed->scale, epsilon)));

        const auto recomposed =
            makeTransform(decomposed->translation, decomposed->orientation, decomposed->scale);
        requireMatrixClose(matrix, recomposed);
    }
}

TEST_CASE("Euler YXZ and quaternion conversion consistency")
{
    using namespace Cory;
    constexpr float epsilon = 1e-4f;

    auto requireMatrixClose = [&](const glm::mat4 &lhs, const glm::mat4 &rhs) {
        for (int col = 0; col < 4; ++col) {
            CAPTURE(col);
            CHECK(glm::all(glm::epsilonEqual(glm::vec4{lhs[col]}, glm::vec4{rhs[col]}, epsilon)));
        }
    };

    const std::vector<glm::vec3> rotations = {
        {0.0f, 0.0f, 0.0f},
        {glm::radians(10.0f), glm::radians(-30.0f), glm::radians(80.0f)},
        {glm::radians(-85.0f), glm::radians(45.0f), glm::radians(15.0f)},
    };

    for (const auto &rotation : rotations) {
        const auto orientation = eulerYXZToQuaternion(rotation);
        const auto recoveredRotation = quaternionToEulerYXZ(orientation, rotation);

        const auto m0 = makeTransform(glm::vec3{0.0f}, rotation, glm::vec3{1.0f});
        const auto m1 = makeTransform(glm::vec3{0.0f}, recoveredRotation, glm::vec3{1.0f});
        requireMatrixClose(m0, m1);
    }
}
