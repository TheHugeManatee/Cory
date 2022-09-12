#include <catch2/catch.hpp>

#include <Cory/Core/SlotMap.hpp>

#include <range/v3/view.hpp>

TEST_CASE("SlotMap", "[Cory/Utils]")
{
    Cory::SlotMap<float> sm;

    auto h1 = sm.acquire();
    auto h2 = sm.acquire();
    CHECK(h1 != h2);

    sm[h1] = 41.0;
    sm[h2] = 42.0;
    CHECK(sm[h1] == 41.0);
    CHECK(sm[h2] == 42.0);

    auto h1_address = &sm[h1];
    auto h2_address = &sm[h2];
    CHECK(sm.size() == 2);

    SECTION("Adding many elements and ensure memory stability")
    {
        [[maybe_unused]] auto handles =
            ranges::views::iota(0) | ranges::views::transform([&](auto) { return sm.acquire(); }) |
            ranges::views::take(1000) | ranges::to<std::vector<Cory::SlotMap<int>::Identifier>>();

        CHECK(sm.size() == 1002);

        CHECK(sm[h1] == 41.0);
        auto h1_address_new = &sm[h1];
        // check that address stays stable
        CHECK(h1_address == h1_address_new);
    }

    SECTION("Memory reuse and stability")
    {
        sm.release(h2);
        CHECK_THROWS(sm[h2]);
        CHECK(sm.size() == 1);

        auto h3 = sm.acquire();
        CHECK(h3 != h2);
        auto h3_address = &sm[h3];
        // ensure slots are reused but access through the ID still throws
        CHECK(h3_address == h2_address);
        CHECK_THROWS(sm[h2]);
    }
}
