#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Base/Result.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/HeadlessFrameSource.hpp>

#include <glm/vec2.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Cory::testing {

/**
 * @brief CPU-side image used by visual tests.
 *
 * Pixels are tightly packed RGBA8 in row-major order with a top-left origin.
 */
struct ImageRgba8 {
    glm::u32vec2 size{};
    std::vector<std::byte> pixels{};
};

/**
 * @brief Tolerances used when comparing an actual image against a stored baseline.
 */
struct ImageCompareOptions {
    /// Maximum accepted absolute error for any channel before a pixel is counted as mismatched.
    uint8_t perChannelTolerance{0};
    /// Maximum accepted average absolute error across all channels.
    double maxMeanAbsoluteError{0.0};
    /// Maximum accepted ratio of mismatched pixels in [0, 1].
    double maxMismatchRatio{0.0};
    /// Optional override for the reference baseline path (primarily for helper tests).
    std::optional<std::filesystem::path> baselinePathOverride{};
    /// Optional override for the artifact root directory (primarily for helper tests).
    std::optional<std::filesystem::path> artifactRootOverride{};
    /// Optional override for the visual reviewer executable path (primarily for helper tests).
    std::optional<std::filesystem::path> reviewerExecutableOverride{};
    /// Additional arguments to pass to the reviewer executable.
    std::vector<std::string> reviewerArguments{};
};

/**
 * @brief Metrics and artifact paths produced by a visual image comparison.
 */
struct ImageCompareResult {
    bool passed{};
    glm::u32vec2 size{};
    uint64_t mismatchedPixels{};
    double mismatchRatio{};
    uint8_t maxChannelError{};
    double meanAbsoluteError{};
    std::filesystem::path baselinePath{};
    std::filesystem::path actualPath{};
    std::filesystem::path diffPath{};
    std::filesystem::path metricsPath{};
    std::filesystem::path requestPath{};
    std::filesystem::path decisionPath{};
    std::string catchTestName{};
    std::filesystem::path sourceFile{};
    uint64_t sourceLine{};
    std::string sourceFunction{};
};

/**
 * @brief Construction parameters for an isolated one-frame visual test canvas.
 */
struct TestCanvasCreateInfo {
    glm::u32vec2 size{128, 128};
    Gpu::Format colorFormat{Gpu::Format::B8G8R8A8_UNORM};
    Gpu::SampleCountFlagBits samples{Gpu::SampleCountFlagBits::Samples1Bit};
    std::string label{"TestCanvas"};
};

/**
 * @brief Per-frame objects passed to a TestCanvas render callback.
 */
struct TestFrame {
    FrameContext &frameCtx;
    Framegraph &graph;
    Framegraph::FrameContextHandles frame;
    TransientTextureHandle color;
    TransientTextureHandle depth;
    TransientTextureHandle swapchain;
};

/**
 * @brief Headless one-frame render target for Catch2 visual tests.
 *
 * The render callback declares framegraph work and returns the color texture to capture. TestCanvas
 * copies that texture into an offscreen capture image, submits the frame, reads it back, and
 * returns an ImageRgba8 suitable for baseline comparison.
 */
class TestCanvas : NoCopy, NoMove {
  public:
    TestCanvas(Context &ctx, glm::u32vec2 size);
    TestCanvas(Context &ctx, TestCanvasCreateInfo createInfo);
    ~TestCanvas();

    [[nodiscard]] Context &ctx();
    [[nodiscard]] glm::u32vec2 size() const;

    /** @brief Render one frame and read back the callback's returned color texture. */
    template <typename RenderFunc> ImageRgba8 render(RenderFunc &&renderFunc)
    {
        return renderImpl([&](TestFrame &frame) -> TransientTextureHandle {
            return std::invoke(std::forward<RenderFunc>(renderFunc), frame);
        });
    }

  private:
    ImageRgba8 renderImpl(const std::function<TransientTextureHandle(TestFrame &)> &renderFunc);

    std::unique_ptr<struct TestCanvasPrivate> data_;
};

/**
 * @brief Create a solid-color RGBA8 image for comparison-helper tests.
 */
[[nodiscard]] ImageRgba8
makeSolidImage(glm::u32vec2 size, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);

/**
 * @brief Write an ImageRgba8 as an uncompressed 32-bit BMP artifact.
 */
void writeBmp(const std::filesystem::path &path, const ImageRgba8 &image);
/**
 * @brief Read an uncompressed 32-bit BMP artifact into ImageRgba8.
 */
[[nodiscard]] Result<ImageRgba8> readBmp(const std::filesystem::path &path);

/**
 * @brief Compare an actual image to `tests/baselines/visual/<caseName>.bmp`.
 *
 * On failure this writes actual/diff/metrics/request artifacts under the test runtime directory and
 * may launch the interactive reviewer when `CORY_VISUAL_INTERACTIVE=1`.
 */
[[nodiscard]] ImageCompareResult
compareToReference(std::string_view caseName,
                   const ImageRgba8 &actual,
                   ImageCompareOptions options = {},
                   std::source_location sourceLocation = std::source_location::current());

/**
 * @brief Catch2-friendly wrapper around compareToReference that checks the comparison result.
 */
void requireMatchesReference(std::string_view caseName,
                             const ImageRgba8 &actual,
                             ImageCompareOptions options = {},
                             std::source_location sourceLocation = std::source_location::current());

} // namespace Cory::testing
