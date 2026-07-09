#include <Cory/Tools/VisualReviewUi.hpp>

#include <fmt/format.h>
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

struct KeyValueRow {
    const char *label;
    std::string value;
    std::string tooltip;
};

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

[[nodiscard]] std::string displayPathLabel(const std::filesystem::path &path)
{
    return path.filename().lexically_normal().generic_string();
}

template <size_t N>
void drawKeyValueTable(const char *id,
                       const char *title,
                       const std::array<KeyValueRow, N> &rows,
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
        for (const auto &[label, value, tooltip] : rows) {
            ImGui::TableNextRow();
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                   (rowIndex % 2 == 0) ? kRowBgA : kRowBgB);

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, toVec4(kLabelColor));
            ImGui::TextUnformatted(label);
            ImGui::PopStyleColor();

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, toVec4(kValueColor));
            ImGui::TextUnformatted(value.c_str());
            if (!tooltip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                ImGui::SetTooltip("%s", tooltip.c_str());
            }
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
        drawKeyValueTable(
            "request-table",
            "Request",
            std::array{
                KeyValueRow{"Case", request.caseName, {}},
                KeyValueRow{"Request ID", request.id, {}},
                KeyValueRow{"Request path",
                            displayPathLabel(request.requestPath),
                            std::filesystem::absolute(request.requestPath)
                                .lexically_normal()
                                .generic_string()},
                KeyValueRow{"Baseline path",
                            displayPathLabel(request.baselinePath),
                            std::filesystem::absolute(request.baselinePath)
                                .lexically_normal()
                                .generic_string()},
                KeyValueRow{"Actual path",
                            displayPathLabel(request.actualPath),
                            std::filesystem::absolute(request.actualPath)
                                .lexically_normal()
                                .generic_string()},
                KeyValueRow{"Diff path",
                            displayPathLabel(request.diffPath),
                            std::filesystem::absolute(request.diffPath)
                                .lexically_normal()
                                .generic_string()},
            },
            kTitleBlue);

        ImGui::TableNextColumn();
        drawKeyValueTable("source-table",
                          "Source location",
                          std::array{
                                            KeyValueRow{"Mismatched pixels",
                            fmt::format("{}", request.metrics.mismatchedPixels),
                            {}},
                KeyValueRow{"Mismatch ratio",
                            fmt::format("{:.4f}", request.metrics.mismatchRatio),
                            {}},
                KeyValueRow{"Max channel error",
                            fmt::format("{}", request.metrics.maxChannelError),
                            {}},
                KeyValueRow{"Mean absolute error",
                            fmt::format("{:.4f}", request.metrics.meanAbsoluteError),
                            {}},
                              KeyValueRow{"Catch test", request.metadata.catchTestName, {}},
                              KeyValueRow{"Source location",
                                          fmt::format("{}:{}",
                                                      displayPathLabel(request.metadata.sourceFile),
                                                      request.metadata.sourceLine),
                                          fmt::format("{}:{}",
                                                      std::filesystem::absolute(request.metadata.sourceFile)
                                                          .lexically_normal()
                                                          .generic_string(),
                                                      request.metadata.sourceLine)},
                              KeyValueRow{"Source function", request.metadata.sourceFunction, {}},
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
    ImGui::BeginChild("toolbar",
                      ImVec2{0.0f, kToolbarHeight},
                      true,
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

        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Zoom");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(170.0f);
        ImGui::SliderFloat("##zoom", &state.zoom, kZoomMin, kZoomMax, "%.0fx");
        ImGui::SameLine();
        if (ImGui::SmallButton(state.showDiff ? "Show actual" : "Show diff")) {
            state.showDiff = !state.showDiff;
        }

        const auto &io = ImGui::GetIO();
        const bool acceptShortcut = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false);
        const bool rejectShortcut = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false);

        ImGui::TableNextColumn();
        const auto buttonWidth = 94.0f;
        const auto totalWidth = buttonWidth * 2.0f + ImGui::GetStyle().ItemSpacing.x;
        const auto offset = std::max(0.0f, ImGui::GetContentRegionAvail().x - totalWidth);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

        const auto handleAccept = [&]() {
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
        };

        if (acceptShortcut || coloredButton("Accept", kAcceptColor, ImVec2{buttonWidth, 0.0f})) {
            handleAccept();
        }

        ImGui::SameLine();
        if (rejectShortcut || coloredButton("Reject", kRejectColor, ImVec2{buttonWidth, 0.0f})) {
            actions.rejectRequested = true;
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
}

[[nodiscard]] float panSpeedFromWheel(const VisualReviewUiState &state)
{
    return std::max(24.0f, 64.0f / std::max(state.zoom, 1.0f));
}

void applyWheelPan(VisualReviewUiState &state, const ImGuiIO &io)
{
    const auto speed = panSpeedFromWheel(state);
    if (io.MouseWheelH != 0.0f) {
        state.panX -= io.MouseWheelH * speed;
    }
    if (io.MouseWheel != 0.0f) {
        state.panY -= io.MouseWheel * speed;
    }
}

void applyWheelZoom(VisualReviewUiState &state,
                    const ImGuiIO &io,
                    const ImVec2 &canvasOrigin,
                    const ImVec2 &mousePos)
{
    const auto oldZoom = std::max(state.zoom, kZoomMin);
    const auto newZoom = std::clamp(oldZoom * std::pow(1.10f, io.MouseWheel), kZoomMin, kZoomMax);
    if (newZoom == oldZoom) {
        return;
    }

    const auto mouseLocal = ImVec2{mousePos.x - canvasOrigin.x, mousePos.y - canvasOrigin.y};
    const auto zoomRatio = newZoom / oldZoom;
    state.panX = (state.panX + mouseLocal.x) * zoomRatio - mouseLocal.x;
    state.panY = (state.panY + mouseLocal.y) * zoomRatio - mouseLocal.y;
    state.zoom = newZoom;
}

void drawImagePane(const char *id,
                   const char *title,
                   const ImageRgba8 *image,
                   ImGuiTextureId textureId,
                   VisualReviewUiState &state,
                   ImU32 titleColor)
{
    ImGui::BeginChild(id,
                      ImVec2{0.0f, 0.0f},
                      true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::PushStyleColor(ImGuiCol_Text, toVec4(titleColor));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();

    if (image == nullptr || textureId == InvalidImGuiTextureId) {
        ImGui::TextDisabled("Image unavailable");
        ImGui::EndChild();
        return;
    }

    ImGui::TextDisabled("%u x %u", image->width, image->height);

    const auto canvasOrigin = ImGui::GetCursorScreenPos();
    const auto pixelSize = std::max(1.0f, state.zoom);
    const auto canvasSize = ImVec2{static_cast<float>(image->width) * pixelSize,
                                   static_cast<float>(image->height) * pixelSize};
    ImGui::InvisibleButton(fmt::format("{}##canvas", id).c_str(),
                           canvasSize,
                           ImGuiButtonFlags_MouseButtonLeft);

    const auto &io = ImGui::GetIO();
    const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    if (hovered) {
        ImGui::SetMouseCursor(io.MouseDown[0] ? ImGuiMouseCursor_ResizeAll : ImGuiMouseCursor_Hand);

        if (io.KeyCtrl && io.MouseWheel != 0.0f) {
            applyWheelZoom(state, io, canvasOrigin, io.MousePos);
        } else {
            applyWheelPan(state, io);
        }

        if (io.MouseDown[0] && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f)) {
            state.panX -= io.MouseDelta.x;
            state.panY -= io.MouseDelta.y;
        }
    }

    auto *drawList = ImGui::GetWindowDrawList();
    const auto drawPixelSize = std::max(1.0f, state.zoom);
    const auto drawMin = ImVec2{canvasOrigin.x - state.panX, canvasOrigin.y - state.panY};
    const auto drawMax = ImVec2{drawMin.x + static_cast<float>(image->width) * drawPixelSize,
                                drawMin.y + static_cast<float>(image->height) * drawPixelSize};
    drawList->AddImage(static_cast<ImTextureID>(textureId), drawMin, drawMax);

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
    auto windowBg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    windowBg.w = 1.0f;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, windowBg);
    if (!ImGui::Begin("Visual Review",
                      nullptr,
                      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoMove)) {
        ImGui::End();
        ImGui::PopStyleColor();
        return actions;
    }

    drawMetadataTables(request);
    ImGui::Spacing();

    const auto &io = ImGui::GetIO();
    const bool effectiveShowDiff = state.showDiff ^ io.KeyAlt;

    if (ImGui::BeginChild("preview-area", ImVec2{0.0f, -kToolbarHeight}, false)) {
        if (ImGui::BeginTable("preview-table",
                              2,
                              ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX |
                                  ImGuiTableFlags_NoPadInnerX)) {
            ImGui::TableSetupColumn("baseline", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("candidate", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            drawImagePane("baseline-pane",
                          "Baseline",
                          images.baseline,
                          images.baselineTexture,
                          state,
                          kTitleBlue);

            ImGui::TableNextColumn();
            drawImagePane("candidate-pane",
                          effectiveShowDiff ? "Diff" : "Actual",
                          effectiveShowDiff ? images.diff : images.actual,
                          effectiveShowDiff ? images.diffTexture : images.actualTexture,
                          state,
                          kTitleYellow);

            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    ImGui::Spacing();
    drawToolbar(request, images, state, actions);

    ImGui::End();
    ImGui::PopStyleColor();
    return actions;
}

} // namespace Cory::Tools::VisualReview
