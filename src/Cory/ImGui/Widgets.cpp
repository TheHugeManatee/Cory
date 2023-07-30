#include "Widgets.hpp"

#include <Cory/ImGui/Inputs.hpp>

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
} // namespace CoImGui