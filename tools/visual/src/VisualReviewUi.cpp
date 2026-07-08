#include <Cory/Tools/VisualReviewUi.hpp>

#include <fmt/format.h>
#include <gsl/narrow>
#include <imgui.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace Cory::Tools::VisualReview {
namespace {

using ImageRgba8 = IO::BmpImageRgba8;

[[nodiscard]] ImU32 pixelColor(const ImageRgba8 &image, uint32_t x, uint32_t y)
{
    const auto offset =
        (static_cast<size_t>(y) * static_cast<size_t>(image.width) + static_cast<size_t>(x)) * 4U;
    return IM_COL32(static_cast<uint8_t>(image.pixelsRgba8[offset + 0]),
                    static_cast<uint8_t>(image.pixelsRgba8[offset + 1]),
                    static_cast<uint8_t>(image.pixelsRgba8[offset + 2]),
                    static_cast<uint8_t>(image.pixelsRgba8[offset + 3]));
}

void drawImagePixels(const char *label, const ImageRgba8 *image, float zoom)
{
    ImGui::BeginChild(label, ImVec2{0.0f, 0.0f}, true, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::TextUnformatted(label);
    if (image == nullptr) {
        ImGui::TextDisabled("Image unavailable");
        ImGui::EndChild();
        return;
    }

    ImGui::Text("%u x %u", image->width, image->height);
    const auto origin = ImGui::GetCursorScreenPos();
    const auto pixelSize = std::max(1.0f, zoom);
    const auto canvasSize = ImVec2{static_cast<float>(image->width) * pixelSize,
                                   static_cast<float>(image->height) * pixelSize};
    ImGui::InvisibleButton(fmt::format("{}-canvas", label).c_str(), canvasSize);

    auto *drawList = ImGui::GetWindowDrawList();
    const auto clipMin = ImGui::GetWindowPos();
    const auto clipMax =
        ImVec2{clipMin.x + ImGui::GetWindowWidth(), clipMin.y + ImGui::GetWindowHeight()};
    drawList->PushClipRect(clipMin, clipMax, true);

    const auto minX = std::clamp(static_cast<int>((clipMin.x - origin.x) / pixelSize) - 1,
                                 0,
                                 gsl::narrow<int>(image->width));
    const auto maxX = std::clamp(static_cast<int>((clipMax.x - origin.x) / pixelSize) + 1,
                                 0,
                                 gsl::narrow<int>(image->width));
    const auto minY = std::clamp(static_cast<int>((clipMin.y - origin.y) / pixelSize) - 1,
                                 0,
                                 gsl::narrow<int>(image->height));
    const auto maxY = std::clamp(static_cast<int>((clipMax.y - origin.y) / pixelSize) + 1,
                                 0,
                                 gsl::narrow<int>(image->height));

    for (int y = minY; y < maxY; ++y) {
        for (int x = minX; x < maxX; ++x) {
            const auto p0 = ImVec2{origin.x + static_cast<float>(x) * pixelSize,
                                   origin.y + static_cast<float>(y) * pixelSize};
            const auto p1 = ImVec2{p0.x + pixelSize, p0.y + pixelSize};
            drawList->AddRectFilled(
                p0, p1, pixelColor(*image, gsl::narrow<uint32_t>(x), gsl::narrow<uint32_t>(y)));
        }
    }
    drawList->PopClipRect();
    ImGui::EndChild();
}

} // namespace

VisualReviewUiActions drawReviewUi(const VisualReviewRequest &request,
                                   const VisualReviewUiImages &images,
                                   VisualReviewUiState &state)
{
    VisualReviewUiActions actions;

    ImGui::SetNextWindowPos(ImVec2{0.0f, 0.0f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
    if (!ImGui::Begin("Visual Review",
                      nullptr,
                      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoMove)) {
        ImGui::End();
        return actions;
    }

    ImGui::Text("Case: %s", request.caseName.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("Request: %s", request.id.c_str());
    ImGui::Text("Catch test: %s", request.metadata.catchTestName.c_str());
    ImGui::Text("Source: %s:%llu",
                request.metadata.sourceFile.c_str(),
                static_cast<unsigned long long>(request.metadata.sourceLine));
    ImGui::Text("Mismatched pixels: %llu (%.6f), max channel error: %u, MAE: %.4f",
                static_cast<unsigned long long>(request.metrics.mismatchedPixels),
                request.metrics.mismatchRatio,
                static_cast<unsigned>(request.metrics.maxChannelError),
                request.metrics.meanAbsoluteError);
    ImGui::TextDisabled("Baseline: %s", request.baselinePath.string().c_str());
    ImGui::TextDisabled("Actual:   %s", request.actualPath.string().c_str());
    ImGui::TextDisabled("Diff:     %s", request.diffPath.string().c_str());

    ImGui::Separator();
    ImGui::SliderFloat("Zoom", &state.zoom, 1.0f, 32.0f, "%.0fx");
    ImGui::SameLine();
    ImGui::Checkbox("Show diff instead of actual", &state.showDiff);
    ImGui::SameLine();
    if (ImGui::Button("Accept / update baseline")) {
        actions.acceptRequested = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reject")) {
        actions.rejectRequested = true;
    }

    ImGui::Separator();
    const auto available = ImGui::GetContentRegionAvail();
    const auto paneWidth = (available.x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    ImGui::BeginChild("baseline-pane", ImVec2{paneWidth, 0.0f}, false);
    drawImagePixels("Baseline", images.baseline, state.zoom);
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("candidate-pane", ImVec2{0.0f, 0.0f}, false);
    drawImagePixels(state.showDiff ? "Diff" : "Actual",
                    state.showDiff ? images.diff : images.actual,
                    state.zoom);
    ImGui::EndChild();

    ImGui::End();
    return actions;
}

} // namespace Cory::Tools::VisualReview
