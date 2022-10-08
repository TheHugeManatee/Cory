#include <catch2/catch.hpp>

#include <Cory/Base/Callback.hpp>

using namespace Cory;

TEST_CASE("Callbacks with no arguments", "[Cory/Base/Callback]")
{
    GIVEN("A callback")
    {
        Callback<void> onCallbackCalled;
        WHEN("No callback function is registered and the callback is invoked")
        {
            THEN("Nothing happens") { CHECK_NOTHROW(onCallbackCalled.invoke()); }
        }
        WHEN("A callback function is registered and the callback is invoked")
        {
            int state{0};
            CHECK_NOTHROW(onCallbackCalled([&state]() { state++; }));
            onCallbackCalled.invoke();

            THEN("The registered callback function is called") { CHECK(state == 1); }
            AND_WHEN("A different callback is called and the function is invoked")
            {
                onCallbackCalled([&state]() { state = 42; });
                THEN("The new callback is called")
                {
                    onCallbackCalled.invoke();

                    CHECK(state == 42);
                }
            }
        }
    }
}

TEST_CASE("Callbacks with multiple arguments", "[Cory/Base/Callback]")
{
    GIVEN("A callback")
    {
        Callback<int> onCallbackCalled;
        WHEN("No callback function is registered and the callback is invoked")
        {
            THEN("Nothing happens") { CHECK_NOTHROW(onCallbackCalled.invoke(1)); }
        }
        WHEN("A callback function is registered and the callback is invoked")
        {
            int state{0};
            CHECK_NOTHROW(onCallbackCalled([&state](int val) { state = val; }));
            onCallbackCalled.invoke(1);

            THEN("The registered callback function is called") { CHECK(state == 1); }
            AND_WHEN("A different callback is called and the function is invoked")
            {
                onCallbackCalled([&state](int val) { state = val + 1; });
                onCallbackCalled.invoke(123);

                THEN("The new callback is called") { CHECK(state == 124); }
            }
            AND_WHEN("The callback function is cleared and the callback is invoked")
            {
                onCallbackCalled.reset();
                onCallbackCalled.invoke(1);

                THEN("No callback is called") { CHECK(state == 1); }
            }
        }
    }
}