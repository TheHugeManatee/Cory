#include "Profiling.h"

#include <chrono>
#include <numeric>

namespace Cory {

void ProfilerRecord::push(long long value)
{
    m_data[m_currentIdx % RECORD_HISTORY_SIZE] = value;
    m_currentIdx++;
}

ProfilerRecord::Stats ProfilerRecord::stats() const
{
    auto endIter = m_currentIdx > m_data.size() ? m_data.cend() : m_data.cbegin() + m_currentIdx;
    auto stats = std::accumulate(++m_data.cbegin(), endIter, Stats{m_data[0], m_data[0], m_data[0]},
                                 [](auto acc, const auto &value) {
                                     acc.min = std::min(acc.min, value);
                                     acc.max = std::max(acc.max, value);
                                     acc.avg += value;
                                     return acc;
                                 });
    stats.avg /= RECORD_HISTORY_SIZE;
    return stats;
}

std::unordered_map<std::string, ProfilerRecord> Profiler::s_records;

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
