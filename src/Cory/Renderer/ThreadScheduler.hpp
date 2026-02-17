#pragma once

#include <Cory/Base/Common.hpp>

#include <cppcoro/coroutine.hpp>

#include <deque>
#include <mutex>
#include <thread>

namespace Cory {

class ThreadScheduler : NoCopy, NoMove {
  public:
    ThreadScheduler();

    struct ScheduleAwaiter {
        ThreadScheduler *scheduler{nullptr};
        [[nodiscard]] bool await_ready() const noexcept;
        void await_suspend(cppcoro::coroutine_handle<> handle) const noexcept;
        void await_resume() const noexcept {}
    };

    [[nodiscard]] ScheduleAwaiter schedule() noexcept { return ScheduleAwaiter{this}; }

    [[nodiscard]] bool isCurrentThread() const noexcept;
    void poll();

  private:
    void enqueue(cppcoro::coroutine_handle<> handle) noexcept;
    void assertCurrentThread(char const *methodName) const;

    std::thread::id ownerThreadId_{};
    std::mutex queueMutex_{};
    std::deque<cppcoro::coroutine_handle<>> queue_{};
};

} // namespace Cory
