#pragma once

#include <Cory/Base/Profiling.hpp>
#include <Cory/Renderer/Context.hpp>

#include <cstddef>
#include <vector>

namespace CoImGui {

void drawProfilerRecords(const std::map<std::string, Cory::Profiler::Record> &records);

struct DeviceMemoryReportHistory {
    double sampleIntervalSeconds{0.3};
    double lastSampleTime{0.0};
    size_t maxSamples{120};
    bool hasSamples{false};

    Cory::DeviceMemoryReportStats latestStats{};
    std::vector<float> currentBytesMiB;
    std::vector<float> totalAllocatedMiB;
    std::vector<float> allocationCounts;
    std::vector<float> allocationFailures;

    void update(const Cory::DeviceMemoryReportStats &stats, double nowSeconds);
    void drawWindow(const char *title = "Device Memory Report") const;
};

}
