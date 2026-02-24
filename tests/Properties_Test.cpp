#include <Cory/Proper/Properties.hpp>
#include <Cory/Proper/PropertySet.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Properties can be set and retrieved", "[Properties]")
{
    using namespace Cory::Prop;

    PropertySet props;

    auto intProp = props.create("myInt", 42);
    auto floatProp = props.create("myFloat", 3.14f);
    auto vec3Prop = props.create("myVec3", glm::vec3{1.0f, 2.0f, 3.0f});

    auto iv = props[intProp]();
    CHECK(std::get<int>(iv) == 42);

    auto fv = props[floatProp]();
    CHECK(std::get<float>(fv) == 3.14f);

    auto f3v = props[vec3Prop]();
    CHECK(std::get<glm::vec3>(f3v) == glm::vec3{1.0f, 2.0f, 3.0f});

    // Can also write through the proxy
    props[intProp] = 100;

    iv = props[intProp]();
    CHECK(std::get<int>(iv) == 100);
}

TEST_CASE("Properties can be co_awaited on", "[Properties]")
{
    using namespace Cory::Prop;

    PropertySet props;

    auto intProp = props.create("myInt", 42);

    int wasSet = -1;
    auto waiter = [&]() -> Proper::Task<> { wasSet = co_await props[intProp].changed<int>(); }();

    CHECK(wasSet == -1);
    props[intProp] = 100;
    CHECK(wasSet == 100);

    auto newWaiter = [&]() -> Proper::Task<> { wasSet = co_await props[intProp].changed<int>(); }();
    newWaiter.cancel();
    props[intProp] = 200;
    CHECK(wasSet == 100); // should not have changed since the waiter was cancelled
}