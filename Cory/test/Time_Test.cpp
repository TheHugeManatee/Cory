#pragma once

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Cory/Base/Time.hpp>

TEST_CASE("AppClock", "[Time]")
{
    Cory::AppClock::Init();

    auto t1 = Cory::AppClock::now();
    auto t2 = Cory::AppClock::now();
    auto t3 = Cory::AppClock::now();

    REQUIRE(t1 < t2);
    REQUIRE(t2 < t3);

    t1 += Cory::Seconds{0.1};
    t2 += 0.1_s;
    REQUIRE(t1 < t2);

    CHECK(t3 - t1 < 1.0_ms);

    auto tse = [](auto t) { return t.time_since_epoch().count(); };

    CHECK(tse(t1 + Cory::Seconds{0.1}) == tse(t1 + 0.1_s));
    CHECK(tse(t1 + Cory::Seconds{0.1}) == Catch::Approx(tse(t1 + 0.1_s)));
    CHECK(tse(t1 + Cory::Seconds{0.1}) == Catch::Approx(tse(t1 + 100.0_ms)));
    CHECK(tse(t1 + Cory::Seconds{0.1}) == Catch::Approx(tse(t1 + 100'000.0_us)));
    CHECK(tse(t1 + Cory::Seconds{0.1}) == Catch::Approx(tse(t1 + 100'000'000.0_ns)));
}

TEST_CASE("AppClock Elapses as expected", "[Time]") {
    auto t1 = Cory::AppClock::now();
    std::this_thread::sleep_for(30.0_ms);
    auto t2 = Cory::AppClock::now();

    CHECK(t2 - t1 > 0.03_s);
}