#include <catch2/catch_test_macros.hpp>

#include <Cory/Base/Coro.hpp>
#include <Cory/Base/CoroBatch.hpp>
#include <Cory/Base/CoroOps.hpp>
#include <Cory/Base/CoroThreadPool.hpp>

#include <cppcoro/schedule_on.hpp>
#include <cppcoro/static_thread_pool.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all_ready.hpp>

#include <atomic>
#include <expected>
#include <stop_token>
#include <string>
#include <vector>

namespace {

cppcoro::task<std::expected<void, std::string>>
runWorkerTask(std::vector<std::atomic_uint32_t> &hits, size_t taskIndex)
{
    hits[taskIndex].fetch_add(1u, std::memory_order_relaxed);
    co_return std::expected<void, std::string>{};
}

cppcoro::task<std::expected<void, std::string>>
runImmediateTask(std::vector<std::atomic_uint32_t> &hits, size_t taskIndex)
{
    hits[taskIndex].fetch_add(1u, std::memory_order_relaxed);
    co_return std::expected<void, std::string>{};
}

cppcoro::task<std::expected<void, std::string>>
runFanOutIteration(cppcoro::static_thread_pool &pool, size_t taskCount)
{
    auto hits = std::vector<std::atomic_uint32_t>(taskCount);
    for (auto &hit : hits) {
        hit.store(0u, std::memory_order_relaxed);
    }

    auto tasks = std::vector<cppcoro::task<std::expected<void, std::string>>>{};
    tasks.reserve(taskCount);
    for (size_t i = 0; i < taskCount; ++i) {
        tasks.emplace_back(cppcoro::schedule_on(pool, runWorkerTask(hits, i)));
    }

    auto completed = co_await cppcoro::when_all_ready(std::move(tasks));
    for (auto &task : completed) {
        auto result = task.result();
        if (!result) {
            co_return std::unexpected(std::move(result.error()));
        }
    }

    for (size_t i = 0; i < taskCount; ++i) {
        if (hits[i].load(std::memory_order_relaxed) != 1u) {
            co_return std::unexpected("unexpected worker hit count");
        }
    }

    co_return std::expected<void, std::string>{};
}

cppcoro::task<std::expected<void, std::string>> runFanOutIterationCory(Cory::CoroThreadPool &pool,
                                                                       size_t taskCount)
{
    auto hits = std::vector<std::atomic_uint32_t>(taskCount);
    for (auto &hit : hits) {
        hit.store(0u, std::memory_order_relaxed);
    }

    auto tasks = std::vector<cppcoro::task<Cory::Result<void>>>{};
    tasks.reserve(taskCount);
    for (size_t i = 0; i < taskCount; ++i) {
        tasks.emplace_back(Cory::resume_on(pool, runWorkerTask(hits, i)));
    }

    auto stopSource = std::stop_source{};
    auto completed = co_await Cory::when_all_fail_fast(std::move(tasks), stopSource);
    if (!completed) {
        co_return std::unexpected(std::move(completed.error()));
    }

    for (size_t i = 0; i < taskCount; ++i) {
        if (hits[i].load(std::memory_order_relaxed) != 1u) {
            co_return std::unexpected("unexpected worker hit count");
        }
    }

    co_return std::expected<void, std::string>{};
}

} // namespace

TEST_CASE("cppcoro fan-out gather baseline using schedule_on and when_all_ready", "[Cppcoro]")
{
    SKIP("Cppcoro MREs are skipped as this triggers TSAN");
    auto pool = cppcoro::static_thread_pool{4};

    constexpr auto iterations = size_t{64};
    constexpr auto taskCount = size_t{256};
    for (size_t iteration = 0; iteration < iterations; ++iteration) {
        auto result = Cory::sync_wait(runFanOutIteration(pool, taskCount));
        REQUIRE(result);
    }
}

TEST_CASE("cppcoro fan-out gather without scheduler baseline", "[Cppcoro]")
{
    constexpr auto taskCount = size_t{1024};
    auto hits = std::vector<std::atomic_uint32_t>(taskCount);
    for (auto &hit : hits) {
        hit.store(0u, std::memory_order_relaxed);
    }

    auto tasks = std::vector<cppcoro::task<std::expected<void, std::string>>>{};
    tasks.reserve(taskCount);
    for (size_t i = 0; i < taskCount; ++i) {
        tasks.emplace_back(runImmediateTask(hits, i));
    }

    auto run = [&]() -> cppcoro::task<std::expected<void, std::string>> {
        auto completed = co_await cppcoro::when_all_ready(std::move(tasks));
        for (auto &task : completed) {
            auto result = task.result();
            if (!result) {
                co_return std::unexpected(std::move(result.error()));
            }
        }
        co_return std::expected<void, std::string>{};
    };

    auto result = Cory::sync_wait(run());
    REQUIRE(result);
    for (size_t i = 0; i < taskCount; ++i) {
        REQUIRE(hits[i].load(std::memory_order_relaxed) == 1u);
    }
}

// This test is skipped intentionally - it was a way MRE to verify the issue lies in cppcoro
TEST_CASE("cppcoro single schedule_on with sync_wait baseline", "[Cppcoro]")
{
    auto pool = cppcoro::static_thread_pool{2};

    auto ran = std::atomic_bool{false};
    auto run = [&]() -> cppcoro::task<std::expected<void, std::string>> {
        co_await cppcoro::schedule_on(pool,
                                      [&]() -> cppcoro::task<std::expected<void, std::string>> {
                                          ran.store(true, std::memory_order_relaxed);
                                          co_return std::expected<void, std::string>{};
                                      }());
        co_return std::expected<void, std::string>{};
    };

    auto result = Cory::sync_wait(run());
    REQUIRE(result);
    REQUIRE(ran.load(std::memory_order_relaxed));
}

TEST_CASE("Cory pool single resume_on with sync_wait baseline", "[Cppcoro]")
{
    auto pool = Cory::CoroThreadPool{2};

    auto ran = std::atomic_bool{false};
    auto run = [&]() -> cppcoro::task<std::expected<void, std::string>> {
        auto result = co_await Cory::resume_on(pool, [&]() -> cppcoro::task<Cory::Result<void>> {
            ran.store(true, std::memory_order_relaxed);
            co_return Cory::Result<void>{};
        }());

        if (!result) {
            co_return std::unexpected(std::move(result.error()));
        }
        co_return std::expected<void, std::string>{};
    };

    auto result = Cory::sync_wait(run());
    REQUIRE(result);
    REQUIRE(ran.load(std::memory_order_relaxed));
}

TEST_CASE("Cory pool fan-out gather baseline", "[Cppcoro]")
{
    auto pool = Cory::CoroThreadPool{4};

    constexpr auto iterations = size_t{64};
    constexpr auto taskCount = size_t{256};
    for (size_t iteration = 0; iteration < iterations; ++iteration) {
        auto result = Cory::sync_wait(runFanOutIterationCory(pool, taskCount));
        REQUIRE(result);
    }
}
