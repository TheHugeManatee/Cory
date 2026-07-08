#include "TestUtils.hpp"
#include "VisualTestUtils.hpp"

#include <catch2/catch_test_macros.hpp>

#include <Cory/RenderTasks/StandardRenderTasks.hpp>

#include <filesystem>

#include <gsl/util>

namespace {

[[nodiscard]] std::filesystem::path testScratchRoot(std::string_view name)
{
    return std::filesystem::path{CORY_TEST_RUNTIME_DIR} / "visual-test-scratch" /
           std::filesystem::path{name};
}

} // namespace

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

    const auto result = Cory::testing::compareToReference("testcanvas-clear-red", actual);

    CHECK(Cory::testing::visualMatch(result));
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
    const auto scratchRoot = testScratchRoot("comparison-mismatch");
    const auto baselinePath = scratchRoot / "baselines" / "comparison-mismatch.bmp";
    const auto artifactRoot = scratchRoot / "artifacts";
    std::filesystem::remove_all(scratchRoot);
    std::filesystem::create_directories(baselinePath.parent_path());

    const auto baseline = Cory::testing::makeSolidImage(glm::u32vec2{2, 2}, 255, 0, 0, 255);
    const auto actual = Cory::testing::makeSolidImage(glm::u32vec2{2, 2}, 0, 255, 0, 255);
    Cory::testing::writeBmp(baselinePath, baseline);

    const auto result = Cory::testing::compareToReference("comparison-mismatch",
                                                          actual,
                                                          Cory::testing::ImageCompareOptions{
                                                              .baselinePathOverride = baselinePath,
                                                              .artifactRootOverride = artifactRoot,
                                                          });

    CHECK_FALSE(Cory::testing::visualMatch(result));
}

TEST_CASE("Interactive visual comparison fails closed when reviewer cannot launch",
          "[visual][TestCanvas][comparison]")
{
    const auto scratchRoot = testScratchRoot("review-fail-closed");
    const auto baselinePath = scratchRoot / "baselines" / "review-fail-closed.bmp";
    const auto artifactRoot = scratchRoot / "artifacts";
    std::filesystem::remove_all(scratchRoot);
    std::filesystem::create_directories(baselinePath.parent_path());

    const auto baseline = Cory::testing::makeSolidImage(glm::u32vec2{2, 2}, 255, 0, 0, 255);
    const auto actual = Cory::testing::makeSolidImage(glm::u32vec2{2, 2}, 0, 255, 0, 255);
    Cory::testing::writeBmp(baselinePath, baseline);

    _putenv_s("CORY_VISUAL_INTERACTIVE", "1");
    auto cleanup = gsl::finally([] { _putenv_s("CORY_VISUAL_INTERACTIVE", ""); });

    const auto result = Cory::testing::compareToReference(
        "review-fail-closed",
        actual,
        Cory::testing::ImageCompareOptions{
            .baselinePathOverride = baselinePath,
            .artifactRootOverride = artifactRoot,
            .reviewerExecutableOverride = scratchRoot / "missing-reviewer.exe",
        });

    CHECK_FALSE(Cory::testing::visualMatch(result));
}

TEST_CASE("Interactive visual comparison can accept and update baseline",
          "[visual][TestCanvas][comparison]")
{
    const auto scratchRoot = testScratchRoot("review-auto-accept");
    const auto baselinePath = scratchRoot / "baselines" / "review-auto-accept.bmp";
    const auto artifactRoot = scratchRoot / "artifacts";
    std::filesystem::remove_all(scratchRoot);
    std::filesystem::create_directories(baselinePath.parent_path());

    const auto baseline = Cory::testing::makeSolidImage(glm::u32vec2{2, 2}, 255, 0, 0, 255);
    const auto actual = Cory::testing::makeSolidImage(glm::u32vec2{2, 2}, 0, 255, 0, 255);
    Cory::testing::writeBmp(baselinePath, baseline);

    _putenv_s("CORY_VISUAL_INTERACTIVE", "1");
    auto cleanup = gsl::finally([] { _putenv_s("CORY_VISUAL_INTERACTIVE", ""); });

    const auto result =
        Cory::testing::compareToReference("review-auto-accept",
                                          actual,
                                          Cory::testing::ImageCompareOptions{
                                              .baselinePathOverride = baselinePath,
                                              .artifactRootOverride = artifactRoot,
                                              .reviewerArguments = {"--auto-accept"},
                                          });
    const auto updatedBaseline = Cory::testing::readBmp(baselinePath);

    CHECK(Cory::testing::visualMatch(result));
    REQUIRE(updatedBaseline);
    CHECK(updatedBaseline->pixels == actual.pixels);
}
