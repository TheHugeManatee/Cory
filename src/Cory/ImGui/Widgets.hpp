#pragma once

#include <Cory/Base/Profiling.hpp>
#include <kdbindings/property.h>

namespace CoImGui {

void drawProfilerRecords(const std::map<std::string, Cory::Profiler::Record> &records);

} // namespace CoImGui
