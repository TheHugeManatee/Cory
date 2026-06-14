#pragma once

#include <Cory/Base/Common.hpp>

#include <cppcoro/coroutine.hpp>

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace Cory {

/// A simple thread pool scheduler for coroutines. Coroutines can co_await the schedule() function
/// to yield execution to another worker thread.
/// Implements the Scheduler concept.
class CoroThreadPool : NoCopy {
  public:
    explicit CoroThreadPool(size_t workerCount = std::thread::hardware_concurrency());
    ~CoroThreadPool();

    CoroThreadPool(CoroThreadPool &&other) = delete;
    CoroThreadPool &operator=(CoroThreadPool &&other) = delete;

    struct ScheduleAwaiter {
        CoroThreadPool *pool{nullptr};

        bool await_ready() const noexcept { return false; }
        void await_suspend(cppcoro::coroutine_handle<> handle) const noexcept;
        void await_resume() const noexcept {}
    };

    [[nodiscard]] ScheduleAwaiter schedule() noexcept { return ScheduleAwaiter{this}; }

  private:
    void enqueue(cppcoro::coroutine_handle<> handle) noexcept;
    void workerLoop();

    std::mutex queueMutex_{};
    std::condition_variable queueCv_{};
    std::deque<cppcoro::coroutine_handle<>> queue_{};
    bool stopRequested_{false};
    std::vector<std::thread> workers_{};
};

} // namespace Cory
