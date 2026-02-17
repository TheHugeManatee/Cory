#pragma once

#include <Cory/Base/Common.hpp>

#include "Log.hpp"

#include <cppcoro/awaitable_traits.hpp>
#include <cppcoro/config.hpp>
#include <cppcoro/coroutine.hpp>

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>

namespace Cory {

namespace detail {

class LightweightManualResetEvent {
  public:
    explicit LightweightManualResetEvent(bool initiallySet = false);
    ~LightweightManualResetEvent();

    LightweightManualResetEvent(const LightweightManualResetEvent &) = delete;
    LightweightManualResetEvent &operator=(const LightweightManualResetEvent &) = delete;

    void set() noexcept;
    void reset() noexcept;
    void wait() noexcept;

  private:
#if CPPCORO_OS_LINUX
    std::atomic<int> m_value;
#elif CPPCORO_OS_WINNT >= 0x0602
    std::atomic<std::uint8_t> m_value;
#elif CPPCORO_OS_WINNT
    void *m_eventHandle;
#else
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_isSet;
#endif
};

template <typename TResult> class SyncWaitTask;

template <typename TResult> class SyncWaitTaskPromise final {
    using CoroutineHandle = cppcoro::coroutine_handle<SyncWaitTaskPromise<TResult>>;

  public:
    using Reference = TResult &&;

    SyncWaitTaskPromise() noexcept = default;

    void start(LightweightManualResetEvent &event)
    {
        m_event = &event;
        CoroutineHandle::from_promise(*this).resume();
    }

    auto get_return_object() noexcept { return CoroutineHandle::from_promise(*this); }

    cppcoro::suspend_always initial_suspend() noexcept { return {}; }

    auto final_suspend() noexcept
    {
        class CompletionNotifier {
          public:
            bool await_ready() const noexcept { return false; }

            void await_suspend(CoroutineHandle coroutine) const noexcept
            {
                coroutine.promise().m_event->set();
            }

            void await_resume() noexcept {}
        };

        return CompletionNotifier{};
    }

#if CPPCORO_COMPILER_MSVC && CPPCORO_COMPILER_MSVC < 19'20'00000
    // Work around an older MSVC coroutine bug in universal-reference await_transform.
    template <typename TAwaitable> TAwaitable &&await_transform(TAwaitable &&awaitable)
    {
        return static_cast<TAwaitable &&>(awaitable);
    }

    struct GetPromiseTag {};
    static constexpr GetPromiseTag getPromise = {};

    auto await_transform(GetPromiseTag)
    {
        class Awaiter {
          public:
            explicit Awaiter(SyncWaitTaskPromise *promise) noexcept
                : m_promise(promise)
            {
            }

            bool await_ready() noexcept { return true; }
            void await_suspend(cppcoro::coroutine_handle<>) noexcept {}
            SyncWaitTaskPromise &await_resume() noexcept { return *m_promise; }

          private:
            SyncWaitTaskPromise *m_promise;
        };

        return Awaiter{this};
    }
#endif

    auto yield_value(Reference result) noexcept
    {
        m_result = std::addressof(result);
        return final_suspend();
    }

    void return_void() noexcept
    {
        // This coroutine should either yield a result or capture an exception.
        assert(false);
    }

    void unhandled_exception() { m_exception = std::current_exception(); }

    Reference result()
    {
        if (m_exception) {
            std::rethrow_exception(m_exception);
        }

        return static_cast<Reference>(*m_result);
    }

  private:
    LightweightManualResetEvent *m_event{nullptr};
    std::remove_reference_t<TResult> *m_result{nullptr};
    std::exception_ptr m_exception{};
};

template <> class SyncWaitTaskPromise<void> final {
    using CoroutineHandle = cppcoro::coroutine_handle<SyncWaitTaskPromise<void>>;

  public:
    SyncWaitTaskPromise() noexcept = default;

    void start(LightweightManualResetEvent &event)
    {
        m_event = &event;
        CoroutineHandle::from_promise(*this).resume();
    }

    auto get_return_object() noexcept { return CoroutineHandle::from_promise(*this); }

    cppcoro::suspend_always initial_suspend() noexcept { return {}; }

    auto final_suspend() noexcept
    {
        class CompletionNotifier {
          public:
            bool await_ready() const noexcept { return false; }

            void await_suspend(CoroutineHandle coroutine) const noexcept
            {
                coroutine.promise().m_event->set();
            }

            void await_resume() noexcept {}
        };

        return CompletionNotifier{};
    }

    void return_void() noexcept {}

    void unhandled_exception() { m_exception = std::current_exception(); }

    void result()
    {
        if (m_exception) {
            std::rethrow_exception(m_exception);
        }
    }

  private:
    LightweightManualResetEvent *m_event{nullptr};
    std::exception_ptr m_exception{};
};

template <typename TResult> class SyncWaitTask final {
  public:
    using promise_type = SyncWaitTaskPromise<TResult>;
    using CoroutineHandle = cppcoro::coroutine_handle<promise_type>;

    SyncWaitTask(CoroutineHandle coroutine) noexcept
        : m_coroutine(coroutine)
    {
    }

    SyncWaitTask(SyncWaitTask &&other) noexcept
        : m_coroutine(std::exchange(other.m_coroutine, CoroutineHandle{}))
    {
    }

    ~SyncWaitTask()
    {
        if (m_coroutine) {
            m_coroutine.destroy();
        }
    }

    SyncWaitTask(const SyncWaitTask &) = delete;
    SyncWaitTask &operator=(const SyncWaitTask &) = delete;

    void start(LightweightManualResetEvent &event) noexcept { m_coroutine.promise().start(event); }

    decltype(auto) result() { return m_coroutine.promise().result(); }

  private:
    CoroutineHandle m_coroutine;
};

#if CPPCORO_COMPILER_MSVC && CPPCORO_COMPILER_MSVC < 19'20'00000
template <typename TAwaitable,
          typename TResult = typename cppcoro::awaitable_traits<TAwaitable &&>::await_result_t,
          std::enable_if_t<!std::is_void_v<TResult>, int> = 0>
SyncWaitTask<TResult> makeSyncWaitTask(TAwaitable &awaitable)
{
    // Work around older MSVC bug where co_yield/co_await interaction is broken.
    auto &promise = co_await SyncWaitTaskPromise<TResult>::getPromise;
    co_await promise.yield_value(co_await std::forward<TAwaitable>(awaitable));
}

template <typename TAwaitable,
          typename TResult = typename cppcoro::awaitable_traits<TAwaitable &&>::await_result_t,
          std::enable_if_t<std::is_void_v<TResult>, int> = 0>
SyncWaitTask<void> makeSyncWaitTask(TAwaitable &awaitable)
{
    co_await static_cast<TAwaitable &&>(awaitable);
}
#else
template <typename TAwaitable,
          typename TResult = typename cppcoro::awaitable_traits<TAwaitable &&>::await_result_t,
          std::enable_if_t<!std::is_void_v<TResult>, int> = 0>
SyncWaitTask<TResult> makeSyncWaitTask(TAwaitable &&awaitable)
{
    co_yield co_await std::forward<TAwaitable>(awaitable);
}

template <typename TAwaitable,
          typename TResult = typename cppcoro::awaitable_traits<TAwaitable &&>::await_result_t,
          std::enable_if_t<std::is_void_v<TResult>, int> = 0>
SyncWaitTask<void> makeSyncWaitTask(TAwaitable &&awaitable)
{
    co_await std::forward<TAwaitable>(awaitable);
}
#endif

} // namespace detail

template <typename TAwaitable>
auto sync_wait(TAwaitable &&awaitable) ->
    typename cppcoro::awaitable_traits<TAwaitable &&>::await_result_t
{
#if CPPCORO_COMPILER_MSVC && CPPCORO_COMPILER_MSVC < 19'20'00000
    auto task = detail::makeSyncWaitTask<TAwaitable>(awaitable);
#else
    auto task = detail::makeSyncWaitTask(std::forward<TAwaitable>(awaitable));
#endif

    auto event = detail::LightweightManualResetEvent{};
    task.start(event);
    event.wait();
    return task.result();
}

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
