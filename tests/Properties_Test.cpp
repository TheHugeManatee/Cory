#include <Cory/Proper/Properties.hpp>
#include <Cory/Proper/Property.hpp>
#include <Cory/Proper/PropertySet.hpp>
#include <Cory/Proper/Parameter.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Parameters")
{
    using namespace Cory;

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

    SECTION("Numeric parameters enforce inclusive bounds")
    {
        NumericParameter<int> parameter("intRange", 5, 1, 10);

        CHECK(parameter.name() == "intRange");
        CHECK(parameter.get() == 5);
        REQUIRE(parameter.min().has_value());
        REQUIRE(parameter.max().has_value());
        CHECK(parameter.min().value() == 1);
        CHECK(parameter.max().value() == 10);
        CHECK(parameter.hasMin());
        CHECK(parameter.hasMax());

        CHECK((parameter = 10));
        CHECK(parameter.get() == 10);

        CHECK_FALSE(parameter.set(0));
        CHECK(parameter.get() == 10);
        CHECK_FALSE((parameter = 11));
        CHECK(parameter.get() == 10);
    }

    SECTION("Numeric parameters can omit explicit bounds")
    {
        NumericParameter<float> parameter("unbounded", 5.0f);

        CHECK(parameter.name() == "unbounded");
        CHECK(parameter.get() == 5.0f);
        CHECK_FALSE(parameter.hasMin());
        CHECK_FALSE(parameter.hasMax());

        CHECK(parameter.set(123.0f));
        CHECK(parameter.get() == 123.0f);
    }

    SECTION("Numeric parameters support glm component-wise bounds")
    {
        NumericParameter<glm::vec3> parameter("position",
                                              glm::vec3{0.0f, 1.0f, 2.0f},
                                              glm::vec3{-1.0f, 0.0f, 1.0f},
                                              glm::vec3{1.0f, 2.0f, 3.0f});

        CHECK(parameter.name() == "position");
        CHECK(parameter.get() == glm::vec3{0.0f, 1.0f, 2.0f});
        CHECK(parameter.hasMin());
        CHECK(parameter.hasMax());

        CHECK(parameter.set(glm::vec3{1.0f, 2.0f, 3.0f}));
        CHECK(parameter.get() == glm::vec3{1.0f, 2.0f, 3.0f});

        CHECK_FALSE(parameter.set(glm::vec3{2.0f, 2.0f, 3.0f}));
        CHECK_FALSE(parameter.set(glm::vec3{1.0f, -1.0f, 3.0f}));
        CHECK(parameter.get() == glm::vec3{1.0f, 2.0f, 3.0f});
    }

    SECTION("Option parameters expose and enforce their accepted values")
    {
        OptionParameter<int, 1, 3, 5> parameter("odd", 3);

        CHECK(parameter.name() == "odd");
        CHECK(parameter.get() == 3);
        CHECK(parameter.acceptedValues() == std::array{1, 3, 5});

        CHECK(parameter.set(5));
        CHECK(parameter.get() == 5);

        CHECK_FALSE(parameter.set(2));
        CHECK(parameter.get() == 5);
    }

    SECTION("Enum parameters derive accepted values from magic_enum")
    {
        EnumParameter<Quality> parameter("quality", Quality::Medium);

        CHECK(parameter.name() == "quality");
        CHECK(parameter.get() == Quality::Medium);
        CHECK(parameter.acceptedValues() == std::array{Quality::Low, Quality::Medium, Quality::High});

        CHECK((parameter = Quality::High));
        CHECK(parameter.get() == Quality::High);
    }

    SECTION("String option parameters expose and enforce accepted values")
    {
        StringOptionParameter parameter("backend", "vulkan", {"vulkan", "opengl", "metal"});

        CHECK(parameter.name() == "backend");
        CHECK(parameter.get() == "vulkan");
        CHECK(std::ranges::equal(parameter.acceptedValues(),
                                 std::array<std::string_view, 3>{"vulkan", "opengl", "metal"}));

        CHECK(parameter.set("metal"));
        CHECK(parameter.get() == "metal");

        CHECK_FALSE(parameter.set("directx"));
        CHECK(parameter.get() == "metal");
    }
}

TEST_CASE("Properties can be set and retrieved", "[Properties]")
{
    using namespace Cory;

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
    using namespace Cory;

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

TEST_CASE("Perf test to get some ideas", "[Properties][.]")
{
    using namespace Cory;

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
    using namespace Cory;
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
