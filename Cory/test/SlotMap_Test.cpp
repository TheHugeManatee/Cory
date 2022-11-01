#include <catch2/catch.hpp>

#include <Cory/Base/SlotMap.hpp>

#include <gsl/gsl>
#include <range/v3/all.hpp>
#include <range/v3/view.hpp>

TEST_CASE("SlotMapHandle", "[Cory/Base]")
{
    using namespace Cory;
    SlotMapHandle h_invalid{};

    CHECK(h_invalid.index() == SlotMapHandle::INVALID_INDEX);
    CHECK(h_invalid.version() == 0);
    CHECK_FALSE(h_invalid.alive());

    SlotMapHandle h1{0, 0};
    CHECK(h1.index() == 0);
    CHECK(h1.version() == 0);
    CHECK(h1.alive());
    // CHECK_FALSE(h1.v & SlotMapHandle::FREE_BIT);

    auto h1_v2 = SlotMapHandle::nextVersion(h1);
    CHECK(h1_v2.index() == 0);
    CHECK(h1_v2.version() == 1);
    CHECK(h1_v2.alive());
    // CHECK_FALSE(h1_v2.v & SlotMapHandle::FREE_BIT);

    auto h1_free = SlotMapHandle::setFreeBit(h1_v2);
    CHECK(h1_free.index() == 0);
    CHECK(h1_free.version() == 1);
    CHECK_FALSE(h1_free.alive());
    // CHECK(h1_free.v & SlotMapHandle::FREE_BIT);

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

// test struct that uses an external storage to track its lifetime
enum ObjectState {
    UNINIT,
    ALIVE,
    DEAD,
};
struct Foo {
    Foo(ObjectState &bookkeep)
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
    ObjectState &bookkeep_;
};

TEST_CASE("SlotMap<Foo>", "[Cory/Base]")
{
    using namespace Cory;
    SECTION("Correct construction and destruction of an object")
    {
        SlotMap<Foo> sm;
        ObjectState bookkeeper{UNINIT};
        auto h = sm.emplace(std::ref(bookkeeper));
        CHECK(bookkeeper == ALIVE);
        sm.release(h);
        CHECK(bookkeeper == DEAD);
    }

    SECTION("Destruction of stored objects on Slotmap Destruction")
    {
        ObjectState bookkeeper1{UNINIT};
        ObjectState bookkeeper2{UNINIT};
        {
            SlotMap<Foo> sm;
            auto h1 = sm.emplace(std::ref(bookkeeper1));
            auto h2 = sm.emplace(std::ref(bookkeeper2));
            CHECK(bookkeeper1 == ALIVE);
            CHECK(bookkeeper2 == ALIVE);

            sm.release(h1);
            CHECK(bookkeeper1 == DEAD);
            CHECK(bookkeeper2 == ALIVE);
        }

        CHECK(bookkeeper1 == DEAD);
        CHECK(bookkeeper2 == DEAD);
    }

    SECTION("Correct construction and destruction of many stored objects")
    {
        std::srand(std::time(0));
        // poor man's fuzz-testing
        std::vector<ObjectState> bookkeeper1(rand() % 512 + 1, UNINIT);
        std::vector<ObjectState> bookkeeper2(rand() % 512 + 1, UNINIT);
        {
            SlotMap<Foo> sm;

            auto handles1 =
                bookkeeper1 |
                ranges::views::transform([&](auto &b) { return sm.emplace(std::ref(b)); }) |
                ranges::to<std::vector<SlotMapHandle>>();

            CHECK(ranges::all_of(bookkeeper1, [](const ObjectState &v) { return v == ALIVE; }));
            CHECK(ranges::all_of(bookkeeper2, [](const ObjectState &v) { return v == UNINIT; }));

            auto handles2 =
                bookkeeper2 |
                ranges::views::transform([&](auto &b) { return sm.emplace(std::ref(b)); }) |
                ranges::to<std::vector<SlotMapHandle>>();

            CHECK(ranges::all_of(bookkeeper1, [](const ObjectState &v) { return v == ALIVE; }));
            CHECK(ranges::all_of(bookkeeper2, [](const ObjectState &v) { return v == ALIVE; }));

            // invalidating a bunch of handles
            auto handles3 = handles2 |
                            ranges::views::transform([&](auto &h) { return sm.update(h); }) |
                            ranges::to<std::vector<SlotMapHandle>>();
            // old handles have been invalidated
            for (SlotMapHandle &h : handles2) {
                CHECK_THROWS(sm[h]);
            }
            // all objects are still alive
            CHECK(ranges::all_of(bookkeeper1, [](const ObjectState &v) { return v == ALIVE; }));
            CHECK(ranges::all_of(bookkeeper2, [](const ObjectState &v) { return v == ALIVE; }));

            // releasing the handles
            ranges::for_each(handles1, [&](auto &h) { sm.release(h); });

            // old handles have been invalidated
            for (SlotMapHandle &h : handles1) {
                CHECK_THROWS(sm[h]);
            }
            // only values from set 2 are still alive
            CHECK(ranges::all_of(bookkeeper1, [](const ObjectState &v) { return v == DEAD; }));
            CHECK(ranges::all_of(bookkeeper2, [](const ObjectState &v) { return v == ALIVE; }));
        }

        // after sm goes out of scope, all objects have been properly destructed
        CHECK(ranges::all_of(bookkeeper1, [](const ObjectState &v) { return v == DEAD; }));
        CHECK(ranges::all_of(bookkeeper2, [](const ObjectState &v) { return v == DEAD; }));
    }

    SECTION("Iterators and use with ranges")
    {
        std::srand(std::time(0));
        std::vector<ObjectState> bookkeeper(rand() % 512 + 1, UNINIT);

        SlotMap<Foo> sm;

        auto handles = bookkeeper |
                       ranges::views::transform([&](auto &b) { return sm.emplace(std::ref(b)); }) |
                       ranges::to<std::vector<SlotMapHandle>>();

        for (auto it = sm.begin(); it != sm.end(); ++it) {
            const Foo &f = *it;
            CHECK(f.bookkeep_ == ALIVE);
        }

        // kill every third element
        size_t numKilled = 0;
        ranges::for_each(handles | ranges::views::stride(3), [&](auto handle) {
            sm.release(handle);
            ++numKilled;
        });
        CHECK(
            ranges::all_of(bookkeeper | ranges::views::stride(3), [](int b) { return b == DEAD; }));
        CHECK(sm.size() == bookkeeper.size() - numKilled);

        // check that we only iterate over alive elements
        size_t numIterated = {};
        for (const Foo &f : sm) {
            CHECK(f.bookkeep_ == ALIVE);
            ++numIterated;
        }
        // verify we iterated over as many elements as we should have
        CHECK(numIterated == sm.size());

        // iterating over const objects should work also
        const SlotMap<Foo> &csm{sm};
        for (const Foo &f : csm) {
            CHECK(f.bookkeep_ == ALIVE);
            ++numIterated;
        }

        // failed attempt to debug why SlotMap is not compatible with ranges - looks like
        // for some reason it does not implement the indirectly_readable concept but I have
        // no idea how to proceed..
        //        static_assert(ranges::detail::dereferenceable_<SlotMap<Foo>::iterator&>);
        //
        //        static_assert(ranges::copyable<SlotMap<Foo>::iterator>);
        //        static_assert(ranges::weakly_incrementable<SlotMap<Foo>::iterator>);
        //        static_assert(ranges::input_or_output_iterator<SlotMap<Foo>::iterator>);
        //        static_assert(ranges::indirectly_readable<SlotMap<Foo>::iterator>);
        //        static_assert(ranges::input_iterator<SlotMap<Foo>::iterator>,
        //                      "SlotMap::iterator is not accepted as an iterator");
        //        static_assert(ranges::input_iterator<SlotMap<Foo>::const_iterator>,
        //                      "SlotMap::const_iterator type is not accepted as an iterator");
        //        auto b = ranges::begin(sm);
        //        auto e = ranges::end(sm);
        //
        //        CHECK(ranges::all_of(sm, [](const Foo& f) { return f.bookkeep_ == ALIVE; }));
    }

    SECTION("Iterating over handles and items")
    {
        std::vector<ObjectState> bookkeepers{5, UNINIT};
        SlotMap<Foo> sm;
        std::vector<SlotMapHandle> handles =
            bookkeepers |
            ranges::views::transform([&sm](ObjectState &s) { return sm.emplace(std::ref(s)); }) |
            ranges::to<std::vector<SlotMapHandle>>;

        sm.release(handles[4]);             // this one should not be iterated over anymore
        handles[2] = sm.update(handles[2]); // this one should be iterated over with the new version

        // iterating over the handles
        gsl::index i{};
        for (SlotMapHandle h : sm.handles()) {
            REQUIRE(h.alive());
            CHECK(h == handles[i]);
            i++;
        }
        CHECK(i == 4);

        // iterating over the items (pairs of handle and value)
        const SlotMap<Foo> &csm = sm;
        gsl::index j{};
        for (auto &[h, v] : csm.items()) {
            REQUIRE(h.alive());
            CHECK(h == handles[j]);
            CHECK(&v.bookkeep_ == &bookkeepers[j]);
            j++;
        }
        CHECK(j == 4);

        auto handles2 = sm.handles() | ranges::to<std::vector<SlotMapHandle>>;
    }

    SECTION("ResolvableHandle<Foo>")
    {
        SlotMap<Foo> sm;
        ObjectState book{UNINIT};
        auto h = sm.emplace(std::ref(book));
        ResolvableHandle rh{sm, h};
        CHECK(rh->bookkeep_ == ALIVE);

        const ResolvableHandle crh{rh};
        CHECK(crh->bookkeep_ == ALIVE);
    }
}

TEST_CASE("bla")
{
    std::vector<int> test{1, 2, 3, 4, 2, 5, 6, 1};
    std::ranges::sort(test, {}, [](int v) { return v % 2; });

    for (const auto &v : test) {
        CO_APP_INFO("element: {}", v);
    }
}