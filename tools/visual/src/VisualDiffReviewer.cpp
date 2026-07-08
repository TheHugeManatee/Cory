#include <Cory/Tools/VisualReviewProtocol.hpp>

#include <Cory/Application/Application.hpp>
#include <Cory/Application/ImGuiLayer.hpp>
#include <Cory/Application/LayerStack.hpp>
#include <Cory/Application/Window.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Cory.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/IO/Bmp.hpp>
#include <Cory/RenderTasks/StandardRenderTasks.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/FrameContext.hpp>

#include <CLI/CLI.hpp>
#include <GLFW/glfw3.h>
#include <fmt/format.h>
#include <glm/vec2.hpp>
#include <gsl/narrow>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Cory::Tools::VisualReview::VisualReviewDecision;
using Cory::Tools::VisualReview::VisualReviewRequest;

using ImageRgba8 = Cory::IO::BmpImageRgba8;

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

    if (pixelSize <= 1.5f) {
        // At 1x, drawing one rect per pixel is still adequate for the small test artifacts this
        // reviewer targets today. For large artifacts the clip-restricted loop keeps work bounded.
    }
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

class VisualDiffReviewerApplication : public Cory::Application {
  public:
    VisualDiffReviewerApplication(std::span<const char *> args, VisualReviewRequest request)
        : request_{std::move(request)}
    {
        CLI::App app{"Cory visual diff reviewer"};
        bool disableValidation{false};
        app.add_option("-f,--frames", framesToRender_, "The maximum number of frames to render");
        app.add_flag("--disable-validation", disableValidation, "Disable validation layers");
        app.allow_config_extras(true);
        app.parse(gsl::narrow<int>(args.size()), args.data());

        baseline_ = Cory::IO::loadBmpRgba8(request_.baselinePath);
        actual_ = Cory::IO::loadBmpRgba8(request_.actualPath);
        diff_ = Cory::IO::loadBmpRgba8(request_.diffPath);

        init(Cory::ContextCreationInfo{
            .validation = disableValidation ? Cory::ValidationLayers::Disabled
                                            : Cory::ValidationLayers::Enabled,
            .args = args,
        });

        window_ = std::make_unique<Cory::Window>(
            ctx(), glm::i32vec2{1500, 900}, "Cory Visual Diff Reviewer", 1);
        const auto viewportDimensions = glm::i32vec2(window_->extent());
        Cory::LayerAttachInfo layerAttachInfo{.maxFramesInFlight = Cory::MAX_FRAMES_IN_FLIGHT,
                                              .viewportDimensions = viewportDimensions};
        layers().emplacePriorityLayer<Cory::ImGuiLayer>(layerAttachInfo, std::ref(*window_));
        layers().connectToWindow(*window_);
    }

    [[nodiscard]] bool decisionWritten() const { return decisionWritten_; }

    void run() override
    {
        auto framegraphs = createFramegraphs();
        runMainLoop(
            *window_,
            framesToRender_,
            {},
            [this, &framegraphs](Cory::FrameContext &frameCtx, const Cory::LogicUpdateContext &) {
                auto recordedFrame = recordFramegraph(
                    framegraphs,
                    frameCtx,
                    [this](Cory::Framegraph &fg, const Cory::FrameContext &currentFrame) {
                        defineRenderPasses(fg, currentFrame);
                    });
                (void)recordedFrame;
            },
            [this](Cory::FrameContext &, const Cory::LogicUpdateContext &) { drawUi(); });
    }

  private:
    void defineRenderPasses(Cory::Framegraph &framegraph, const Cory::FrameContext &frameCtx)
    {
        auto frameHandles = framegraph.importFrameContext(frameCtx);
        auto clear = Cory::StandardRenderTasks::clearAttachments(
            framegraph.declareTask("TASK_ClearReviewer"),
            frameHandles.colorImage,
            frameHandles.depthImage,
            Gpu::ColorClearValue{0.02f, 0.02f, 0.025f, 1.0f});

        auto layersOutput = layers().declareRenderTasks(
            framegraph, {.color = clear.output().color, .depth = *clear.output().depth});

        auto copiedSwapchain = Cory::StandardRenderTasks::copyToTarget(
                                   framegraph.declareTask("TASK_CopyReviewerToTarget"),
                                   layersOutput.color,
                                   frameHandles.swapchainImage)
                                   .output();
        framegraph.declareOutput(copiedSwapchain, Cory::Sync::AccessType::Present);
    }

    void writeDecisionAndClose(bool accepted, std::string_view note)
    {
        Cory::Tools::VisualReview::writeDecision(request_.decisionPath,
                                                 VisualReviewDecision{.requestId = request_.id,
                                                                      .accepted = accepted,
                                                                      .note = std::string{note}});
        decisionWritten_ = true;
        glfwSetWindowShouldClose(window_->getGlfwWindow().get(), GLFW_TRUE);
    }

    void drawUi()
    {
        ImGui::SetNextWindowPos(ImVec2{0.0f, 0.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
        if (!ImGui::Begin("Visual Review",
                          nullptr,
                          ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                              ImGuiWindowFlags_NoMove)) {
            ImGui::End();
            return;
        }

        ImGui::Text("Case: %s", request_.caseName.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("Request: %s", request_.id.c_str());
        ImGui::Text("Catch test: %s", request_.metadata.catchTestName.c_str());
        ImGui::Text("Source: %s:%llu",
                    request_.metadata.sourceFile.c_str(),
                    static_cast<unsigned long long>(request_.metadata.sourceLine));
        ImGui::Text("Mismatched pixels: %llu (%.6f), max channel error: %u, MAE: %.4f",
                    static_cast<unsigned long long>(request_.metrics.mismatchedPixels),
                    request_.metrics.mismatchRatio,
                    static_cast<unsigned>(request_.metrics.maxChannelError),
                    request_.metrics.meanAbsoluteError);
        ImGui::TextDisabled("Baseline: %s", request_.baselinePath.string().c_str());
        ImGui::TextDisabled("Actual:   %s", request_.actualPath.string().c_str());
        ImGui::TextDisabled("Diff:     %s", request_.diffPath.string().c_str());

        ImGui::Separator();
        ImGui::SliderFloat("Zoom", &zoom_, 1.0f, 32.0f, "%.0fx");
        ImGui::SameLine();
        ImGui::Checkbox("Show diff instead of actual", &showDiff_);
        ImGui::SameLine();
        if (ImGui::Button("Accept / update baseline")) {
            writeDecisionAndClose(true, "accepted in VisualDiffReviewer");
        }
        ImGui::SameLine();
        if (ImGui::Button("Reject")) {
            writeDecisionAndClose(false, "rejected in VisualDiffReviewer");
        }

        ImGui::Separator();
        const auto available = ImGui::GetContentRegionAvail();
        const auto paneWidth = (available.x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        ImGui::BeginChild("baseline-pane", ImVec2{paneWidth, 0.0f}, false);
        drawImagePixels("Baseline", baseline_ ? &*baseline_ : nullptr, zoom_);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("candidate-pane", ImVec2{0.0f, 0.0f}, false);
        drawImagePixels(showDiff_ ? "Diff" : "Actual",
                        showDiff_ ? (diff_ ? &*diff_ : nullptr) : (actual_ ? &*actual_ : nullptr),
                        zoom_);
        ImGui::EndChild();

        ImGui::End();
    }

    VisualReviewRequest request_;
    Cory::Result<ImageRgba8> baseline_;
    Cory::Result<ImageRgba8> actual_;
    Cory::Result<ImageRgba8> diff_;
    std::unique_ptr<Cory::Window> window_;
    uint64_t framesToRender_{0};
    float zoom_{8.0f};
    bool showDiff_{false};
    bool decisionWritten_{false};
};

} // namespace

int main(int argc, char **argv)
{
    CLI::App app{"Cory visual diff reviewer"};

    std::filesystem::path requestPath;
    bool autoAccept{false};
    bool autoReject{false};

    app.add_option("--request", requestPath, "Path to a visual review request JSON")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_flag("--auto-accept", autoAccept, "Accept the request without opening UI");
    app.add_flag("--auto-reject", autoReject, "Reject the request without opening UI");
    app.allow_config_extras(true);

    try {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError &e) {
        return app.exit(e);
    }

    if (autoAccept && autoReject) {
        std::cerr << "Use only one of --auto-accept or --auto-reject.\n";
        return EXIT_FAILURE;
    }

    auto request = Cory::Tools::VisualReview::readRequest(requestPath);
    if (!request) {
        std::cerr << "Failed to read visual review request: " << requestPath << "\n";
        return EXIT_FAILURE;
    }

    if (autoAccept || autoReject) {
        Cory::Tools::VisualReview::writeDecision(
            request->decisionPath,
            VisualReviewDecision{
                .requestId = request->id,
                .accepted = autoAccept,
                .note = autoAccept ? "auto-accepted" : "auto-rejected",
            });
        return EXIT_SUCCESS;
    }

    try {
        Cory::Init();
        std::vector<const char *> appArgs;
        appArgs.reserve(static_cast<size_t>(argc));
        for (int i = 0; i < argc; ++i) {
            appArgs.push_back(argv[i]);
        }
        VisualDiffReviewerApplication reviewer{std::span{appArgs}, std::move(*request)};
        reviewer.run();
        return reviewer.decisionWritten() ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    catch (const std::exception &e) {
        spdlog::critical("Uncaught exception on main thread: {}", e.what());
        return EXIT_FAILURE;
    }
}
