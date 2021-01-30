#include "Profiling.h"

#include <chrono>

namespace Cory {

std::unordered_map<std::string, Profiler::Record> Profiler::s_records;

void Profiler::PushCounter(std::string &name, long long deltaNs)
{
  s_records[name].push(deltaNs);
}

ScopeTimer::ScopeTimer(std::string name)
    : m_start{std::chrono::high_resolution_clock::now()}
    , m_name{name}
{
}

ScopeTimer::~ScopeTimer()
{
  auto end = std::chrono::high_resolution_clock::now();
  auto delta =
      std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_start)
          .count();
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
  auto lapTime =
      std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_lastLap)
          .count();
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