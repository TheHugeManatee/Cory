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