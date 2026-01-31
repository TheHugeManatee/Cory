#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <map>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

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

        const auto count = std::min<std::size_t>(m_currentIdx, m_data.size());
        auto endIter = std::next(m_data.cbegin(), static_cast<ptrdiff_t>(count));
        const auto beginIter = m_data.cbegin();
        auto stats = std::accumulate(std::next(beginIter),
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
            return {m_data.cbegin(), std::next(m_data.cbegin(), static_cast<ptrdiff_t>(m_currentIdx))};
        }

        std::vector<int64_t> hist{std::next(m_data.cbegin(), static_cast<ptrdiff_t>(breakPoint)),
                                  m_data.cend()};

        if (breakPoint > 0) {
            const auto breakIter =
                std::next(m_data.cbegin(), static_cast<ptrdiff_t>(breakPoint));
            std::copy(m_data.cbegin(), breakIter, std::back_inserter(hist));
        }

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
    static std::map<std::string, Record> &GetRecords();

  private:
    // No data members; storage is held by GetRecords() static function.
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

    Record::Stats stats() const { return m_lapTimes.stats(); }
    auto hist() const { return m_lapTimes.history(); }

  private:
    std::chrono::high_resolution_clock::time_point m_lastLap;
    Record m_lapTimes;
    std::chrono::high_resolution_clock::time_point m_lastReport;
    std::chrono::milliseconds m_reportInterval;
};

class ElapsedTimer {
    std::chrono::high_resolution_clock::time_point m_start;

  public:
    ElapsedTimer()
        : m_start{std::chrono::high_resolution_clock::now()}
    {
    }

    [[nodiscard]] double elapsed() const noexcept
    {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>(now - m_start).count();
    }
};

} // namespace Cory
