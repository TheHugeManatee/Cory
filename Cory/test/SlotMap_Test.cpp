#include <catch2/catch.hpp>

#include <Cory/Base/SlotMap.hpp>

#include <range/v3/all.hpp>
#include <range/v3/view.hpp>

TEST_CASE("SlotMap<float>", "[Cory/Base]")
{
    Cory::SlotMap<float> sm;

    auto h1 = sm.insert();
    auto h2 = sm.insert();
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
            ranges::views::iota(0, 1000) |
            ranges::views::transform([&](auto) { return sm.insert(); }) |
            ranges::to<std::vector<Cory::SlotMapHandle>>();

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

        auto h3 = sm.insert();
        CHECK(h3 != h2);
        auto h3_address = &sm[h3];
        // ensure slots are reused but access through the ID still throws
        CHECK(h3_address == h2_address);
        CHECK_THROWS(sm[h2]);
    }
}

// test struct that uses an int storage to track its lifetime
struct Foo {
    Foo(int &bookkeep)
        : bookkeep_{bookkeep}
    {
        bookkeep_ = 1;
    }
    ~Foo() { bookkeep_ = 2; }
    int &bookkeep_;
};

TEST_CASE("SlotMap<Foo>", "[Cory/Base]")
{
    using namespace Cory;
    SECTION("Correct construction and destruction of an object")
    {
        SlotMap<Foo> sm;
        int book{0};
        auto h = sm.emplace(std::ref(book));
        CHECK(book == 1);
        sm.release(h);
        CHECK(book == 2);
    }

    SECTION("Correct construction and destruction of many stored objects")
    {
        SlotMap<Foo> sm;
        std::srand(std::time(0));
        // poor man's fuzz-testing
        std::vector<int> bookkeeper1(rand() % 512 + 1, 0);
        std::vector<int> bookkeeper2(rand() % 512 + 1, 0);
        WARN(bookkeeper1.size());
        WARN(bookkeeper2.size());

        auto handles1 = bookkeeper1 |
                        ranges::views::transform([&](auto &b) { return sm.emplace(std::ref(b)); }) |
                        ranges::to<std::vector<SlotMapHandle>>();

        CHECK(ranges::all_of(bookkeeper1, [](const int &v) { return v == 1; }));
        CHECK(ranges::all_of(bookkeeper2, [](const int &v) { return v == 0; }));

        auto handles2 = bookkeeper2 |
                        ranges::views::transform([&](auto &b) { return sm.emplace(std::ref(b)); }) |
                        ranges::to<std::vector<SlotMapHandle>>();

        CHECK(ranges::all_of(bookkeeper1, [](const int &v) { return v == 1; }));
        CHECK(ranges::all_of(bookkeeper2, [](const int &v) { return v == 1; }));

        // invalidating a bunch of handles
        auto handles3 = handles2 | ranges::views::transform([&](auto &h) { return sm.update(h); }) |
                        ranges::to<std::vector<SlotMapHandle>>();
        // old handles have been invalidated
        for (SlotMapHandle &h : handles2) {
            CHECK_THROWS(sm[h]);
        }
        // all objects are still alive
        CHECK(ranges::all_of(bookkeeper1, [](const int &v) { return v == 1; }));
        CHECK(ranges::all_of(bookkeeper2, [](const int &v) { return v == 1; }));

        // releasing the handles
        ranges::for_each(handles1, [&](auto &h) { sm.release(h); });

        // old handles have been invalidated
        for (SlotMapHandle &h : handles1) {
            CHECK_THROWS(sm[h]);
        }
        // only values from set 2 are still alive
        CHECK(ranges::all_of(bookkeeper1, [](const int &v) { return v == 2; }));
        CHECK(ranges::all_of(bookkeeper2, [](const int &v) { return v == 1; }));
    }
}
