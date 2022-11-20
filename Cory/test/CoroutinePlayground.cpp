#include <catch2/catch_test_macros.hpp>

#include <cppcoro/coroutine.hpp>
#include <cppcoro/is_awaitable.hpp>
#include <cppcoro/task.hpp>
#include <fmt/core.h>

#include <string>
#include <variant>

class MultiStepProcess {
  public:
    struct promise_type {
        cppcoro::suspend_always initial_suspend() noexcept { return {}; }

        MultiStepProcess get_return_object() { return MultiStepProcess{this}; }

        cppcoro::suspend_always final_suspend() noexcept { return {}; }
        void unhandled_exception() { result = std::current_exception(); }

        cppcoro::suspend_always yield_value(int v) noexcept
        {
            if (!std::holds_alternative<std::monostate>(result)) {
                result = std::make_exception_ptr(
                    std::logic_error{"Coroutine yielded values out of order!"});
            }
            else {
                result = v;
            }
            return {};
        }
        cppcoro::suspend_always yield_value(std::string v)
        {
            if (!std::holds_alternative<int>(result)) {
                result = std::make_exception_ptr(
                    std::logic_error{"Coroutine yielded values out of order!"});
            }
            else {
                result = v;
            }
            return {};
        }
        void return_void() {}

        // support co_await'ing for a std::string{} as well as a general awaitable
        template <typename T> decltype(auto) await_transform(T &&t)
        {
            if constexpr (std::is_same_v<T, std::string>) {
                struct awaiter {
                    promise_type &pt;
                    constexpr bool await_ready() const noexcept { return true; }
                    std::string await_resume() const noexcept { return std::move(pt.stepTwoInput); }
                    void await_suspend(cppcoro::coroutine_handle<promise_type>) const noexcept {}
                };
                return awaiter{*this};
            }
            else if constexpr (cppcoro::is_awaitable_v<T>) {
                return t.operator co_await();
            }
        }

        std::variant<std::monostate, int, std::string, std::exception_ptr> result;
        std::string stepTwoInput;
    };

    using Handle = cppcoro::coroutine_handle<promise_type>;

    explicit MultiStepProcess(promise_type *p)
        : coroHandle_{Handle::from_promise(*p)}
    {
    }
    // move-only!
    MultiStepProcess(MultiStepProcess &&rhs)
        : coroHandle_{std::exchange(rhs.coroHandle_, nullptr)}
    {
    }
    ~MultiStepProcess()
    {
        if (coroHandle_) { coroHandle_.destroy(); }
    }

    int doStep1()
    {
        if (!coroHandle_.done()) { coroHandle_.resume(); }
        auto &result = coroHandle_.promise().result;
        if (std::holds_alternative<std::exception_ptr>(result)) {
            std::rethrow_exception(std::get<std::exception_ptr>(result));
        }
        return std::get<int>(result);
    }

    std::string doStep2(std::string stepTwoInput)
    {
        coroHandle_.promise().stepTwoInput = stepTwoInput;
        if (!coroHandle_.done()) { coroHandle_.resume(); }
        auto &result = coroHandle_.promise().result;
        if (std::holds_alternative<std::exception_ptr>(result)) {
            std::rethrow_exception(std::get<std::exception_ptr>(result));
        }
        return std::move(std::get<std::string>(result));
    }

  private:
    Handle coroHandle_;
};

MultiStepProcess asyncJobAlpha()
{
    co_yield 1;
    auto input = co_await std::string{};
    co_yield "hello " + input;
}

TEST_CASE("Coroutines yielding different values", "[Cory/Coroutine/Playground]")
{
    auto job = asyncJobAlpha();

    auto result_1 = job.doStep1();
    CHECK(result_1 == 1);

    auto result_2 = job.doStep2("world");
    CHECK(result_2 == "hello world");
}

MultiStepProcess asyncJobBeta()
{
    co_yield 2;
    co_yield 2;
    co_yield "World";
}

MultiStepProcess asyncJobGamma()
{
    co_yield "World";
    co_yield 2;
}

TEST_CASE("Usage contract violations", "[Cory/Coroutine/Playground]")
{
    SECTION("Coroutine attempts to yield step1 result multiple times")
    {
        auto job = asyncJobBeta();
        auto result_1 = job.doStep1();
        REQUIRE(result_1 == 2);
        CHECK_THROWS(job.doStep2(""));
    }
    SECTION("Coroutine attempts to yield step2 before step1")
    {
        auto job = asyncJobGamma();
        CHECK_THROWS(job.doStep1());
    }
    SECTION("Caller not adhering to contract")
    {
        auto job = asyncJobAlpha();
        CHECK_THROWS(job.doStep2(""));
    }
}

MultiStepProcess asyncJobDelta(cppcoro::task<int> inputTask)
{
    int val = co_await inputTask;
    co_yield val;
    auto input = co_await std::string{};
    co_yield "delta " + input;
}

cppcoro::task<int> nestedJob(int v) { co_return v; }

TEST_CASE("Interop with cppcoro tasks", "[Cory/Coroutine/Playground]")
{
    auto job = asyncJobDelta(nestedJob(42));

    auto result_1 = job.doStep1();
    CHECK(result_1 == 42);

    auto result_2 = job.doStep2("world");
    CHECK(result_2 == "delta world");
}
