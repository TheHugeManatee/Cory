#pragma once

#include <Cory/Base/Profiling.hpp>

namespace CoImGui {

void drawProfilerRecords(const std::map<std::string, Cory::Profiler::Record> &records);

}