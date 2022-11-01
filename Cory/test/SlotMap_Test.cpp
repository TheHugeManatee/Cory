#include <catch2/catch.hpp>

#include <Cory/Base/SlotMap.hpp>

#include <range/v3/all.hpp>
#include <range/v3/view.hpp>

TEST_CASE("SlotMapHandle", "[Cory/Base]")
{
    using namespace Cory;
    SlotMapHandle h_invalid{};

    CHECK(h_invalid.index() == SlotMapHandle::INVALID_INDEX);
    CHECK(h_invalid.version() == SlotMapHandle::FREE_VERSION);
    CHECK_FALSE(h_invalid.alive());

    SlotMapHandle h1{0, 0};
    CHECK(h1.index() == 0);
    CHECK(h1.version() == 0);
    CHECK(h1.alive());
    CHECK_FALSE(h1.v & SlotMapHandle::FREE_BIT);

    auto h1_v2 = SlotMapHandle::nextVersion(h1);
    CHECK(h1_v2.index() == 0);
    CHECK(h1_v2.version() == 1);
    CHECK(h1_v2.alive());
    CHECK_FALSE(h1_v2.v & SlotMapHandle::FREE_BIT);

    auto h1_free = SlotMapHandle::setFreeBit(h1_v2);
    CHECK(h1_free.index() == 0);
    CHECK(h1_free.version() == (SlotMapHandle::FREE_VERSION | 1));
    CHECK_FALSE(h1_free.alive());
    CHECK(h1_free.v & SlotMapHandle::FREE_BIT);

    auto h1_unfree = SlotMapHandle::clearFreeBit(h1_free);
    CHECK(h1_unfree == h1_v2);
}

TEST_CASE("SlotMap<float>", "[Cory/Base]")
{
    Cory::SlotMap<float> sm;

    auto h1 = sm.insert(1.0f);
    auto h2 = sm.insert(2.0f);
    CHECK(sm[h1] == 1.0f);
    CHECK(sm[h2] == 2.0f);
    CHECK(h1 != h2);

    // updating values in-place does not update the version
    sm[h1] = 41.0;
    sm[h2] = 42.0;
    CHECK(sm[h1] == 41.0);
    CHECK(sm[h2] == 42.0);

    // save address for later
    auto h1_address = &sm[h1];
    auto h2_address = &sm[h2];

    // check size makes sense
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

        auto h3 = sm.insert(3.0);
        CHECK(h3 != h2);
        auto h3_address = &sm[h3];
        // verify that slots are reused but access through the old handle still throws
        CHECK(h3_address == h2_address);
        CHECK_THROWS(sm[h2]);
    }

    SECTION("Updating a value")
    {
        auto h3 = sm.update(h2, 3.0);

        // h2 should be retired
        CHECK_THROWS(sm[h2]);
        // new value should be stored in that handle
        CHECK(sm[h3] == 3.0);

        // trying to update with outdated handle also throws
        CHECK_THROWS(sm.update(h2));
    }
}

static constexpr int UNINIT = 0;
static constexpr int ALIVE = 1;
static constexpr int DEAD = 2;

// test struct that uses an int storage to track its lifetime
struct Foo {
    Foo(int &bookkeep)
        : bookkeep_{bookkeep}
    {
        bookkeep_ = ALIVE;
    }
    ~Foo()
    {
        if (bookkeep_ != ALIVE) {
            FAIL("already destructed or not properly constructed: bookkeep_ = " +
                 std::to_string(bookkeep_));
        }
        bookkeep_ = DEAD;
    }
    int &bookkeep_;
};

TEST_CASE("SlotMap<Foo>", "[Cory/Base]")
{
    using namespace Cory;
    SECTION("Correct construction and destruction of an object")
    {
        SlotMap<Foo> sm;
        int book{UNINIT};
        auto h = sm.emplace(std::ref(book));
        CHECK(book == ALIVE);
        sm.release(h);
        CHECK(book == DEAD);
    }

    SECTION("Destruction of stored objects on Slotmap Destruction")
    {
        int book1{UNINIT};
        int book2{UNINIT};
        {
            SlotMap<Foo> sm;
            auto h1 = sm.emplace(std::ref(book1));
            auto h2 = sm.emplace(std::ref(book2));
            CHECK(book1 == ALIVE);
            CHECK(book2 == ALIVE);

            sm.release(h1);
            CHECK(book1 == DEAD);
            CHECK(book2 == ALIVE);
        }

        CHECK(book1 == DEAD);
        CHECK(book2 == DEAD);
    }

    SECTION("Correct construction and destruction of many stored objects")
    {
        std::srand(std::time(0));
        // poor man's fuzz-testing
        std::vector<int> bookkeeper1(rand() % 512 + 1, UNINIT);
        std::vector<int> bookkeeper2(rand() % 512 + 1, UNINIT);
        {
            SlotMap<Foo> sm;

            auto handles1 =
                bookkeeper1 |
                ranges::views::transform([&](auto &b) { return sm.emplace(std::ref(b)); }) |
                ranges::to<std::vector<SlotMapHandle>>();

            CHECK(ranges::all_of(bookkeeper1, [](const int &v) { return v == ALIVE; }));
            CHECK(ranges::all_of(bookkeeper2, [](const int &v) { return v == UNINIT; }));

            auto handles2 =
                bookkeeper2 |
                ranges::views::transform([&](auto &b) { return sm.emplace(std::ref(b)); }) |
                ranges::to<std::vector<SlotMapHandle>>();

            CHECK(ranges::all_of(bookkeeper1, [](const int &v) { return v == ALIVE; }));
            CHECK(ranges::all_of(bookkeeper2, [](const int &v) { return v == ALIVE; }));

            // invalidating a bunch of handles
            auto handles3 = handles2 |
                            ranges::views::transform([&](auto &h) { return sm.update(h); }) |
                            ranges::to<std::vector<SlotMapHandle>>();
            // old handles have been invalidated
            for (SlotMapHandle &h : handles2) {
                CHECK_THROWS(sm[h]);
            }
            // all objects are still alive
            CHECK(ranges::all_of(bookkeeper1, [](const int &v) { return v == ALIVE; }));
            CHECK(ranges::all_of(bookkeeper2, [](const int &v) { return v == ALIVE; }));

            // releasing the handles
            ranges::for_each(handles1, [&](auto &h) { sm.release(h); });

            // old handles have been invalidated
            for (SlotMapHandle &h : handles1) {
                CHECK_THROWS(sm[h]);
            }
            // only values from set 2 are still alive
            CHECK(ranges::all_of(bookkeeper1, [](const int &v) { return v == DEAD; }));
            CHECK(ranges::all_of(bookkeeper2, [](const int &v) { return v == ALIVE; }));
        }

        // after sm goes out of scope, all objects have been properly destructed
        CHECK(ranges::all_of(bookkeeper1, [](const int &v) { return v == DEAD; }));
        CHECK(ranges::all_of(bookkeeper2, [](const int &v) { return v == DEAD; }));
    }

    SECTION("Iterators and use with ranges")
    {
        std::srand(std::time(0));
        std::vector<int> bookkeeper(rand() % 512 + 1, UNINIT);

        SlotMap<Foo> sm;

        auto handles = bookkeeper |
                       ranges::views::transform([&](auto &b) { return sm.emplace(std::ref(b)); }) |
                       ranges::to<std::vector<SlotMapHandle>>();

        for (const Foo &f : sm) {
            CHECK(f.bookkeep_ == ALIVE);
        }

        // CHECK(ranges::all_of(sm, [](const Foo& f) { return f.bookkeep_ == ALIVE; }));

        ranges::for_each(handles | ranges::views::stride(3),
                         [&](auto handle) { sm.release(handle); });
        CHECK(
            ranges::all_of(bookkeeper | ranges::views::stride(3), [](int b) { return b == DEAD; }));
    }

    SECTION("ResolvableHandle<Foo>")
    {
        SlotMap<Foo> sm;
        int book{UNINIT};
        auto h = sm.emplace(std::ref(book));
        ResolvableHandle rh{sm, h};
        CHECK(rh->bookkeep_ == ALIVE);

        const ResolvableHandle crh{rh};
        CHECK(crh->bookkeep_ == ALIVE);
    }
}
