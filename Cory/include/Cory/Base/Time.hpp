#pragma once

#include <chrono>

#include <fmt/core.h>

namespace Cory {

/**
 * The application clock timer
 */
class AppClock {
  public:
    // these are required to be an STL-compatible clock
    using duration = std::chrono::duration<double>;
    using rep = duration::rep;
    using period = duration::period;
    using time_point = std::chrono::time_point<AppClock>;
    static constexpr bool is_steady = false;

    using upstream_clock = std::chrono::high_resolution_clock;
    using system_clock = std::chrono::system_clock;

    inline static time_point now() noexcept
    {
        // return current time_point by subtracting epoch from system_clock
        return time_point(upstream_clock::now().time_since_epoch() - epoch_);
    }

    static void Init();

    inline static system_clock::time_point ToSystem(time_point t) noexcept
    {
        return system_clock::time_point(systemEpoch_ + ToSystem(t.time_since_epoch()));
    }

    inline static system_clock::duration ToSystem(duration d) noexcept
    {
        return std::chrono::duration_cast<system_clock::duration>(d);
    }

  private:
    // choose any epoch
    static upstream_clock::duration epoch_;
    static system_clock::duration systemEpoch_;
};

using Seconds = AppClock::duration;
using Timepoint = AppClock::time_point;

} // namespace Cory

// user-defined literals for convenience

/// create a seconds literal
constexpr Cory::Seconds operator""_s(long double x) { return Cory::Seconds(x); }
/// create a milliseconds literal
constexpr Cory::Seconds operator""_ms(long double x) { return Cory::Seconds(x / 1'000.0); }
/// create a microseconds literal
constexpr Cory::Seconds operator""_us(long double x) { return Cory::Seconds(x / 1'000'000.0); }
/// create a nanoseconds literal
constexpr Cory::Seconds operator""_ns(long double x) { return Cory::Seconds(x / 1'000'000'000.0); }
