#include <Cory/Testing/TestUtils.hpp>
#include <Cory/Testing/VisualTestUtils.hpp>

#include <catch2/catch_test_macros.hpp>

#include <Cory/RenderTasks/StandardRenderTasks.hpp>
#include <Cory/Tools/VisualReviewProtocol.hpp>
#include <Cory/Tools/VisualReviewUi.hpp>

#include <filesystem>
#include <fstream>

namespace {

[[nodiscard]] Cory::IO::BmpImageRgba8 toBmpImage(const Cory::testing::ImageRgba8 &image)
{
    return Cory::IO::BmpImageRgba8{
        .width = image.size.x, .height = image.size.y, .pixelsRgba8 = image.pixels};
}

[[nodiscard]] std::filesystem::path makeFixtureRoot()
{
    auto root = std::filesystem::temp_directory_path() / "Cory" / "VisualDiffReviewerUi_Test" /
                "visual-review-ui-fixed-request";
    std::filesystem::create_directories(root);
    return root;
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

    const auto fixtureRoot = makeFixtureRoot();
    const auto baselineFixturePath = fixtureRoot / "baseline.bmp";
    const auto actualFixturePath = fixtureRoot / "actual.bmp";
    const auto diffFixturePath = fixtureRoot / "diff.bmp";
    const auto metricsFixturePath = fixtureRoot / "metrics.json";
    const auto requestFixturePath = fixtureRoot / "request.json";
    const auto decisionFixturePath = fixtureRoot / "decision.json";

    const auto request = VisualReviewRequest{
        .id = "visual-review-ui-fixed-request",
        .caseName = "reviewer-ui-regression",
        .metadata =
            VisualReviewMetadata{.catchTestName = "VisualDiffReviewer UI matches reference",
                                 .sourceFile = "tools/visual/tests/VisualDiffReviewerUi_Test.cpp",
                                 .sourceLine = 49,
                                 .sourceFunction = "ui regression"},
        .baselinePath = "visual-review-fixture/baseline.bmp",
        .actualPath = "visual-review-fixture/actual.bmp",
        .diffPath = "visual-review-fixture/diff.bmp",
        .metricsPath = "visual-review-fixture/metrics.json",
        .requestPath = "visual-review-fixture/request.json",
        .decisionPath = "visual-review-fixture/decision.json",
        .metrics = VisualReviewMetrics{.mismatchedPixels = 64,
                                       .mismatchRatio = 1.0,
                                       .maxChannelError = 255,
                                       .meanAbsoluteError = 127.5},
    };

    REQUIRE(Cory::IO::writeBmpRgba8(baselineFixturePath, baselineBmp));
    REQUIRE(Cory::IO::writeBmpRgba8(actualFixturePath, actualBmp));
    REQUIRE(Cory::IO::writeBmpRgba8(diffFixturePath, diffBmp));

    writeRequest(requestFixturePath, request);

    {
        std::ofstream metrics{metricsFixturePath, std::ios::binary | std::ios::trunc};
        REQUIRE(metrics.is_open());
        metrics << "{\n"
                << "  \"mismatchedPixels\": " << request.metrics.mismatchedPixels << ",\n"
                << "  \"mismatchRatio\": " << request.metrics.mismatchRatio << ",\n"
                << "  \"maxChannelError\": " << static_cast<int>(request.metrics.maxChannelError)
                << ",\n"
                << "  \"meanAbsoluteError\": " << request.metrics.meanAbsoluteError << "\n"
                << "}\n";
    }

    {
        std::ofstream decision{decisionFixturePath, std::ios::binary | std::ios::trunc};
        REQUIRE(decision.is_open());
        decision << "{}\n";
    }

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
