#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <map>
#include <numeric>
#include <string>
#include <string_view>

namespace Cory {

template <int64_t RECORD_HISTORY_SIZE> class ProfilerRecord {
  public:
    ProfilerRecord() { m_data.fill(0); }

    struct Stats {
        int64_t min;
        int64_t max;
        int64_t avg;
    };

    void push(int64_t value)
    {
        m_data[m_currentIdx % RECORD_HISTORY_SIZE] = value;
        ++m_currentIdx;
    }

    Stats stats() const
    {
        if (m_currentIdx == 0) return {0, 0, 0};

        auto endIter =
            m_currentIdx > m_data.size() ? m_data.cend() : m_data.cbegin() + m_currentIdx;
        auto stats = std::accumulate(++m_data.cbegin(),
                                     endIter,
                                     Stats{m_data[0], m_data[0], m_data[0]},
                                     [](auto acc, const auto &value) {
                                         acc.min = std::min(acc.min, value);
                                         acc.max = std::max(acc.max, value);
                                         acc.avg += value;
                                         return acc;
                                     });
        stats.avg /= (m_currentIdx > RECORD_HISTORY_SIZE) ? RECORD_HISTORY_SIZE : m_currentIdx;
        return stats;
    }

    std::vector<int64_t> history() const
    {
        if (m_currentIdx == 0) return {};

        auto breakPoint = m_currentIdx % RECORD_HISTORY_SIZE;

        if (m_currentIdx <= RECORD_HISTORY_SIZE) {
            return {m_data.cbegin(), m_data.cbegin() + m_currentIdx};
        }

        std::vector<int64_t> hist{m_data.cbegin() + breakPoint, m_data.cend()};

        if (breakPoint > 0)
            std::copy(m_data.cbegin(), m_data.cbegin() + breakPoint, std::back_inserter(hist));

        return hist;
    }

  private:
    std::array<int64_t, RECORD_HISTORY_SIZE> m_data;
    std::size_t m_currentIdx{};
};

class Profiler {
  public:
    using Record = ProfilerRecord<128>;
    static void PushCounter(std::string &name, int64_t deltaNs);
    static std::map<std::string, Record> GetRecords() { return s_records; }

  private:
    static std::map<std::string, Record> s_records;
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