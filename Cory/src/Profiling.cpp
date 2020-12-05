#include "Profiling.h"

#include <chrono>


namespace Cory {



std::unordered_map<std::string, Profiler::Record> Profiler::s_records;

void Profiler::PushCounter(std::string &name, long long deltaNs) { s_records[name].push(deltaNs); }

ScopeTimer::ScopeTimer(std::string name)
    : m_start{std::chrono::high_resolution_clock::now()}
    , m_name{name}
{
}

ScopeTimer::~ScopeTimer()
{
    auto end = std::chrono::high_resolution_clock::now();
    auto delta = std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_start).count();
    Profiler::PushCounter(m_name, delta);
}

LapTimer::LapTimer(std::chrono::milliseconds reportInterval)
    : m_reportInterval{reportInterval}
{
    m_lastReport = m_lastLap = std::chrono::high_resolution_clock::now();
}

bool LapTimer::lap()
{
    // save the lap time
    auto now = std::chrono::high_resolution_clock::now();
    auto lapTime = std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_lastLap).count();
    m_lapTimes.push(lapTime);
    m_lastLap = now;

    // reporting logic
    if (std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_lastReport) >
        m_reportInterval) {
        m_lastReport = now;
        return true;
    }

    return false;
}

} // namespace Cory

#ifdef CORY_ENABLE_TESTING
#include "Log.h"

#include <doctest/doctest.h>
#include <thread>

TEST_CASE("Testing basic timer API functionality")
{
    {
        using namespace std::chrono_literals;
        Cory::ScopeTimer t("Test case");
        std::this_thread::sleep_for(10ms);

        {
            Cory::ScopeTimer t2("Nested timer");
            std::this_thread::sleep_for(2ms);
        }
    }

    auto recs = Cory::Profiler::GetRecords();
    for (const auto [k, v] : recs) {
        auto s = v.stats();

        CO_CORE_INFO("{}: {} ({}-{})", k, s.avg, s.min, s.max);

        CHECK(s.min == 0);
        CHECK(s.max > 0);
        CHECK(s.avg > 0);
    }
}
#endif // CORY_ENABLE_TESTING
