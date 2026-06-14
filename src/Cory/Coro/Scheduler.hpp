
#pragma once

#include <Cory/Base/Locked.hpp>
#include <Cory/Coro/Coro.hpp>

#include <cppcoro/coroutine.hpp>

#include <concepts>
#include <mutex>
#include <vector>

namespace Cory {

/// Simple scheduler that immediately resumes the coroutine on the same thread - useful for testing
/// and as a fallback when no other scheduler is available
class SyncScheduler {
  public:
    auto schedule() noexcept { return cppcoro::suspend_never{}; }
};

/// A scheduler that enqueues all coroutines to be resumed on the next tick - useful for avoiding
/// deep call stacks and ensuring a consistent execution order.
///
/// schedule() is threadsafe, but coroutines will be resumed on whatever thread calls the `tick()`
/// function.
class NextTickScheduler {
  public:
    /// Schedule a coroutine to be resumed on the next tick. This function is threadsafe.
    auto schedule()
    {
        struct Awaiter {
            NextTickScheduler *scheduler;
            bool await_ready() const noexcept { return false; }
            void await_suspend(cppcoro::coroutine_handle<> handle) { scheduler->enqueue(handle); }
            void await_resume() const noexcept {}
        };
        return Awaiter{.scheduler = this};
    }

    /// Resume all scheduled coroutines.
    void tick();

    /// Enqueue a coroutine to be resumed on the next tick.
    void enqueue(cppcoro::coroutine_handle<> handle) { scheduled_.lock()->push_back(handle); }

  private:
    Locked<std::vector<cppcoro::coroutine_handle<>>> scheduled_;
};
} // namespace Cory
