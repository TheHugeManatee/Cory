
#include <catch2/catch_test_macros.hpp>

#include <Cory/Renderer/FrameResourceLifetimeHelper.hpp>

TEST_CASE("Frame Resource Lifetime Manager")
{
    Cory::FrameResourceLifetimeHelper<std::string> helper;

    helper.scheduleForRelease("Frame 1 - Buffer", 1);
    helper.scheduleForRelease("Frame 1 - Texture", 1);

    auto releasable_f1 = helper.collectReleasableResources(1);
    REQUIRE(releasable_f1.size() == 0);

    helper.scheduleForRelease("Frame 2", 2);
    helper.scheduleForRelease("Frame 3", 3);

    auto releasable_f2 = helper.collectReleasableResources(2);
    REQUIRE(releasable_f2.size() == 0);

    auto releasable_f3 = helper.collectReleasableResources(3);
    REQUIRE(releasable_f3.size() == 2);
    CHECK((releasable_f3[0] == "Frame 1 - Buffer" || releasable_f3[0] == "Frame 1 - Texture"));

    auto releasable_f3_second = helper.collectReleasableResources(3);
    REQUIRE(releasable_f3_second.size() == 0);

    auto releasable_f4 = helper.collectReleasableResources(4);
    REQUIRE(releasable_f4.size() == 1);
    CHECK(releasable_f4[0] == "Frame 2");


    helper.scheduleForRelease("F4", 4);
    helper.scheduleForRelease("F5", 5);
    helper.scheduleForRelease("F6", 6);

    auto releasable_f10 = helper.collectReleasableResources(10);
    REQUIRE(releasable_f10.size() == 4);
    CHECK(releasable_f10[0] == "Frame 3");
    CHECK(releasable_f10[1] == "F4");
    CHECK(releasable_f10[2] == "F5");
    CHECK(releasable_f10[3] == "F6");
}