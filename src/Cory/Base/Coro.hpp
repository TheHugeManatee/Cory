#pragma once

#include "Log.hpp"

#include <cppcoro/coroutine.hpp>

namespace Cory {

/// Synchronous coroutine that will run eagerly run upo creation to the first co_await
/// and then allow retrieving the result via get().
/// It always final-suspends, so the caller is responsible for destroying the coroutine handle.
class EagerJob : NoCopy {
  public:
    struct promise_type {
        using Handle = cppcoro::coroutine_handle<promise_type>;
        EagerJob get_return_object() { return EagerJob{Handle::from_promise(*this)}; }
        cppcoro::suspend_never initial_suspend() noexcept { return {}; }
        cppcoro::suspend_always final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        [[noreturn]] void unhandled_exception()
        {
            CO_CORE_ERROR("Unhandled exception in EagerJob coroutine");
            std::terminate();
        }
    };

    using Handle = cppcoro::coroutine_handle<promise_type>;

    EagerJob() = default;
    explicit EagerJob(cppcoro::coroutine_handle<> h)
        : handle(h)
    {
    }
    EagerJob(EagerJob &&other) noexcept
        : handle(other.handle)
    {
        other.handle = nullptr;
    }
    EagerJob &operator=(EagerJob &&other) noexcept
    {
        if (this != &other) {
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }
    ~EagerJob()
    {
        if (handle) {
            handle.destroy();
        }
    }

  private:
    cppcoro::coroutine_handle<> handle{nullptr};
};

} // namespace Cory
