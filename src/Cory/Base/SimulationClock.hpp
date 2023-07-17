//
// Created by j on 7/17/2023.
//

#pragma once
#include <chrono>
#include <tuple>

namespace Cory {
/**
 * This is s std::chrono compatible clock with some adaptations for simulation
 *
 * Mainly:
 *   - The clock has to be tick()ed to advance time
 *   - The clock can be scaled to run faster or slower than real time (including
 *     pausing by setting time scale to 0.0)
 *   - The clock can be instantiated. A global clock is provided for convenience,
 *     but multiple clocks can be used for different purposes.
 */
template <typename UpstreamClock> class BasicSimulationClock {
  public:
    using StorageType = double;
    // symmetry to std::chrono::clock
    static constexpr bool is_steady{false};
    using duration = std::chrono::duration<StorageType>;
    using rep = duration::rep;
    using period = duration::period;
    using time_point = std::chrono::time_point<BasicSimulationClock>;

    struct TickInfo {
        time_point now{};     ///< the simulation time after the tick
        time_point realNow{}; ///< the real time after the tick
        duration delta{};     ///< the elapsed time
    };

    BasicSimulationClock() { reset(); }

    time_point realNow() const { return lastTick_.realNow; }
    time_point simNow() const { return lastTick_.now; }
    duration delta() const { return lastTick_.delta; }
    uint64_t ticks() const { return ticks_; }

    TickInfo tick()
    {
        const auto upstreamNow = UpstreamClock::now();
        const auto delta = upstreamNow - lastTickUpstream_;

        duration simulatedDelta = duration_cast<duration>(delta) * timeScale_;

        TickInfo thisTick{.now = simNow() + simulatedDelta,
                          .realNow = realNow() + delta,
                          .delta = simulatedDelta};
        ++ticks_;

        lastTick_ = thisTick;
        lastTickUpstream_ = upstreamNow;

        return thisTick;
    }

    void setTimeScale(StorageType scale) { timeScale_ = scale; }
    StorageType timeScale() const { return timeScale_; }

    void reset()
    {
        upstreamEpoch_ = UpstreamClock::now();
        lastTickUpstream_ = upstreamEpoch_;
        lastTick_ = TickInfo{};
        ticks_ = 0;
    }

    static void Init() { globalClock().reset(); };
    // static interface for std::chrono compliant clock
    static BasicSimulationClock &globalClock()
    {
        static BasicSimulationClock clock{};
        return clock;
    }
    // global instance just ticks any time a time point is queried
    static time_point now() { return globalClock().tick().now; }

  private:
    UpstreamClock::time_point upstreamEpoch_{};
    UpstreamClock::time_point lastTickUpstream_{};
    TickInfo lastTick_{};
    uint64_t ticks_{};
    StorageType timeScale_{1.0};
};

using SimulationClock = BasicSimulationClock<std::chrono::high_resolution_clock>;
using Seconds = SimulationClock::duration;

namespace literals {
/// create a seconds literal
constexpr Cory::Seconds operator""_s(long double x) { return Cory::Seconds(x); }
/// create a milliseconds literal
constexpr Cory::Seconds operator""_ms(long double x) { return Cory::Seconds(x / 1'000.0); }
/// create a microseconds literal
constexpr Cory::Seconds operator""_us(long double x) { return Cory::Seconds(x / 1'000'000.0); }
/// create a nanoseconds literal
constexpr Cory::Seconds operator""_ns(long double x) { return Cory::Seconds(x / 1'000'000'000.0); }
} // namespace literals
} // namespace Cory

// comparison operators for convenience
inline auto operator<=>(Cory::SimulationClock::time_point lhs, double rhs)
{
    return lhs.time_since_epoch().count() <=> rhs;
}
inline auto operator<=>(Cory::SimulationClock::duration lhs, double rhs)
{
    return lhs.count() <=> rhs;
}