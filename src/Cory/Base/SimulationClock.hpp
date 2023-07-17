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
        time_point now;
        duration delta;
    };

    BasicSimulationClock() { reset(); }

    time_point realNow() const
    {
        using namespace std::chrono;
        return time_point{duration_cast<duration>(UpstreamClock::now() - epoch_)};
    }
    time_point simNow() const { return time_point{simulationNow_}; }

    TickInfo tick()
    {
        const auto now = UpstreamClock::now();
        const auto delta = now - lastTick_;

        duration simulatedDelta = duration_cast<duration>(delta) * timeScale_;

        simulationNow_ += simulatedDelta;
        lastTick_ = now;

        ++ticks_;

        return {.now = simulationNow_, .delta = simulatedDelta};
    }

    void setTimeScale(StorageType scale) { timeScale_ = scale; }
    StorageType timeScale() const { return timeScale_; }
    uint64_t ticks() const { return ticks_; }

    void reset()
    {
        epoch_ = UpstreamClock::now();
        lastTick_ = epoch_;
        ticks_ = 0;
        simulationNow_ = time_point{};
    }

    static void Init() {
        globalClock().reset();
    };
    // static interface for std::chrono compliant clock
    static BasicSimulationClock &globalClock()
    {
        static BasicSimulationClock clock{};
        return clock;
    }
    // global instance just ticks any time a time point is queried
    static time_point now() { return globalClock().tick().now; }

  private:
    UpstreamClock::time_point epoch_{};
    UpstreamClock::time_point lastTick_{};
    uint64_t ticks_{};
    time_point simulationNow_{};
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