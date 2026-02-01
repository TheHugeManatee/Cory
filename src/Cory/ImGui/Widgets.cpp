#include "Widgets.hpp"

#include <Cory/ImGui/Inputs.hpp>

#include <algorithm>
#include <limits>

#include <gsl/narrow>
#include <imgui.h>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

namespace CoImGui {

void drawProfilerRecords(const std::map<std::string, Cory::Profiler::Record> &records)
{
    auto to_ms = [](uint64_t ns) { return double(ns) / 1'000'000.0; };

    if (ImGui::BeginTable("Profiling", 5)) {

        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("min [ms]", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("max [ms]", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("avg [ms]", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("graph", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (auto [name, record] : records) {
            auto stats = record.stats();
            auto hist = record.history();
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            CoImGui::Text("{}", name);
            ImGui::TableNextColumn();
            CoImGui::Text("{:3.2f}", to_ms(stats.min));
            ImGui::TableNextColumn();
            CoImGui::Text("{:3.2f}", to_ms(stats.max));
            ImGui::TableNextColumn();
            CoImGui::Text("{:3.2f}", to_ms(stats.avg));
            ImGui::TableNextColumn();

            auto h = hist | ranges::views::transform([](auto v) { return float(v); }) |
                     ranges::to<std::vector>;
            ImGui::PlotLines(
                "", h.data(), gsl::narrow<int>(h.size()), 0, nullptr, 0.0f, float(stats.max));
        }
        ImGui::EndTable();
    }
}

namespace {

template <typename T> void pushSample(std::vector<T> &samples, T value, size_t maxSamples)
{
    samples.push_back(value);
    if (samples.size() > maxSamples) {
        samples.erase(samples.begin(), samples.begin() + (samples.size() - maxSamples));
    }
}

float maxOrDefault(const std::vector<float> &values, float fallback)
{
    if (values.empty()) {
        return fallback;
    }
    return std::max(fallback, *std::max_element(values.begin(), values.end()));
}

} // namespace

void DeviceMemoryReportHistory::update(const Cory::DeviceMemoryReportStats &stats, double nowSeconds)
{
    if (!stats.supported) {
        hasSamples = false;
        return;
    }

    if (hasSamples && (nowSeconds - lastSampleTime) < sampleIntervalSeconds) {
        return;
    }

    lastSampleTime = nowSeconds;
    latestStats = stats;
    hasSamples = true;

    const auto bytesToMiB = [](uint64_t bytes) {
        return static_cast<float>(bytes) / (1024.0f * 1024.0f);
    };

    pushSample(currentBytesMiB, bytesToMiB(stats.currentBytes), maxSamples);
    pushSample(totalAllocatedMiB, bytesToMiB(stats.totalAllocatedBytes), maxSamples);
    pushSample(allocationCounts, static_cast<float>(stats.allocationCount), maxSamples);
    pushSample(allocationFailures, static_cast<float>(stats.allocationFailedCount), maxSamples);
}

void DeviceMemoryReportHistory::drawWindow(const char *title) const
{
    if (!ImGui::Begin(title)) {
        ImGui::End();
        return;
    }

    if (!hasSamples) {
        ImGui::TextDisabled("Device memory report extension is not available.");
        ImGui::End();
        return;
    }

    const auto bytesToMiB = [](uint64_t bytes) {
        return static_cast<float>(bytes) / (1024.0f * 1024.0f);
    };

    ImGui::Text("Sample interval: %.0f ms", sampleIntervalSeconds * 1000.0);
    ImGui::Separator();
    ImGui::Text("Current allocated: %.2f MiB", bytesToMiB(latestStats.currentBytes));
    ImGui::Text("Total allocated: %.2f MiB", bytesToMiB(latestStats.totalAllocatedBytes));
    ImGui::Text("Total freed: %.2f MiB", bytesToMiB(latestStats.totalFreedBytes));
    ImGui::Text("Allocations: %llu", static_cast<unsigned long long>(latestStats.allocationCount));
    ImGui::Text("Frees: %llu", static_cast<unsigned long long>(latestStats.freeCount));
    ImGui::Text("Imports: %llu", static_cast<unsigned long long>(latestStats.importCount));
    ImGui::Text("Unimports: %llu", static_cast<unsigned long long>(latestStats.unimportCount));
    ImGui::Text("Allocation failures: %llu",
                static_cast<unsigned long long>(latestStats.allocationFailedCount));

    ImGui::Separator();

    const float currentMax = maxOrDefault(currentBytesMiB, 1.0f);
    ImGui::PlotLines("Current Allocated (MiB)",
                     currentBytesMiB.data(),
                     gsl::narrow<int>(currentBytesMiB.size()),
                     0,
                     nullptr,
                     0.0f,
                     currentMax);

    const float allocationsMax = maxOrDefault(allocationCounts, 1.0f);
    ImGui::PlotLines("Allocation Count",
                     allocationCounts.data(),
                     gsl::narrow<int>(allocationCounts.size()),
                     0,
                     nullptr,
                     0.0f,
                     allocationsMax);

    const float failureMax = maxOrDefault(allocationFailures, 1.0f);
    ImGui::PlotLines("Allocation Failures",
                     allocationFailures.data(),
                     gsl::narrow<int>(allocationFailures.size()),
                     0,
                     nullptr,
                     0.0f,
                     failureMax);

    if (!latestStats.heaps.empty()) {
        ImGui::Separator();
        if (ImGui::BeginTable("MemoryReportHeaps", 6)) {
            ImGui::TableSetupColumn("Heap");
            ImGui::TableSetupColumn("Current (MiB)");
            ImGui::TableSetupColumn("Allocated (MiB)");
            ImGui::TableSetupColumn("Freed (MiB)");
            ImGui::TableSetupColumn("Allocs");
            ImGui::TableSetupColumn("Frees");
            ImGui::TableHeadersRow();

            for (const auto &heap : latestStats.heaps) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%u", heap.heapIndex);
                ImGui::TableNextColumn();
                ImGui::Text("%.2f", bytesToMiB(heap.currentBytes));
                ImGui::TableNextColumn();
                ImGui::Text("%.2f", bytesToMiB(heap.totalAllocatedBytes));
                ImGui::TableNextColumn();
                ImGui::Text("%.2f", bytesToMiB(heap.totalFreedBytes));
                ImGui::TableNextColumn();
                ImGui::Text("%llu",
                            static_cast<unsigned long long>(heap.allocationCount));
                ImGui::TableNextColumn();
                ImGui::Text("%llu", static_cast<unsigned long long>(heap.freeCount));
            }

            ImGui::EndTable();
        }
    }

    ImGui::End();
}
} // namespace CoImGui
