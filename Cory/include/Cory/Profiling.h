#pragma once

#include <array>
#include <chrono>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Cory {

class ProfilerRecord {
  public:
    static constexpr std::size_t RECORD_HISTORY_SIZE{64};

    ProfilerRecord() { m_data.fill(0); }

    struct Stats {
        long long min;
        long long max;
        long long avg;
    };

    void push(long long value);

    Stats stats() const;

  private:
    std::array<long long, RECORD_HISTORY_SIZE> m_data;
    std::size_t m_currentIdx{};
};

class Profiler {
  public:
    static void PushCounter(std::string &name, long long deltaNs);
    static std::unordered_map<std::string, ProfilerRecord> GetRecords() { return s_records; }

  private:
    static std::unordered_map<std::string, ProfilerRecord> s_records;
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
    LapTimer(std::chrono::milliseconds reportInterval = std::chrono::milliseconds{1000})
        : m_reportInterval{reportInterval}
    {
        m_lastReport = m_lastLap = std::chrono::high_resolution_clock::now();
    }

    bool lap()
    {
        // save the lap time
        auto now = std::chrono::high_resolution_clock::now();
        auto lapTime =
            std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_lastLap).count();
        m_lapTimes.push(lapTime);
        m_lastLap = now;

        // reporting logic
        if (std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_lastReport) >
            m_reportInterval) {
            m_lastReport = now;
            return true;
        }

        return false;
    };

    ProfilerRecord::Stats stats() { return m_lapTimes.stats(); };

  private:
    std::chrono::high_resolution_clock::time_point m_lastLap;
    ProfilerRecord m_lapTimes;
    std::chrono::high_resolution_clock::time_point m_lastReport;
    std::chrono::milliseconds m_reportInterval;
};

} // namespace Cory