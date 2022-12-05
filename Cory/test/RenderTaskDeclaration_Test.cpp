#include <Cory/Framegraph/RenderTaskDeclaration.hpp>

#include "TestUtils.hpp"

#include <cppcoro/coroutine.hpp>

#include <catch2/catch_test_macros.hpp>

namespace FG = Cory::Framegraph;

struct TheMightyScheduler {
    auto theSign()
    {
        struct SignAwaiter {
            TheMightyScheduler &ol;
            [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }
            [[nodiscard]] int await_resume() const noexcept { return ol.sign; }
            void await_suspend(cppcoro::coroutine_handle<> coroHandle) const noexcept
            {
                ol.coro = coroHandle;
            }
        };
        return SignAwaiter{*this};
    }
    void resumeCoro() { coro.resume(); }
    ~TheMightyScheduler() { coro.destroy(); }
    cppcoro::coroutine_handle<> coro;
    int sign;
};

struct TestOutput {
    int foo;
};

enum class CoroState { NotStarted, BeforeYield, BeforeAwait, AfterAwait };

FG::RenderTaskDeclaration<TestOutput>
testCoro(TheMightyScheduler &ol, CoroState &coroState, int &coroValue)
{
    coroState = CoroState::BeforeYield;
    co_yield TestOutput{321};
    coroState = CoroState::BeforeAwait;
    coroValue = co_await ol.theSign();
    coroState = CoroState::AfterAwait;
}

TEST_CASE("Regular pingpong between coroutine and scheduler")
{
    TheMightyScheduler scheduler{.sign = 666};
    int coroValue{0};

    CoroState coroState{CoroState::NotStarted};
    auto coroObject = testCoro(scheduler, coroState, coroValue);
    // coroutine should start only when output is queried
    CHECK(coroState == CoroState::NotStarted);
    CHECK(coroValue == 0);

    // when the output is queried, it runs through the first co_yield and waits until the co_yield
    CHECK(coroObject.output().foo == 321);
    CHECK(coroValue == 0);
    CHECK(coroState == CoroState::BeforeAwait);

    // when the overlord resumes the coroutine, it should receive the sign value
    scheduler.resumeCoro();
    CHECK(coroState == CoroState::AfterAwait);
    CHECK(coroValue == 666);
}

FG::RenderTaskDeclaration<TestOutput> errorCoro(bool throwBeforeYield)
{
    if (throwBeforeYield) { throw std::runtime_error{"Ohno :("}; }

    co_yield TestOutput{123};

    throw std::logic_error{">:("};
}

TEST_CASE("Exceptions in a render pass")
{
    SECTION("Coroutine throws before yielding a value")
    {
        auto coro = errorCoro(true);
        CHECK_THROWS_AS(coro.output(), std::runtime_error);
    }
    SECTION("Coroutine throws after yielding a value")
    {
        auto coro = errorCoro(false);
        CHECK_THROWS_AS(coro.output(), std::logic_error);
    }
}
