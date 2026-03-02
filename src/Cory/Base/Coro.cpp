#include <Cory/Base/Coro.hpp>

#include <system_error>

#if CPPCORO_OS_WINNT
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#if CPPCORO_OS_WINNT >= 0x0602

Cory::detail::LightweightManualResetEvent::LightweightManualResetEvent(bool initiallySet)
    : m_value(initiallySet ? 1 : 0)
{
}

Cory::detail::LightweightManualResetEvent::~LightweightManualResetEvent() = default;

void Cory::detail::LightweightManualResetEvent::set() noexcept
{
    m_value.store(1, std::memory_order_release);
    ::WakeByAddressAll(&m_value);
}

void Cory::detail::LightweightManualResetEvent::reset() noexcept
{
    m_value.store(0, std::memory_order_relaxed);
}

void Cory::detail::LightweightManualResetEvent::wait() noexcept
{
    // WaitOnAddress can have spurious wake-ups.
    auto value = static_cast<int>(m_value.load(std::memory_order_acquire));
    auto ok = TRUE;
    while (value == 0) {
        if (!ok) {
            ::Sleep(1);
        }

        ok = ::WaitOnAddress(&m_value, &value, sizeof(m_value), INFINITE);
        value = static_cast<int>(m_value.load(std::memory_order_acquire));
    }
}

#else

Cory::detail::LightweightManualResetEvent::LightweightManualResetEvent(bool initiallySet)
{
    m_eventHandle = ::CreateEventW(nullptr, TRUE, initiallySet, nullptr);
    if (m_eventHandle == nullptr) {
        const auto errorCode = ::GetLastError();
        throw std::system_error{static_cast<int>(errorCode), std::system_category()};
    }
}

Cory::detail::LightweightManualResetEvent::~LightweightManualResetEvent()
{
    // Destructor is noexcept, so ignore close failure.
    (void)::CloseHandle(static_cast<HANDLE>(m_eventHandle));
}

void Cory::detail::LightweightManualResetEvent::set() noexcept
{
    if (!::SetEvent(static_cast<HANDLE>(m_eventHandle))) {
        std::abort();
    }
}

void Cory::detail::LightweightManualResetEvent::reset() noexcept
{
    if (!::ResetEvent(static_cast<HANDLE>(m_eventHandle))) {
        std::abort();
    }
}

void Cory::detail::LightweightManualResetEvent::wait() noexcept
{
    constexpr auto alertable = FALSE;
    auto waitResult =
        ::WaitForSingleObjectEx(static_cast<HANDLE>(m_eventHandle), INFINITE, alertable);
    if (waitResult == WAIT_FAILED) {
        std::abort();
    }
}

#endif

#elif CPPCORO_OS_LINUX

#include <cerrno>
#include <climits>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cassert>
#include <ctime>

namespace {

int futex(int *userAddress,
          int futexOperation,
          int value,
          const struct timespec *timeout,
          int *userAddress2,
          int value3)
{
    return syscall(SYS_futex, userAddress, futexOperation, value, timeout, userAddress2, value3);
}

} // namespace

Cory::detail::LightweightManualResetEvent::LightweightManualResetEvent(bool initiallySet)
    : m_value(initiallySet ? 1 : 0)
{
}

Cory::detail::LightweightManualResetEvent::~LightweightManualResetEvent() = default;

void Cory::detail::LightweightManualResetEvent::set() noexcept
{
    m_value.store(1, std::memory_order_release);
    constexpr auto numberOfWaitersToWakeUp = INT_MAX;

    [[maybe_unused]] auto numberOfWaitersWokenUp = futex(reinterpret_cast<int *>(&m_value),
                                                         FUTEX_WAKE_PRIVATE,
                                                         numberOfWaitersToWakeUp,
                                                         nullptr,
                                                         nullptr,
                                                         0);

    // Errors here indicate invalid usage/state.
    assert(numberOfWaitersWokenUp != -1);
}

void Cory::detail::LightweightManualResetEvent::reset() noexcept
{
    m_value.store(0, std::memory_order_relaxed);
}

void Cory::detail::LightweightManualResetEvent::wait() noexcept
{
    // Wait in a loop to handle spurious wake-ups and transient futex errors.
    auto oldValue = m_value.load(std::memory_order_acquire);
    while (oldValue == 0) {
        auto result = futex(
            reinterpret_cast<int *>(&m_value), FUTEX_WAIT_PRIVATE, oldValue, nullptr, nullptr, 0);
        if (result == -1) {
            if (errno != EAGAIN && errno != EINTR) {
                // Treat unknown errors as transient; reload and retry.
            }
        }

        // Important: always reload after FUTEX_WAIT, including EAGAIN.
        // Returning on EAGAIN can skip the acquire load that synchronizes with set().
        oldValue = m_value.load(std::memory_order_acquire);
    }
}

#else

Cory::detail::LightweightManualResetEvent::LightweightManualResetEvent(bool initiallySet)
    : m_isSet(initiallySet)
{
}

Cory::detail::LightweightManualResetEvent::~LightweightManualResetEvent() = default;

void Cory::detail::LightweightManualResetEvent::set() noexcept
{
    auto lock = std::lock_guard<std::mutex>{m_mutex};
    m_isSet = true;
    m_cv.notify_all();
}

void Cory::detail::LightweightManualResetEvent::reset() noexcept
{
    auto lock = std::lock_guard<std::mutex>{m_mutex};
    m_isSet = false;
}

void Cory::detail::LightweightManualResetEvent::wait() noexcept
{
    auto lock = std::unique_lock<std::mutex>{m_mutex};
    m_cv.wait(lock, [this] { return m_isSet; });
}

#endif
