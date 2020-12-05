#pragma once

#include <array>
#include <chrono>
#include <iterator>
#include <numeric>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Cory {

template <long long RECORD_HISTORY_SIZE = 64> class ProfilerRecord {
  public:
    ProfilerRecord() { m_data.fill(0); }

    struct Stats {
        long long min;
        long long max;
        long long avg;
    };

    void push(long long value)
    {
        m_data[m_currentIdx % RECORD_HISTORY_SIZE] = value;
        m_currentIdx++;
    }

    Stats stats() const
    {
        // FIXME: stats are wrong when only one item exists?
        auto endIter =
            m_currentIdx > m_data.size() ? m_data.cend() : m_data.cbegin() + m_currentIdx;
        auto stats =
            std::accumulate(++m_data.cbegin(), endIter, Stats{m_data[0], m_data[0], m_data[0]},
                            [](auto acc, const auto &value) {
                                acc.min = std::min(acc.min, value);
                                acc.max = std::max(acc.max, value);
                                acc.avg += value;
                                return acc;
                            });
        stats.avg /= RECORD_HISTORY_SIZE;
        return stats;
    }

    std::vector<long long> history() const
    {
        auto breakPoint = m_currentIdx % RECORD_HISTORY_SIZE;

        std::vector<long long> hist{m_data.cbegin() + breakPoint, m_data.cend()};

        if (breakPoint > 0)
            std::copy(m_data.cbegin(), m_data.cbegin() + breakPoint - 1, std::back_inserter(hist));

        return hist;
    }

  private:
    std::array<long long, RECORD_HISTORY_SIZE> m_data;
    std::size_t m_currentIdx{};
};

class Profiler {
  public:
    using Record = ProfilerRecord<128>;
    static void PushCounter(std::string &name, long long deltaNs);
    static std::unordered_map<std::string, Record> GetRecords() { return s_records; }

  private:
    static std::unordered_map<std::string, Record> s_records;
};

class ScopeTimer {
  public:
    ScopeTimer(std::string name);

    ~ScopeTimer();

  private:
    std::chrono::high_resolution_clock::time_point m_start;
    std::string m_name;
};

class LapTimer {
  public:
    using Record = ProfilerRecord<256>;
    LapTimer(std::chrono::milliseconds reportInterval = std::chrono::milliseconds{1000});

    bool lap();

    Record::Stats stats() const { return m_lapTimes.stats(); };
    auto hist() const { return m_lapTimes.history(); }

  private:
    std::chrono::high_resolution_clock::time_point m_lastLap;
    Record m_lapTimes;
    std::chrono::high_resolution_clock::time_point m_lastReport;
    std::chrono::milliseconds m_reportInterval;
};

} // namespace Cory