#include "TestUtils.hpp"
#include "VisualTestUtils.hpp"

#include <catch2/catch_test_macros.hpp>

#include <Cory/RenderTasks/StandardRenderTasks.hpp>

#include <filesystem>

#include <gsl/util>

TEST_CASE("TestCanvas captures clear color", "[visual][TestCanvas]")
{
    Cory::testing::VulkanTester tester;
    Cory::testing::TestCanvas canvas{tester.ctx(), glm::u32vec2{32, 32}};

    auto actual = canvas.render([](Cory::testing::TestFrame &frame) {
        auto clear = Cory::StandardRenderTasks::clearAttachments(
            frame.graph.declareTask("TASK_ClearRed"),
            frame.color,
            frame.depth,
            Gpu::ColorClearValue{1.0f, 0.0f, 0.0f, 1.0f});
        return clear.output().color;
    });

    Cory::testing::requireMatchesReference("testcanvas-clear-red", actual);
}

TEST_CASE("Visual BMP helpers round-trip RGBA images", "[visual][TestCanvas][IO]")
{
    const auto image = Cory::testing::makeSolidImage(glm::u32vec2{4, 3}, 12, 34, 56, 255);
    const auto path = std::filesystem::temp_directory_path() / "cory-visual-bmp-roundtrip.bmp";

    Cory::testing::writeBmp(path, image);
    auto decoded = Cory::testing::readBmp(path);

    REQUIRE(decoded);
    CHECK(decoded->size == image.size);
    CHECK(decoded->pixels == image.pixels);
}

TEST_CASE("Visual comparison reports mismatch for non-matching reference",
          "[visual][TestCanvas][comparison]")
{
    const auto tempRoot =
        std::filesystem::temp_directory_path() / "cory-visual-comparison-mismatch";
    const auto baselineRoot = tempRoot / "baselines";
    const auto artifactRoot = tempRoot / "artifacts";
    std::filesystem::remove_all(tempRoot);
    std::filesystem::create_directories(baselineRoot);

    const auto baseline = Cory::testing::makeSolidImage(glm::u32vec2{2, 2}, 255, 0, 0, 255);
    const auto actual = Cory::testing::makeSolidImage(glm::u32vec2{2, 2}, 0, 255, 0, 255);
    Cory::testing::writeBmp(baselineRoot / "comparison-mismatch.bmp", baseline);

    _putenv_s("CORY_VISUAL_BASELINE_DIR", baselineRoot.string().c_str());
    _putenv_s("CORY_VISUAL_ARTIFACT_DIR", artifactRoot.string().c_str());
    auto cleanup = gsl::finally([] {
        _putenv_s("CORY_VISUAL_BASELINE_DIR", "");
        _putenv_s("CORY_VISUAL_ARTIFACT_DIR", "");
    });

    const auto result = Cory::testing::compareToReference("comparison-mismatch", actual);

    CHECK_FALSE(result.passed);
    CHECK(result.mismatchedPixels == 4);
    CHECK(result.maxChannelError == 255);
    CHECK(result.meanAbsoluteError > 0.0);
    CHECK(std::filesystem::exists(result.actualPath));
    CHECK(std::filesystem::exists(result.diffPath));
    CHECK(std::filesystem::exists(result.metricsPath));
}
