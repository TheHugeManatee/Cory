#include "VisualTestUtils.hpp"

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Framegraph/FramegraphResourceManager.hpp>
#include <Cory/IO/Bmp.hpp>
#include <Cory/RenderTasks/StandardRenderTasks.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/Synchronization.hpp>
#include <Cory/Tools/VisualReviewProtocol.hpp>

#include <KDGpu/buffer_options.h>
#include <KDGpu/texture.h>
#include <KDGpu/vulkan/vulkan_resource_manager.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/interfaces/catch_interfaces_capture.hpp>
#include <gsl/narrow>

#include <vulkan/vulkan.h>

#if defined(_WIN32)
#include <process.h>
#endif

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace Cory::testing {
namespace {

constexpr size_t kRgbaBytesPerPixel = 4;

[[nodiscard]] bool envEnabled(const char *name)
{
    const char *value = std::getenv(name);
    return value != nullptr && std::string_view{value} != "" && std::string_view{value} != "0";
}

[[nodiscard]] std::string sanitizeCaseName(std::string_view caseName)
{
    std::string result;
    result.reserve(caseName.size());
    for (const char ch : caseName) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '_') {
            result.push_back(ch);
        }
        else {
            result.push_back('-');
        }
    }
    return result.empty() ? "visual-case" : result;
}

[[nodiscard]] std::filesystem::path baselinePathFor(std::string_view caseName,
                                                    const ImageCompareOptions &options)
{
    if (options.baselinePathOverride) return *options.baselinePathOverride;
#ifdef CORY_TEST_SOURCE_DIR
    const auto root = std::filesystem::path{CORY_TEST_SOURCE_DIR} / "baselines" / "visual";
#else
    const auto root = std::filesystem::current_path() / "tests" / "baselines" / "visual";
#endif
    return root / fmt::format("{}.bmp", sanitizeCaseName(caseName));
}

[[nodiscard]] std::string currentCatchTestName()
{
    try {
        return Catch::getResultCapture().getCurrentTestName();
    }
    catch (...) {
        return {};
    }
}

[[nodiscard]] std::filesystem::path artifactDirFor(std::string_view caseName,
                                                   std::string_view catchTestName,
                                                   const std::source_location &sourceLocation,
                                                   const ImageCompareOptions &options)
{
#ifdef CORY_TEST_RUNTIME_DIR
    const auto defaultRoot = std::filesystem::path{CORY_TEST_RUNTIME_DIR} / "visual-artifacts";
#else
    const auto defaultRoot = std::filesystem::current_path() / "visual-artifacts";
#endif
    const auto root = options.artifactRootOverride ? *options.artifactRootOverride : defaultRoot;
    const auto testComponent =
        catchTestName.empty() ? std::string{"unknown-catch-test"} : sanitizeCaseName(catchTestName);
    const auto caseComponent =
        fmt::format("{}-L{}", sanitizeCaseName(caseName), sourceLocation.line());
    return root / testComponent / caseComponent;
}

void writeText(const std::filesystem::path &path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file{path, std::ios::binary};
    file << text;
}

[[nodiscard]] int launchReviewerProcess(const std::filesystem::path &reviewer,
                                        const std::filesystem::path &requestPath,
                                        std::span<const std::string> extraArgs)
{
    std::vector<std::string> args;
    args.push_back(reviewer.string());
    args.emplace_back("--request");
    args.push_back(requestPath.string());
    args.insert(args.end(), extraArgs.begin(), extraArgs.end());

#if defined(_WIN32)
    std::vector<const char *> argv;
    argv.reserve(args.size() + 1);
    for (const auto &arg : args) {
        argv.push_back(arg.c_str());
    }
    argv.push_back(nullptr);
    return _spawnv(_P_WAIT, args.front().c_str(), argv.data());
#else
    std::string command;
    for (const auto &arg : args) {
        if (!command.empty()) command += " ";
        command += "'";
        for (const char ch : arg) {
            if (ch == '\'')
                command += "'\\''";
            else
                command.push_back(ch);
        }
        command += "'";
    }
    return std::system(command.c_str());
#endif
}

[[nodiscard]] std::filesystem::path reviewerExecutablePath(const ImageCompareOptions &options)
{
    if (options.reviewerExecutableOverride) return *options.reviewerExecutableOverride;
#ifdef CORY_TEST_RUNTIME_DIR
#ifdef CORY_TEST_EXECUTABLE_SUFFIX
    return std::filesystem::path{CORY_TEST_RUNTIME_DIR} /
           fmt::format("VisualDiffReviewer{}", CORY_TEST_EXECUTABLE_SUFFIX);
#else
    return std::filesystem::path{CORY_TEST_RUNTIME_DIR} / "VisualDiffReviewer";
#endif
#else
    return std::filesystem::current_path() / "VisualDiffReviewer";
#endif
}

[[nodiscard]] bool runInteractiveReview(const Tools::VisualReview::VisualReviewRequest &request,
                                        const ImageCompareOptions &options)
{
    if (!envEnabled("CORY_VISUAL_INTERACTIVE") || envEnabled("CI")) return false;

    const auto reviewer = reviewerExecutablePath(options);
    if (!std::filesystem::exists(reviewer)) {
        CO_CORE_ERROR(
            "CORY_VISUAL_INTERACTIVE is enabled, but reviewer executable was not found at {}",
            reviewer.string());
        return false;
    }

    std::error_code ec;
    std::filesystem::remove(request.decisionPath, ec);

    const int exitCode =
        launchReviewerProcess(reviewer, request.requestPath, options.reviewerArguments);
    if (exitCode != 0) {
        CO_CORE_ERROR("Visual reviewer failed or was rejected by the shell (exit code {}).",
                      exitCode);
        return false;
    }

    auto decision = Tools::VisualReview::readDecision(request.decisionPath);
    if (!decision) {
        CO_CORE_ERROR("Visual reviewer did not write a valid decision file: {}",
                      request.decisionPath.string());
        return false;
    }
    if (decision->requestId != request.id) {
        CO_CORE_ERROR("Visual reviewer decision id mismatch: expected '{}', got '{}'",
                      request.id,
                      decision->requestId);
        return false;
    }
    return decision->accepted;
}

[[nodiscard]] ImageRgba8 makeDiffImage(const ImageRgba8 &baseline, const ImageRgba8 &actual)
{
    ImageRgba8 diff{.size = actual.size};
    diff.pixels.resize(actual.pixels.size());
    for (size_t i = 0; i < actual.pixels.size(); i += 4) {
        const auto ar = static_cast<uint8_t>(actual.pixels[i + 0]);
        const auto ag = static_cast<uint8_t>(actual.pixels[i + 1]);
        const auto ab = static_cast<uint8_t>(actual.pixels[i + 2]);
        const auto br = static_cast<uint8_t>(baseline.pixels[i + 0]);
        const auto bg = static_cast<uint8_t>(baseline.pixels[i + 1]);
        const auto bb = static_cast<uint8_t>(baseline.pixels[i + 2]);
        diff.pixels[i + 0] = static_cast<std::byte>(std::min(255, std::abs(int(ar) - int(br)) * 8));
        diff.pixels[i + 1] = static_cast<std::byte>(std::min(255, std::abs(int(ag) - int(bg)) * 8));
        diff.pixels[i + 2] = static_cast<std::byte>(std::min(255, std::abs(int(ab) - int(bb)) * 8));
        diff.pixels[i + 3] = static_cast<std::byte>(255);
    }
    return diff;
}

[[nodiscard]] std::string metricsJson(std::string_view caseName, const ImageCompareResult &result)
{
    std::ostringstream out;
    out << std::setprecision(10);
    out << "{\n";
    out << "  \"case\": \"" << caseName << "\",\n";
    out << "  \"passed\": " << (result.passed ? "true" : "false") << ",\n";
    out << "  \"width\": " << result.size.x << ",\n";
    out << "  \"height\": " << result.size.y << ",\n";
    out << "  \"mismatchedPixels\": " << result.mismatchedPixels << ",\n";
    out << "  \"mismatchRatio\": " << result.mismatchRatio << ",\n";
    out << "  \"maxChannelError\": " << static_cast<uint32_t>(result.maxChannelError) << ",\n";
    out << "  \"meanAbsoluteError\": " << result.meanAbsoluteError << ",\n";
    out << "  \"baseline\": \"" << result.baselinePath.generic_string() << "\",\n";
    out << "  \"actual\": \"" << result.actualPath.generic_string() << "\",\n";
    out << "  \"diff\": \"" << result.diffPath.generic_string() << "\",\n";
    out << "  \"metrics\": \"" << result.metricsPath.generic_string() << "\",\n";
    out << "  \"request\": \"" << result.requestPath.generic_string() << "\",\n";
    out << "  \"decision\": \"" << result.decisionPath.generic_string() << "\",\n";
    out << "  \"catchTestName\": \"" << result.catchTestName << "\",\n";
    out << "  \"sourceFile\": \"" << result.sourceFile.generic_string() << "\",\n";
    out << "  \"sourceLine\": " << result.sourceLine << ",\n";
    out << "  \"sourceFunction\": \"" << result.sourceFunction << "\"\n";
    out << "}\n";
    return out.str();
}

[[nodiscard]] ImageRgba8
readbackTextureRgba8(Context &ctx, const Texture &texture, glm::u32vec2 size, Gpu::Format format)
{
    CO_CORE_ASSERT(format == Gpu::Format::B8G8R8A8_UNORM || format == Gpu::Format::R8G8B8A8_UNORM,
                   "TestCanvas readback currently supports only BGRA8/RGBA8 UNORM textures");

    const auto byteSize = static_cast<Gpu::DeviceSize>(size.x) *
                          static_cast<Gpu::DeviceSize>(size.y) *
                          static_cast<Gpu::DeviceSize>(kRgbaBytesPerPixel);
    auto readback = ctx.device().createBuffer(
        Gpu::BufferOptions{.label = "TestCanvasReadback",
                           .size = byteSize,
                           .usage = Gpu::BufferUsageFlagBits::TransferDstBit,
                           .memoryUsage = Gpu::MemoryUsage::CpuOnly});

    auto recorder = ctx.device().createCommandRecorder(Gpu::CommandRecorderOptions{
        .label = "CMD-TestCanvasReadback",
        .queue = ctx.graphicsQueue().handle(),
        .level = Gpu::CommandBufferLevel::Primary,
    });

    auto *textureResource = ctx.resources().getTexture(texture.handle());
    auto *bufferResource = ctx.resources().getBuffer(readback.handle());
    auto *commandRecorderResource = ctx.resources().getCommandRecorder(recorder);
    CO_CORE_ASSERT(textureResource != nullptr && bufferResource != nullptr &&
                       commandRecorderResource != nullptr,
                   "Failed to resolve Vulkan resources for TestCanvas readback");

    VkBufferImageCopy region{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .mipLevel = 0,
                             .baseArrayLayer = 0,
                             .layerCount = 1},
        .imageOffset = {0, 0, 0},
        .imageExtent = {size.x, size.y, 1},
    };
    vkCmdCopyImageToBuffer(commandRecorderResource->commandBuffer,
                           textureResource->image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           bufferResource->buffer,
                           1,
                           &region);

    auto commands = recorder.finish();
    auto fence = ctx.device().createFence(
        Gpu::FenceOptions{.label = "TestCanvasReadbackFence", .createSignalled = false});
    ctx.graphicsQueue().submit(
        Gpu::SubmitOptions{.commandBuffers = {commands.handle()}, .signalFence = fence.handle()});
    fence.wait();

    const auto *mapped = static_cast<const std::byte *>(readback.map());
    CO_CORE_ASSERT(mapped != nullptr, "Failed to map TestCanvas readback buffer");

    ImageRgba8 image{.size = size};
    image.pixels.resize(gsl::narrow<size_t>(byteSize));
    if (format == Gpu::Format::R8G8B8A8_UNORM) {
        std::memcpy(image.pixels.data(), mapped, image.pixels.size());
    }
    else {
        for (size_t i = 0; i < image.pixels.size(); i += 4) {
            image.pixels[i + 0] = mapped[i + 2];
            image.pixels[i + 1] = mapped[i + 1];
            image.pixels[i + 2] = mapped[i + 0];
            image.pixels[i + 3] = mapped[i + 3];
        }
    }
    readback.unmap();
    return image;
}

} // namespace

struct TestCanvasPrivate {
    Context *ctx{};
    TestCanvasCreateInfo createInfo{};
    HeadlessFrameSource frameSource;
    FramegraphResourceManager resources;
    Framegraph framegraph;

    TestCanvasPrivate(Context &context, TestCanvasCreateInfo info)
        : ctx{&context}
        , createInfo{std::move(info)}
        , frameSource{context,
                      HeadlessFrameSourceCreateInfo{.label = createInfo.label,
                                                    .size = createInfo.size,
                                                    .samples = createInfo.samples,
                                                    .colorFormat = createInfo.colorFormat,
                                                    .imageCount = 1}}
        , resources{context}
        , framegraph{context, resources, 0}
    {
    }
};

TestCanvas::TestCanvas(Context &ctx, glm::u32vec2 size)
    : TestCanvas{ctx, TestCanvasCreateInfo{.size = size}}
{
}

TestCanvas::TestCanvas(Context &ctx, TestCanvasCreateInfo createInfo)
    : data_{std::make_unique<TestCanvasPrivate>(ctx, std::move(createInfo))}
{
}

TestCanvas::~TestCanvas() = default;

Context &TestCanvas::ctx()
{
    return *data_->ctx;
}

glm::u32vec2 TestCanvas::size() const
{
    return data_->createInfo.size;
}

ImageRgba8
TestCanvas::renderImpl(const std::function<TransientTextureHandle(TestFrame &)> &renderFunc)
{
    const Texture *captureTexture{};
    {
        auto frames = data_->frameSource.frames();
        auto it = frames.begin();
        FrameContext &frameCtx = *it;
        captureTexture = frameCtx.swapchainImage;

        data_->framegraph.resetForNextFrame(frameCtx.frameNumber);
        auto frameHandles = data_->framegraph.importFrameContext(frameCtx);
        TestFrame frame{.frameCtx = frameCtx,
                        .graph = data_->framegraph,
                        .frame = frameHandles,
                        .color = frameHandles.colorImage,
                        .depth = frameHandles.depthImage,
                        .swapchain = frameHandles.swapchainImage};

        const auto colorToCapture = renderFunc(frame);
        auto copied = StandardRenderTasks::copyToTarget(
                          data_->framegraph.declareTask("TASK_TestCanvasCopyToCapture"),
                          colorToCapture,
                          frameHandles.swapchainImage)
                          .output();
        data_->framegraph.declareOutput(copied, Sync::AccessType::TransferRead);
        (void)data_->framegraph.record(frameCtx);
        // Leaving this scope destroys the FrameGenerator iterator, which submits the pending frame.
    }

    data_->ctx->device().waitUntilIdle();
    CO_CORE_ASSERT(captureTexture != nullptr, "TestCanvas did not capture a frame texture");
    return readbackTextureRgba8(
        *data_->ctx, *captureTexture, data_->createInfo.size, data_->createInfo.colorFormat);
}

ImageRgba8 makeSolidImage(glm::u32vec2 size, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    ImageRgba8 image{.size = size};
    image.pixels.resize(static_cast<size_t>(size.x) * static_cast<size_t>(size.y) *
                        kRgbaBytesPerPixel);
    for (size_t i = 0; i < image.pixels.size(); i += 4) {
        image.pixels[i + 0] = static_cast<std::byte>(r);
        image.pixels[i + 1] = static_cast<std::byte>(g);
        image.pixels[i + 2] = static_cast<std::byte>(b);
        image.pixels[i + 3] = static_cast<std::byte>(a);
    }
    return image;
}

void writeBmp(const std::filesystem::path &path, const ImageRgba8 &image)
{
    const auto result = IO::writeBmpRgba8(path,
                                          IO::BmpImageRgba8{.width = image.size.x,
                                                            .height = image.size.y,
                                                            .pixelsRgba8 = image.pixels});
    CO_CORE_ASSERT(result.has_value(), "{}", result.error());
}

Result<ImageRgba8> readBmp(const std::filesystem::path &path)
{
    auto image = IO::loadBmpRgba8(path);
    if (!image) return std::unexpected(std::move(image.error()));
    return ImageRgba8{.size = glm::u32vec2{image->width, image->height},
                      .pixels = std::move(image->pixelsRgba8)};
}

ImageCompareResult compareToReference(std::string_view caseName,
                                      const ImageRgba8 &actual,
                                      ImageCompareOptions options,
                                      std::source_location sourceLocation)
{
    auto result = ImageCompareResult{.size = actual.size};
    result.catchTestName = currentCatchTestName();
    result.sourceFile = sourceLocation.file_name();
    result.sourceLine = sourceLocation.line();
    result.sourceFunction = sourceLocation.function_name();
    result.baselinePath = baselinePathFor(caseName, options);
    const auto artifactDir =
        artifactDirFor(caseName, result.catchTestName, sourceLocation, options);
    result.actualPath = artifactDir / "actual.bmp";
    result.diffPath = artifactDir / "diff.bmp";
    result.metricsPath = artifactDir / "metrics.json";
    result.requestPath = artifactDir / "request.json";
    result.decisionPath = artifactDir / "decision.json";

    auto writeFailureArtifactsAndMaybeReview = [&](const ImageRgba8 *baselineImage) {
        std::filesystem::create_directories(artifactDir);
        writeBmp(result.actualPath, actual);
        if (baselineImage != nullptr && baselineImage->size == actual.size &&
            baselineImage->pixels.size() == actual.pixels.size()) {
            writeBmp(result.diffPath, makeDiffImage(*baselineImage, actual));
        }
        writeText(result.metricsPath, metricsJson(caseName, result));

        auto request = Tools::VisualReview::VisualReviewRequest{
            .id = Tools::VisualReview::makeRequestId(
                fmt::format("{}-{}-L{}", result.catchTestName, caseName, result.sourceLine)),
            .caseName = std::string{caseName},
            .metadata =
                Tools::VisualReview::VisualReviewMetadata{
                    .catchTestName = result.catchTestName,
                    .sourceFile = result.sourceFile.generic_string(),
                    .sourceLine = result.sourceLine,
                    .sourceFunction = result.sourceFunction,
                },
            .baselinePath = result.baselinePath,
            .actualPath = result.actualPath,
            .diffPath = result.diffPath,
            .metricsPath = result.metricsPath,
            .requestPath = result.requestPath,
            .decisionPath = result.decisionPath,
            .metrics =
                Tools::VisualReview::VisualReviewMetrics{
                    .mismatchedPixels = result.mismatchedPixels,
                    .mismatchRatio = result.mismatchRatio,
                    .maxChannelError = result.maxChannelError,
                    .meanAbsoluteError = result.meanAbsoluteError,
                },
        };
        Tools::VisualReview::writeRequest(result.requestPath, request);

        if (runInteractiveReview(request, options)) {
            writeBmp(result.baselinePath, actual);
            result.passed = true;
            CO_CORE_INFO("Accepted visual review '{}' and updated baseline at {}",
                         caseName,
                         result.baselinePath.string());
        }

        writeText(result.metricsPath, metricsJson(caseName, result));
        CO_CORE_INFO("CORY_VISUAL_RESULT {}", metricsJson(caseName, result));
    };

    auto baseline = readBmp(result.baselinePath);
    if (!baseline) {
        result.passed = false;
        writeFailureArtifactsAndMaybeReview(nullptr);
        return result;
    }

    if (baseline->size != actual.size || baseline->pixels.size() != actual.pixels.size()) {
        result.passed = false;
        writeFailureArtifactsAndMaybeReview(&*baseline);
        return result;
    }

    uint64_t totalError = 0;
    for (size_t pixel = 0; pixel < actual.pixels.size(); pixel += 4) {
        bool pixelMismatch = false;
        for (size_t c = 0; c < 4; ++c) {
            const auto a = static_cast<uint8_t>(actual.pixels[pixel + c]);
            const auto b = static_cast<uint8_t>(baseline->pixels[pixel + c]);
            const auto error = static_cast<uint8_t>(std::abs(int(a) - int(b)));
            totalError += error;
            result.maxChannelError = std::max(result.maxChannelError, error);
            if (error > options.perChannelTolerance) pixelMismatch = true;
        }
        if (pixelMismatch) ++result.mismatchedPixels;
    }

    const auto pixelCount =
        static_cast<uint64_t>(actual.size.x) * static_cast<uint64_t>(actual.size.y);
    result.mismatchRatio = pixelCount == 0 ? 0.0
                                           : static_cast<double>(result.mismatchedPixels) /
                                                 static_cast<double>(pixelCount);
    result.meanAbsoluteError =
        actual.pixels.empty()
            ? 0.0
            : static_cast<double>(totalError) / static_cast<double>(actual.pixels.size());
    result.passed = result.mismatchRatio <= options.maxMismatchRatio &&
                    result.meanAbsoluteError <= options.maxMeanAbsoluteError;

    if (!result.passed) {
        writeFailureArtifactsAndMaybeReview(&*baseline);
        return result;
    }
    CO_CORE_INFO("CORY_VISUAL_RESULT {}", metricsJson(caseName, result));
    return result;
}

void requireMatchesReference(std::string_view caseName,
                             const ImageRgba8 &actual,
                             ImageCompareOptions options,
                             std::source_location sourceLocation)
{
    const auto result = compareToReference(caseName, actual, options, sourceLocation);
    INFO("Visual baseline: " << result.baselinePath.string());
    INFO("Visual actual: " << result.actualPath.string());
    INFO("Visual diff: " << result.diffPath.string());
    INFO("Visual metrics: " << result.metricsPath.string());
    INFO("Visual review request: " << result.requestPath.string());
    if (!result.decisionPath.empty()) {
        INFO("Visual review decision: " << result.decisionPath.string());
    }
    CHECK(result.passed);
}

} // namespace Cory::testing
