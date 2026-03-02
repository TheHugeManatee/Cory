#include <Cory/Proper/Properties.hpp>
#include <Cory/Proper/Property.hpp>
#include <Cory/Proper/PropertySet.hpp>
#include <Cory/Proper/Parameter.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Parameters")
{
    using namespace Cory::Proper;

    enum class Quality {
        Low,
        Medium,
        High,
    };

    SECTION("Plain parameters store a name")
    {
        Parameter<int> parameter("intParam", 5);

        CHECK(parameter.name() == "intParam");
        CHECK(parameter.get() == 5);
    }

    SECTION("Ranged parameters enforce inclusive bounds")
    {
        RangedParameter<int, 1, 10> parameter("intRange", 5);

        CHECK(parameter.name() == "intRange");
        CHECK(parameter.get() == 5);
        CHECK(parameter.min() == 1);
        CHECK(parameter.max() == 10);

        parameter = 10;
        CHECK(parameter.get() == 10);

        CHECK_THROWS_AS(parameter.set(0), std::out_of_range);
        CHECK_THROWS_AS(parameter = 11, std::out_of_range);
    }

    SECTION("Option parameters expose and enforce their accepted values")
    {
        OptionParameter<int, 1, 3, 5> parameter("odd", 3);

        CHECK(parameter.name() == "odd");
        CHECK(parameter.get() == 3);
        CHECK(parameter.acceptedValues() == std::array{1, 3, 5});

        parameter.set(5);
        CHECK(parameter.get() == 5);

        CHECK_THROWS_AS(parameter.set(2), std::invalid_argument);
    }

    SECTION("Enum parameters derive accepted values from magic_enum")
    {
        EnumParameter<Quality> parameter("quality", Quality::Medium);

        CHECK(parameter.name() == "quality");
        CHECK(parameter.get() == Quality::Medium);
        CHECK(parameter.acceptedValues() == std::array{Quality::Low, Quality::Medium, Quality::High});

        parameter = Quality::High;
        CHECK(parameter.get() == Quality::High);
    }
}

TEST_CASE("Properties can be set and retrieved", "[Properties]")
{
    using namespace Cory::Proper;

    Property intProp(42);
    Property floatProp(3.14f);
    Property vec3Prop(glm::vec3{1.0f, 2.0f, 3.0f});

    CHECK(intProp.get() == 42);
    CHECK(floatProp.get() == 3.14f);
    CHECK(vec3Prop.get() == glm::vec3{1.0f, 2.0f, 3.0f});

    intProp.set(100);
    floatProp.set(6.28f);
    vec3Prop.set(glm::vec3{4.0f, 5.0f, 6.0f});

    CHECK(intProp.get() == 100);
    CHECK(floatProp.get() == 6.28f);
    CHECK(vec3Prop.get() == glm::vec3{4.0f, 5.0f, 6.0f});
}

TEST_CASE("Properties can be co_awaited on safely, and pending waiters are cleaned up",
          "[Properties]")
{
    using namespace Cory::Proper;

    auto waitForChanges = [](Property<int> &property, int &target, bool &cleanedUp) -> Task<> {
        struct CleanupChecker {
            bool *wasCleanedUp;
            ~CleanupChecker() { *wasCleanedUp = true; }
        };

        CleanupChecker cleanupChecker{&cleanedUp};
        while (true) {
            target = co_await property.changed();
        }
    };

    auto waitForSingleChange = [](Property<int> &property, int &target) -> Task<> {
        target = co_await property.changed();
    };

    Property intProp(32);
    bool waiter1CleanedUp = false;
    bool waiter2CleanedUp = false;

    {
        int setFromWaiter1 = -1;
        auto waiter1 = waitForChanges(intProp, setFromWaiter1, waiter1CleanedUp);

        SECTION("Changing property value wakes waiters")
        {
            CHECK(setFromWaiter1 == -1);
            intProp = 100;
            CHECK(setFromWaiter1 == 100);
        }
        SECTION("Multiple changes wake waiters multiple times")
        {
            CHECK(setFromWaiter1 == -1);
            intProp = 100;
            intProp = 150;
            CHECK(setFromWaiter1 == 150);
        }
        SECTION("If a waiter is cancelled, it doesn't receive further notifications")
        {
            intProp = 100;
            waiter1.cancel();
            intProp = 200;
            CHECK(setFromWaiter1 == 100);
        }
        SECTION("If a waiter is goes out of scope, it doesn't receive further notifications")
        {
            intProp = 100;
            int setFromWaiter3 = -1;
            {
                auto waiter3 = waitForSingleChange(intProp, setFromWaiter3);
            }
            intProp = 200;
            CHECK(setFromWaiter3 == -1);
        }

        auto setFromWaiter2 = -1;
        auto waiter2 = waitForChanges(intProp, setFromWaiter2, waiter2CleanedUp);
        SECTION("Multiple waiters are all woken")
        {
            intProp = 150;
            CHECK(setFromWaiter1 == 150);
            CHECK(setFromWaiter2 == 150);
        }
        SECTION("Cancelling one waiter doesn't affect the other")
        {
            intProp = 150;
            waiter2.cancel();
            intProp = 200;
            CHECK(setFromWaiter1 == 200);
            CHECK(setFromWaiter2 == 150);
        }
    }
    CHECK(waiter1CleanedUp);
    CHECK(waiter2CleanedUp);
}

TEST_CASE("Perf test to get some ideas", "[Properties]")
{
    using namespace Cory::Proper;

    auto waitForever = [](Property<int> &property) -> Task<> {
        while (true) {
            co_await property.changed();
        }
    };

    Property<int> prop(0);

    constexpr size_t numWaiters = 30'000;
    std::vector<Task<>> waiters;
    waiters.reserve(numWaiters);
    std::generate_n(std::back_inserter(waiters), numWaiters, [&]() { return waitForever(prop); });

    auto start = std::chrono::high_resolution_clock::now();
    constexpr int iterations = 50'000;
    for (int i = 0; i < iterations; ++i) {
        prop.set(i);
        CHECK(prop.get() == i);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    WARN("Time taken for " << iterations << " iterations with " << numWaiters
                           << " waiters: " << duration << " ms");
    WARN("Time per iteration (ns): " << (duration * 1'000'000.0 / iterations));
}

TEST_CASE("Pingpong")
{
    using namespace Cory::Proper;
    spdlog::warn("Starting pingpong");

    auto setTo1 = [](Property<int> &property) -> Task<> {
        while (true) {
            spdlog::warn("Waiting for change to set to 1");
            co_await property.changed();
            spdlog::warn("Setting to 1");
            property.set(1);
            spdlog::warn("Setting to 1 done - value: {}", property.get());
        }
    };
    auto setTo2 = [](Property<int> &property) -> Task<> {
        while (true) {
            spdlog::warn("Waiting for change to set to 2");
            co_await property.changed();
            spdlog::warn("Setting to 2");
            property.set(2);
            spdlog::warn("Setting to 2 done - value: {}", property.get());
        }
    };

    Property<int> prop(0);
    std::vector<int> log1;
    auto waiter1 = setTo1(prop);
    auto waiter2 = setTo2(prop);

    spdlog::warn("Setting to 5");
    prop = 5;
    spdlog::warn("Setting to 5 done - value: {}", prop.get());
}
