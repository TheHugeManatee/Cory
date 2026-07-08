#include <Cory/Tools/VisualReviewUi.hpp>

#include <fmt/format.h>
#include <gsl/narrow>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

#include <Cory/Base/Log.hpp>

namespace Cory::Tools::VisualReview {
namespace {

using ImageRgba8 = IO::BmpImageRgba8;

constexpr float kMetadataHeight = 300.0f;
constexpr float kToolbarHeight = 56.0f;
constexpr float kZoomMin = 1.0f;
constexpr float kZoomMax = 64.0f;
constexpr ImU32 kAcceptColor = IM_COL32(56, 196, 102, 255);
constexpr ImU32 kRejectColor = IM_COL32(226, 74, 62, 255);
constexpr ImU32 kTitleBlue = IM_COL32(122, 181, 255, 255);
constexpr ImU32 kTitleYellow = IM_COL32(240, 208, 100, 255);
constexpr ImU32 kRowBgA = IM_COL32(24, 28, 38, 255);
constexpr ImU32 kRowBgB = IM_COL32(18, 22, 30, 255);
constexpr ImU32 kLabelColor = IM_COL32(165, 183, 214, 255);
constexpr ImU32 kValueColor = IM_COL32(236, 240, 245, 255);

[[nodiscard]] ImVec4 toVec4(ImU32 color)
{
    return ImGui::ColorConvertU32ToFloat4(color);
}

[[nodiscard]] bool coloredButton(const char *label, ImU32 color, ImVec2 size)
{
    const auto base = toVec4(color);
    const auto hovered = ImVec4{std::min(base.x + 0.10f, 1.0f),
                                std::min(base.y + 0.10f, 1.0f),
                                std::min(base.z + 0.10f, 1.0f),
                                1.0f};
    const auto active = ImVec4{std::max(base.x - 0.08f, 0.0f),
                               std::max(base.y - 0.08f, 0.0f),
                               std::max(base.z - 0.08f, 0.0f),
                               1.0f};

    ImGui::PushStyleColor(ImGuiCol_Button, base);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, active);
    const auto clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return clicked;
}

[[nodiscard]] ImU32 pixelColor(const ImageRgba8 &image, uint32_t x, uint32_t y)
{
    const auto offset = (static_cast<size_t>(y) * static_cast<size_t>(image.width) +
                         static_cast<size_t>(x)) *
                        4U;
    return IM_COL32(static_cast<uint8_t>(image.pixelsRgba8[offset + 0]),
                    static_cast<uint8_t>(image.pixelsRgba8[offset + 1]),
                    static_cast<uint8_t>(image.pixelsRgba8[offset + 2]),
                    static_cast<uint8_t>(image.pixelsRgba8[offset + 3]));
}

void drawImagePixels(const char *title, const ImageRgba8 *image, float zoom, ImU32 titleColor)
{
    ImGui::PushStyleColor(ImGuiCol_Text, toVec4(titleColor));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();

    if (image == nullptr) {
        ImGui::TextDisabled("Image unavailable");
        return;
    }

    ImGui::TextDisabled("%u x %u", image->width, image->height);
    const auto origin = ImGui::GetCursorScreenPos();
    const auto pixelSize = std::max(1.0f, zoom);
    const auto canvasSize = ImVec2{static_cast<float>(image->width) * pixelSize,
                                   static_cast<float>(image->height) * pixelSize};
    ImGui::InvisibleButton(fmt::format("{}-canvas", title).c_str(), canvasSize);

    auto *drawList = ImGui::GetWindowDrawList();
    const auto clipMin = ImGui::GetWindowPos();
    const auto clipMax = ImVec2{clipMin.x + ImGui::GetWindowWidth(),
                                clipMin.y + ImGui::GetWindowHeight()};
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
}

[[nodiscard]] std::string displayPath(const std::filesystem::path &path)
{
    return std::filesystem::absolute(path).lexically_normal().generic_string();
}

[[nodiscard]] std::string displaySourceLocation(const std::string &sourceFile, uint64_t line)
{
    return fmt::format("{}:{}", displayPath(sourceFile), line);
}

template <size_t N>
void drawKeyValueTable(const char *id,
                       const char *title,
                       const std::array<std::pair<const char *, std::string>, N> &rows,
                       ImU32 titleColor)
{
    ImGui::PushStyleColor(ImGuiCol_Text, toVec4(titleColor));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    ImGui::Separator();

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2{8.0f, 3.0f});
    if (ImGui::BeginTable(id,
                          2,
                          ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 152.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableHeadersRow();

        auto rowIndex = 0;
        for (const auto &[label, value] : rows) {
            ImGui::TableNextRow();
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                   (rowIndex % 2 == 0) ? kRowBgA : kRowBgB);

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, toVec4(kLabelColor));
            ImGui::TextUnformatted(label);
            ImGui::PopStyleColor();

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, toVec4(kValueColor));
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
            ImGui::TextUnformatted(value.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();

            ++rowIndex;
        }

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

void drawMetadataTables(const VisualReviewRequest &request)
{
    ImGui::BeginChild("metadata",
                      ImVec2{0.0f, kMetadataHeight},
                      true,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar |
                          ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::PushStyleColor(ImGuiCol_Text, toVec4(kTitleBlue));
    ImGui::TextUnformatted("Review metadata");
    ImGui::PopStyleColor();
    ImGui::Separator();

    if (ImGui::BeginTable("metadata-columns",
                          2,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX |
                              ImGuiTableFlags_NoPadInnerX)) {
        ImGui::TableSetupColumn("request", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("source", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        drawKeyValueTable("request-table",
                          "Request",
                          std::array{
                              std::pair{"Case", request.caseName},
                              std::pair{"Request ID", request.id},
                              std::pair{"Request path", displayPath(request.requestPath)},
                              std::pair{"Baseline path", displayPath(request.baselinePath)},
                              std::pair{"Actual path", displayPath(request.actualPath)},
                              std::pair{"Diff path", displayPath(request.diffPath)},
                              std::pair{"Mismatched pixels", fmt::format("{}", request.metrics.mismatchedPixels)},
                              std::pair{"Mismatch ratio", fmt::format("{:.4f}", request.metrics.mismatchRatio)},
                              std::pair{"Max channel error", fmt::format("{}", request.metrics.maxChannelError)},
                              std::pair{"Mean absolute error", fmt::format("{:.4f}", request.metrics.meanAbsoluteError)},
                          },
                          kTitleBlue);

        ImGui::TableNextColumn();
        drawKeyValueTable("source-table",
                          "Source location",
                          std::array{
                              std::pair{"Catch test", request.metadata.catchTestName},
                              std::pair{"Source location",
                                        displaySourceLocation(request.metadata.sourceFile,
                                                              request.metadata.sourceLine)},
                              std::pair{"Source function", request.metadata.sourceFunction},
                          },
                          kTitleYellow);

        ImGui::EndTable();
    }

    ImGui::EndChild();
}

void drawToolbar(const VisualReviewRequest &request,
                 const VisualReviewUiImages &images,
                 VisualReviewUiState &state,
                 VisualReviewUiActions &actions)
{
    ImGui::BeginChild("toolbar", ImVec2{0.0f, kToolbarHeight}, true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (ImGui::BeginTable("toolbar-table",
                          2,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings |
                              ImGuiTableFlags_NoPadOuterX)) {
        ImGui::TableSetupColumn("controls", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("actions",
                                ImGuiTableColumnFlags_WidthFixed,
                                2.0f * 94.0f + ImGui::GetStyle().ItemSpacing.x);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Zoom");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(170.0f);
        ImGui::SliderFloat("##zoom", &state.zoom, kZoomMin, kZoomMax, "%.0fx");
        ImGui::SameLine();
        if (ImGui::SmallButton(state.showDiff ? "Show actual" : "Show diff")) {
            state.showDiff = !state.showDiff;
        }

        ImGui::TableSetColumnIndex(1);
        const auto buttonWidth = 94.0f;
        const auto totalWidth = buttonWidth * 2.0f + ImGui::GetStyle().ItemSpacing.x;
        const auto offset = std::max(0.0f, ImGui::GetContentRegionAvail().x - totalWidth);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

        if (coloredButton("Accept", kAcceptColor, ImVec2{buttonWidth, 0.0f})) {
            if (images.actual != nullptr) {
                if (const auto result = IO::writeBmpRgba8(request.baselinePath, *images.actual);
                    result) {
                    actions.acceptRequested = true;
                } else {
                    CO_CORE_ERROR("Failed to update baseline image '{}': {}",
                                  request.baselinePath.string(),
                                  result.error());
                }
            } else {
                CO_CORE_ERROR("Cannot accept visual review '{}': actual image is missing",
                              request.id);
            }
        }

        ImGui::SameLine();
        if (coloredButton("Reject", kRejectColor, ImVec2{buttonWidth, 0.0f})) {
            actions.rejectRequested = true;
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
}

void drawPreviewPane(const char *id,
                     const char *title,
                     const ImageRgba8 *image,
                     float zoom,
                     ImU32 titleColor)
{
    ImGui::BeginChild(id,
                      ImVec2{0.0f, 0.0f},
                      true,
                      ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoMove);
    drawImagePixels(title, image, zoom, titleColor);
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

    drawMetadataTables(request);
    ImGui::Spacing();

    if (ImGui::BeginChild("preview-area", ImVec2{0.0f, -kToolbarHeight}, false)) {
        if (ImGui::BeginTable("preview-table",
                              2,
                              ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX |
                                  ImGuiTableFlags_NoPadInnerX)) {
            ImGui::TableSetupColumn("baseline", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("candidate", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            drawPreviewPane("baseline-pane", "Baseline", images.baseline, state.zoom, kTitleBlue);

            ImGui::TableSetColumnIndex(1);
            drawPreviewPane("candidate-pane",
                            state.showDiff ? "Diff" : "Actual",
                            state.showDiff ? images.diff : images.actual,
                            state.zoom,
                            kTitleYellow);

            ImGui::EndTable();
        }

        const auto &io = ImGui::GetIO();
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) && io.MouseWheel != 0.0f) {
            state.zoom = std::clamp(state.zoom * std::pow(1.10f, io.MouseWheel),
                                    kZoomMin,
                                    kZoomMax);
        }
    }
    ImGui::EndChild();

    ImGui::Spacing();
    drawToolbar(request, images, state, actions);

    ImGui::End();
    return actions;
}

} // namespace Cory::Tools::VisualReview
