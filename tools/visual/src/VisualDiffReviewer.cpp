#include <Cory/Tools/VisualReviewProtocol.hpp>
#include <Cory/Tools/VisualReviewUi.hpp>

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
#include <gsl/narrow>
#include <spdlog/spdlog.h>

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
        const auto actions = Cory::Tools::VisualReview::drawReviewUi(
            request_,
            Cory::Tools::VisualReview::VisualReviewUiImages{.baseline =
                                                                baseline_ ? &*baseline_ : nullptr,
                                                            .actual = actual_ ? &*actual_ : nullptr,
                                                            .diff = diff_ ? &*diff_ : nullptr},
            uiState_);
        if (actions.acceptRequested) {
            writeDecisionAndClose(true, "accepted in VisualDiffReviewer");
        }
        if (actions.rejectRequested) {
            writeDecisionAndClose(false, "rejected in VisualDiffReviewer");
        }
    }

    VisualReviewRequest request_;
    Cory::Result<ImageRgba8> baseline_;
    Cory::Result<ImageRgba8> actual_;
    Cory::Result<ImageRgba8> diff_;
    std::unique_ptr<Cory::Window> window_;
    uint64_t framesToRender_{0};
    Cory::Tools::VisualReview::VisualReviewUiState uiState_{};
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
