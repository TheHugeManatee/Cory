#include <Cory/Testing/TestUtils.hpp>
#include <Cory/Testing/VisualTestUtils.hpp>

#include <catch2/catch_test_macros.hpp>

#include <Cory/RenderTasks/StandardRenderTasks.hpp>

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

namespace {

class ScopedEnvironmentVariable {
  public:
    ScopedEnvironmentVariable(const char *name, const char *value)
        : name_{name}
    {
        if (const auto *existing = std::getenv(name); existing != nullptr) {
            previousValue_ = existing;
        }
        set(value);
    }

    ~ScopedEnvironmentVariable() { set(previousValue_ ? previousValue_->c_str() : nullptr); }

  private:
    void set(const char *value)
    {
#if defined(_WIN32)
        _putenv_s(name_.c_str(), value != nullptr ? value : "");
#else
        if (value != nullptr) {
            (void)setenv(name_.c_str(), value, 1);
        }
        else {
            (void)unsetenv(name_.c_str());
        }
#endif
    }

    std::string name_;
    std::optional<std::string> previousValue_;
};

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

    CHECK(result.passed);
}

TEST_CASE("Minimal visual assertion example", "[visual][TestCanvas]")
{
    Cory::testing::VulkanTester tester;
    Cory::testing::TestCanvas canvas{tester.ctx(), glm::u32vec2{32, 32}};

    auto actual = canvas.render([](Cory::testing::TestFrame &frame) {
        auto clear = Cory::StandardRenderTasks::clearAttachments(
            frame.graph.declareTask("TASK_MinimalClearRed"),
            frame.color,
            frame.depth,
            Gpu::ColorClearValue{1.0f, 0.0f, 0.0f, 1.0f});
        return clear.output().color;
    });

    Cory::testing::requireMatchesReference("testcanvas-clear-red", actual);
}

TEST_CASE("Interactive visual comparison demo opens reviewer on mismatch",
          "[visual][TestCanvas][comparison][manual][.]")
{
    const auto scratchRoot = testScratchRoot("reviewer-demo");
    const auto baselinePath = scratchRoot / "baselines" / "reviewer-demo.bmp";
    const auto artifactRoot = scratchRoot / "artifacts";
    std::filesystem::remove_all(scratchRoot);
    std::filesystem::create_directories(baselinePath.parent_path());

    const auto baseline = Cory::testing::makeSolidImage(glm::u32vec2{2, 2}, 255, 0, 0, 255);
    const auto actual = Cory::testing::makeSolidImage(glm::u32vec2{2, 2}, 0, 255, 0, 255);
    Cory::testing::writeBmp(baselinePath, baseline);

    const ScopedEnvironmentVariable interactive{"CORY_VISUAL_INTERACTIVE", "1"};

    const auto result = Cory::testing::compareToReference("reviewer-demo",
                                                          actual,
                                                          Cory::testing::ImageCompareOptions{
                                                              .baselinePathOverride = baselinePath,
                                                              .artifactRootOverride = artifactRoot,
                                                          });

    CHECK(result.passed);
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

    CHECK_FALSE(result.passed);
}

TEST_CASE("Visual comparison honors environment overrides when updating a baseline",
          "[visual][TestCanvas][comparison]")
{
    const auto scratchRoot = testScratchRoot("environment-overrides");
    const auto baselineRoot = scratchRoot / "baselines";
    const auto artifactRoot = scratchRoot / "artifacts";
    std::filesystem::remove_all(scratchRoot);

    const auto baselineRootString = baselineRoot.string();
    const auto artifactRootString = artifactRoot.string();
    const ScopedEnvironmentVariable baselineDir{"CORY_VISUAL_BASELINE_DIR",
                                                baselineRootString.c_str()};
    const ScopedEnvironmentVariable artifactDir{"CORY_VISUAL_ARTIFACT_DIR",
                                                artifactRootString.c_str()};
    const ScopedEnvironmentVariable updateBaselines{"CORY_UPDATE_VISUAL_BASELINES", "1"};

    const auto actual = Cory::testing::makeSolidImage(glm::u32vec2{2, 2}, 10, 20, 30, 255);
    const auto result = Cory::testing::compareToReference("environment-overrides", actual);

    CHECK(result.passed);
    CHECK(result.baselinePath == baselineRoot / "environment-overrides.bmp");
    CHECK(result.actualPath.string().starts_with(artifactRoot.string()));
    CHECK(std::filesystem::exists(result.baselinePath));
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

    const ScopedEnvironmentVariable interactive{"CORY_VISUAL_INTERACTIVE", "1"};

    const auto result = Cory::testing::compareToReference(
        "review-fail-closed",
        actual,
        Cory::testing::ImageCompareOptions{
            .baselinePathOverride = baselinePath,
            .artifactRootOverride = artifactRoot,
            .reviewerExecutableOverride = scratchRoot / "missing-reviewer.exe",
        });

    CHECK_FALSE(result.passed);
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

    const ScopedEnvironmentVariable interactive{"CORY_VISUAL_INTERACTIVE", "1"};

    const auto result =
        Cory::testing::compareToReference("review-auto-accept",
                                          actual,
                                          Cory::testing::ImageCompareOptions{
                                              .baselinePathOverride = baselinePath,
                                              .artifactRootOverride = artifactRoot,
                                              .reviewerArguments = {"--auto-accept"},
                                          });
    const auto updatedBaseline = Cory::testing::readBmp(baselinePath);

    CHECK(result.passed);
    REQUIRE(updatedBaseline);
    CHECK(updatedBaseline->pixelsRgba8 == actual.pixelsRgba8);
}
