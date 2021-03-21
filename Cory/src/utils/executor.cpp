#include "utils/executor.h"

#include <algorithm>
#include <numeric>

#include "Cory/Log.h"

#if WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <processthreadsapi.h>
#endif

namespace cory::utils {

void executor::executor_main()
{
    set_thread_name();

    std::unique_lock lck{queue_mtx_};
    while (!stop_thread_) {
        drain_queue(lck);
        // wait until either stop is requested or queue has new entries
        queue_condition_.wait(lck, [&]() { return stop_thread_ || !task_queue_.empty(); });
    }

    // when stopping, drain the queue to make sure all tasks have been completed
    drain_queue(lck);
}

void executor::set_thread_name()
{
#if WIN32
    size_t convertedChars = 0;
    const size_t newsize = (name_.size() + 1) * 2;
    std::wstring wname;
    wname.resize(newsize);
    mbstowcs_s(&convertedChars, wname.data(), name_.size() + 1, name_.c_str(), _TRUNCATE);

    SetThreadDescription(GetCurrentThread(), wname.c_str());
#endif
}

void executor::drain_queue(std::unique_lock<std::mutex> &lck)
{
    while (!task_queue_.empty()) {
        // dequeue a task from the queue
        // move the task out of the queue and pop it
        std::packaged_task<void()> task{std::move(task_queue_.front())};
        task_queue_.pop();
        {
            lck.unlock();
            try {
                task();
            }
            catch (...) {
                // ignore all exceptions to ensure the lock is re-acquired
            }
            lck.lock();
        }
    }
}

void executor::flush()
{
    CO_CORE_ASSERT(std::this_thread::get_id() != worker_thread_.get_id(),
                   "cannot flush from the executor worker thread!");
    // enqueue an empty marker job and wait for it to be executed
    async([]() {}).get();
}

} // namespace cory::utils

#ifndef DOCTEST_CONFIG_DISABLE

#include <doctest/doctest.h>

TEST_SUITE_BEGIN("executor");

SCENARIO("basic executor usage")
{
    GIVEN("an executor")
    {
        cory::utils::executor executor("test executor");
        THEN("it should be possible to query the name")
        {
            REQUIRE(executor.name() == "test executor");
        }

        WHEN("scheduling a task that returns a value")
        {
            //             auto result = executor.async([]() { return 42; });
            //
            //             THEN("the result should be available") {
            //                 auto result_value = result.get();
            //                 REQUIRE(result_value == 42);
            //             }
        }
        WHEN("scheduling a task that does not return a value")
        {
            std::atomic_bool task_was_executed{false};
            auto result = executor.async([&]() {
                std::this_thread::sleep_for(std::chrono::milliseconds{100});
                task_was_executed = true;
            });

            THEN("the result should be available") { CHECK_NOTHROW(result.get()); }
            THEN("the task should have been executed")
            {
                result.get();
                CHECK(task_was_executed == true);
            }
        }
        WHEN("scheduling several tasks")
        {
            constexpr int num_tasks{10};
            std::vector<std::thread::id> thread_ids(num_tasks);
            std::vector<int> task_proof;
            std::vector<cory::future<void>> results;

            for (int i = 0; i < num_tasks; ++i) {
                results.push_back(executor.async([&, task_idx = i]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds{25});
                    thread_ids[task_idx] = std::this_thread::get_id();
                    task_proof.push_back(task_idx);
                }));
            }

            THEN("all tasks are executed")
            {
                // wait for all tasks
                for (auto &r : results) {
                    r.get();
                }
                CHECK(task_proof.size() == num_tasks);
            }
            THEN("all tasks are executed on the same thread")
            {
                // wait for all tasks
                for (auto &r : results) {
                    r.get();
                }
                for (const auto &tid : thread_ids) {
                    CHECK(thread_ids[0] == tid);
                }
            }
            THEN("tasks were executed in order")
            {
                // wait for all tasks
                for (auto &r : results) {
                    r.get();
                }

                std::vector<int> id_diff(num_tasks);
                std::adjacent_difference(task_proof.begin(), task_proof.end(), id_diff.begin());
                for (int tidx = 1; tidx < num_tasks; ++tidx) {
                    CHECK(id_diff[tidx] == 1);
                }
            }
            AND_WHEN("flushing the executor")
            {
                executor.flush();
                THEN("all tasks should be finished") { CHECK(task_proof.size() == num_tasks); }
            }
        }
        WHEN("scheduling a task from another task")
        {
            int task1_executed{};
            int task2_executed{};

            auto task1_future = executor.async([&]() {
                task1_executed = 1;
                executor.async([&]() { task2_executed = 1; });
            });

            THEN("both tasks should execute without a deadlock")
            {
                executor.flush();
                CHECK(task1_executed);
                CHECK(task2_executed);
            }
        }
    }
}

SCENARIO("executor shutdown")
{
    GIVEN("an executor")
    {
        WHEN("scheduling many tasks")
        {
            constexpr int num_tasks{10};
            std::vector<int> task_proof;

            {
                cory::utils::executor executor("out-of-scope test executor");

                for (int i = 0; i < num_tasks; ++i) {
                    executor.async([&, task_idx = i]() {
                        std::this_thread::sleep_for(std::chrono::milliseconds{25});
                        task_proof.push_back(task_idx);
                    });
                }
            }
            AND_WHEN("the executor goes out of scope")
            {
                THEN("all tasks should be executed") { CHECK(task_proof.size() == num_tasks); }
            }
        }
    }
}

SCENARIO("multithreaded usage")
{
    GIVEN("an executor")
    {
        cory::utils::executor executor("multithread test executor");
        WHEN("scheduling tasks from multiple threads")
        {
            constexpr int tasks_per_thread{50};
            constexpr int num_threads{4};

            std::mutex data_mutex;
            std::vector<std::pair<int, int>> results;

            std::vector<std::thread> scheduling_threads;
            for (int tidx = 0; tidx < num_threads; ++tidx) {
                scheduling_threads.emplace_back([&]() {
                    for (int i = 0; i < tasks_per_thread; ++i) {
                        executor.async([=, &data_mutex, &results]() {
                            std::unique_lock lck{data_mutex};
                            results.emplace_back(tidx, i);
                        });
                    }
                });
            }

            // wait for the scheduling threads to finish
            for (auto &t : scheduling_threads) {
                if (t.joinable()) t.join();
            }

            THEN("all tasks should be executed")
            {
                executor.flush();
                CHECK(results.size() == tasks_per_thread * num_threads);
            }
        }
    }
}

SCENARIO("exceptions")
{
    GIVEN("an executor")
    {
        cory::utils::executor executor("exception test executor");

        WHEN("scheduling a task that throws an exception")
        {
            {
                auto exceptional_result =
                    executor.async([]() { throw std::runtime_error{"on no!"}; });

                THEN("the exception should be propagated through the task and the future")
                {
                    CHECK_THROWS_AS(exceptional_result.get(), std::runtime_error);
                }
                AND_WHEN("scheduling another task")
                {
                    std::atomic_bool task2_executed{};
                    executor.async([&]() { task2_executed = true; });
                    THEN("the succeeding task should be executed normally")
                    {
                        executor.flush();
                        CHECK(task2_executed == true);
                    }
                }
                AND_WHEN("the exceptional result is not explicitly handled through get()")
                {
                    THEN("the exception is silently ignored") { 
                        CHECK_NOTHROW(exceptional_result = {}); 
                    }
                }
            }
        }
    }
}

TEST_SUITE_END();

#endif