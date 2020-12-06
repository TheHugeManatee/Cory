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

#ifndef CORY_DISABLE_TESTS

#include "Log.h"

#include <doctest/doctest.h>
#include <thread>

TEST_CASE("ProfilerRecord class")
{
    using namespace Cory;
    ProfilerRecord<4> rec;

    SUBCASE("Correct stats")
    {
        auto s = rec.stats(); // [xx, xx, xx, xx]
        CHECK(s.min == 0);
        CHECK(s.max == 0);
        CHECK(s.avg == 0);

        rec.push(10);
        s = rec.stats(); // [10, xx, xx, xx]
        CHECK(s.min == 10);
        CHECK(s.max == 10);
        CHECK(s.avg == 10);

        rec.push(20);
        rec.push(60);
        s = rec.stats(); // [10, 20, 60, xx]
        CHECK(s.min == 10);
        CHECK(s.max == 60);
        CHECK(s.avg == 30);

        rec.push(5);
        rec.push(122);
        s = rec.stats(); // [20, 60, 5, 122]
        CHECK(s.min == 5);
        CHECK(s.max == 122);
        CHECK(s.avg == 51);
    }

    SUBCASE("History")
    {
        auto h = rec.history();
        CHECK(h.empty());

        rec.push(10);
        rec.push(20);
        h = rec.history();
        CHECK(h[0] == 10);
        CHECK(h[1] == 20);

        rec.push(30);
        rec.push(40);
        rec.push(50);
        h = rec.history();
        CHECK(h[0] == 20);
        CHECK(h[1] == 30);
        CHECK(h[2] == 40);
        CHECK(h[3] == 50);
    }
}

TEST_CASE("Scope timer functionality")
{
    {
        using namespace std::chrono_literals;
        Cory::ScopeTimer t("Test case");
        std::this_thread::sleep_for(10ms);

        {
            Cory::ScopeTimer t2("Nested timer");
            std::this_thread::sleep_for(2ms);
        }
        {
            Cory::ScopeTimer t2("Nested timer");
            std::this_thread::sleep_for(2ms);
        }
    }

    auto recs = Cory::Profiler::GetRecords();
    CHECK(recs.count("Test case") > 0);
    CHECK(recs.count("Nested timer") > 0);

    auto record = recs["Test case"];
    auto s = record.stats();
    CHECK(record.history().size() == 1);
    // total for this timer should be >14ms
    CHECK(s.min > 14'000);
    CHECK(s.max > 14'000);
    CHECK(s.avg > 14'000);

    record = recs["Nested timer"];
    CHECK(record.history().size() == 2);
    s = record.stats();
    CHECK(s.min > 2'000);
    CHECK(s.max > 2'000);
    CHECK(s.avg > 2'000);
}
#endif // CORY_DISABLE_TESTS