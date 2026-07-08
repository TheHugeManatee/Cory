#include <Cory/Testing/TestUtils.hpp>
#include <Cory/Testing/VisualTestUtils.hpp>

#include <catch2/catch_test_macros.hpp>

#include <Cory/RenderTasks/StandardRenderTasks.hpp>
#include <Cory/Tools/VisualReviewProtocol.hpp>
#include <Cory/Tools/VisualReviewUi.hpp>

namespace {

[[nodiscard]] Cory::IO::BmpImageRgba8 toBmpImage(const Cory::testing::ImageRgba8 &image)
{
    return Cory::IO::BmpImageRgba8{
        .width = image.size.x, .height = image.size.y, .pixelsRgba8 = image.pixels};
}

} // namespace

TEST_CASE("VisualDiffReviewer UI matches reference", "[visual][VisualDiffReviewer][ui]")
{
    using namespace Cory::Tools::VisualReview;

    Cory::testing::VulkanTester tester;
    Cory::testing::TestCanvas canvas{tester.ctx(), glm::u32vec2{1500, 900}};
    Cory::testing::ImGuiTestRenderer imgui{tester.ctx(), canvas.size()};

    const auto baselineImage = Cory::testing::makeSolidImage(glm::u32vec2{8, 8}, 255, 0, 0, 255);
    const auto actualImage = Cory::testing::makeSolidImage(glm::u32vec2{8, 8}, 0, 255, 0, 255);
    const auto diffImage = Cory::testing::makeSolidImage(glm::u32vec2{8, 8}, 255, 255, 0, 255);

    const auto baselineBmp = toBmpImage(baselineImage);
    const auto actualBmp = toBmpImage(actualImage);
    const auto diffBmp = toBmpImage(diffImage);

    const auto request = VisualReviewRequest{
        .id = "visual-review-ui-fixed-request",
        .caseName = "reviewer-ui-regression",
        .metadata =
            VisualReviewMetadata{.catchTestName = "VisualDiffReviewer UI matches reference",
                                 .sourceFile = "tools/visual/tests/VisualDiffReviewerUi_Test.cpp",
                                 .sourceLine = 0,
                                 .sourceFunction = "ui regression"},
        .baselinePath = "baseline.bmp",
        .actualPath = "actual.bmp",
        .diffPath = "diff.bmp",
        .metricsPath = "metrics.json",
        .requestPath = "request.json",
        .decisionPath = "decision.json",
        .metrics = VisualReviewMetrics{.mismatchedPixels = 64,
                                       .mismatchRatio = 1.0,
                                       .maxChannelError = 255,
                                       .meanAbsoluteError = 127.5},
    };

    auto uiState = VisualReviewUiState{};
    const auto actual = canvas.render([&](Cory::testing::TestFrame &frame) {
        auto clear = Cory::StandardRenderTasks::clearAttachments(
            frame.graph.declareTask("TASK_ClearReviewerUiTest"),
            frame.color,
            frame.depth,
            Gpu::ColorClearValue{0.02f, 0.02f, 0.025f, 1.0f});
        return imgui
            .render(frame.graph.declareTask("TASK_DrawReviewerUi"),
                    clear.output().color,
                    clear.output().depth,
                    [&] {
                        (void)drawReviewUi(request,
                                           VisualReviewUiImages{.baseline = &baselineBmp,
                                                                .actual = &actualBmp,
                                                                .diff = &diffBmp},
                                           uiState);
                    })
            .output();
    });

    Cory::testing::requireMatchesReference("visual-diff-reviewer-ui", actual);
}
