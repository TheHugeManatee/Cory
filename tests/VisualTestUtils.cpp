#include "VisualTestUtils.hpp"

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Framegraph/FramegraphResourceManager.hpp>
#include <Cory/RenderTasks/StandardRenderTasks.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/Synchronization.hpp>

#include <KDGpu/buffer_options.h>
#include <KDGpu/texture.h>
#include <KDGpu/vulkan/vulkan_resource_manager.h>
#include <catch2/catch_test_macros.hpp>
#include <gsl/narrow>

#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <span>
#include <sstream>
#include <stdexcept>

namespace Cory::testing {
namespace {

constexpr uint16_t kBmpFileHeaderSize = 14;
constexpr uint32_t kBmpInfoHeaderSize = 40;
constexpr uint16_t kBmpPlanes = 1;
constexpr uint16_t kBmpBitCount = 32;
constexpr uint32_t kBmpCompressionRgb = 0;
constexpr size_t kRgbaBytesPerPixel = 4;

[[nodiscard]] bool envEnabled(const char *name)
{
    const char *value = std::getenv(name);
    return value != nullptr && std::string_view{value} != "" && std::string_view{value} != "0";
}

[[nodiscard]] std::filesystem::path envPath(const char *name, std::filesystem::path fallback)
{
    const char *value = std::getenv(name);
    if (value == nullptr || std::string_view{value}.empty()) return fallback;
    return std::filesystem::path{value};
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

void appendU16(std::vector<std::byte> &out, uint16_t value)
{
    out.push_back(static_cast<std::byte>(value & 0xFFU));
    out.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
}

void appendU32(std::vector<std::byte> &out, uint32_t value)
{
    out.push_back(static_cast<std::byte>(value & 0xFFU));
    out.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::byte>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<std::byte>((value >> 24U) & 0xFFU));
}

void appendI32(std::vector<std::byte> &out, int32_t value)
{
    appendU32(out, static_cast<uint32_t>(value));
}

[[nodiscard]] uint16_t readLe16(std::span<const std::byte> bytes, size_t offset)
{
    return static_cast<uint16_t>(
        static_cast<uint8_t>(bytes[offset]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(bytes[offset + 1])) << 8U));
}

[[nodiscard]] uint32_t readLe32(std::span<const std::byte> bytes, size_t offset)
{
    return static_cast<uint32_t>(static_cast<uint8_t>(bytes[offset])) |
           (static_cast<uint32_t>(static_cast<uint8_t>(bytes[offset + 1])) << 8U) |
           (static_cast<uint32_t>(static_cast<uint8_t>(bytes[offset + 2])) << 16U) |
           (static_cast<uint32_t>(static_cast<uint8_t>(bytes[offset + 3])) << 24U);
}

[[nodiscard]] int32_t readLeI32(std::span<const std::byte> bytes, size_t offset)
{
    return static_cast<int32_t>(readLe32(bytes, offset));
}

[[nodiscard]] std::filesystem::path baselinePathFor(std::string_view caseName)
{
#ifdef CORY_TEST_SOURCE_DIR
    const auto defaultRoot = std::filesystem::path{CORY_TEST_SOURCE_DIR} / "baselines" / "visual";
#else
    const auto defaultRoot = std::filesystem::current_path() / "tests" / "baselines" / "visual";
#endif
    const auto root = envPath("CORY_VISUAL_BASELINE_DIR", defaultRoot);
    return root / fmt::format("{}.bmp", sanitizeCaseName(caseName));
}

[[nodiscard]] std::filesystem::path artifactDirFor(std::string_view caseName)
{
    const auto root =
        envPath("CORY_VISUAL_ARTIFACT_DIR", std::filesystem::current_path() / "visual-artifacts");
    return root / sanitizeCaseName(caseName);
}

void writeText(const std::filesystem::path &path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file{path, std::ios::binary};
    file << text;
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
    out << "  \"metrics\": \"" << result.metricsPath.generic_string() << "\"\n";
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
    std::filesystem::create_directories(path.parent_path());
    const auto pixelBytes = static_cast<uint32_t>(image.pixels.size());
    const auto fileSize = kBmpFileHeaderSize + kBmpInfoHeaderSize + pixelBytes;

    std::vector<std::byte> bytes;
    bytes.reserve(fileSize);
    bytes.push_back(static_cast<std::byte>('B'));
    bytes.push_back(static_cast<std::byte>('M'));
    appendU32(bytes, fileSize);
    appendU16(bytes, 0);
    appendU16(bytes, 0);
    appendU32(bytes, kBmpFileHeaderSize + kBmpInfoHeaderSize);
    appendU32(bytes, kBmpInfoHeaderSize);
    appendI32(bytes, gsl::narrow<int32_t>(image.size.x));
    appendI32(bytes, -gsl::narrow<int32_t>(image.size.y)); // top-down rows
    appendU16(bytes, kBmpPlanes);
    appendU16(bytes, kBmpBitCount);
    appendU32(bytes, kBmpCompressionRgb);
    appendU32(bytes, pixelBytes);
    appendI32(bytes, 0);
    appendI32(bytes, 0);
    appendU32(bytes, 0);
    appendU32(bytes, 0);

    for (size_t i = 0; i < image.pixels.size(); i += 4) {
        bytes.push_back(image.pixels[i + 2]);
        bytes.push_back(image.pixels[i + 1]);
        bytes.push_back(image.pixels[i + 0]);
        bytes.push_back(image.pixels[i + 3]);
    }

    std::ofstream file{path, std::ios::binary};
    file.write(reinterpret_cast<const char *>(bytes.data()),
               gsl::narrow<std::streamsize>(bytes.size()));
}

Result<ImageRgba8> readBmp(const std::filesystem::path &path)
{
    std::ifstream file{path, std::ios::binary | std::ios::ate};
    if (!file) return std::unexpected(fmt::format("Could not open BMP: {}", path.string()));
    const auto size = file.tellg();
    file.seekg(0);
    std::vector<std::byte> bytes(gsl::narrow<size_t>(size));
    file.read(reinterpret_cast<char *>(bytes.data()), size);

    if (bytes.size() < kBmpFileHeaderSize + kBmpInfoHeaderSize || bytes[0] != std::byte{'B'} ||
        bytes[1] != std::byte{'M'}) {
        return std::unexpected("Not a BMP file");
    }

    const auto pixelOffset = readLe32(bytes, 10);
    const auto headerSize = readLe32(bytes, 14);
    const auto width = readLeI32(bytes, 18);
    const auto height = readLeI32(bytes, 22);
    const auto planes = readLe16(bytes, 26);
    const auto bitCount = readLe16(bytes, 28);
    const auto compression = readLe32(bytes, 30);
    if (headerSize != kBmpInfoHeaderSize || width <= 0 || height == 0 || planes != kBmpPlanes ||
        bitCount != kBmpBitCount || compression != kBmpCompressionRgb) {
        return std::unexpected("Unsupported BMP format; expected uncompressed 32-bit BGRA BMP");
    }

    const auto absHeight = height < 0 ? -height : height;
    const auto imageSize =
        glm::u32vec2{gsl::narrow<uint32_t>(width), gsl::narrow<uint32_t>(absHeight)};
    const auto requiredBytes =
        static_cast<size_t>(imageSize.x) * static_cast<size_t>(imageSize.y) * kRgbaBytesPerPixel;
    if (pixelOffset + requiredBytes > bytes.size())
        return std::unexpected("BMP pixel data is truncated");

    ImageRgba8 image{.size = imageSize};
    image.pixels.resize(requiredBytes);
    const bool topDown = height < 0;
    for (uint32_t y = 0; y < imageSize.y; ++y) {
        const auto srcY = topDown ? y : (imageSize.y - 1U - y);
        const auto *src = bytes.data() + pixelOffset +
                          static_cast<size_t>(srcY) * imageSize.x * kRgbaBytesPerPixel;
        auto *dst = image.pixels.data() + static_cast<size_t>(y) * imageSize.x * kRgbaBytesPerPixel;
        for (uint32_t x = 0; x < imageSize.x; ++x) {
            dst[x * 4 + 0] = src[x * 4 + 2];
            dst[x * 4 + 1] = src[x * 4 + 1];
            dst[x * 4 + 2] = src[x * 4 + 0];
            dst[x * 4 + 3] = src[x * 4 + 3];
        }
    }
    return image;
}

ImageCompareResult
compareToReference(std::string_view caseName, const ImageRgba8 &actual, ImageCompareOptions options)
{
    auto result = ImageCompareResult{.size = actual.size};
    result.baselinePath = baselinePathFor(caseName);
    const auto artifactDir = artifactDirFor(caseName);
    result.actualPath = artifactDir / "actual.bmp";
    result.diffPath = artifactDir / "diff.bmp";
    result.metricsPath = artifactDir / "metrics.json";

    if (envEnabled("CORY_UPDATE_VISUAL_BASELINES")) {
        writeBmp(result.baselinePath, actual);
        result.passed = true;
        CO_CORE_INFO("Updated visual baseline '{}' at {}", caseName, result.baselinePath.string());
        return result;
    }

    auto baseline = readBmp(result.baselinePath);
    if (!baseline) {
        writeBmp(result.actualPath, actual);
        result.passed = false;
        writeText(result.metricsPath, metricsJson(caseName, result));
        CO_CORE_INFO("CORY_VISUAL_RESULT {}", metricsJson(caseName, result));
        return result;
    }

    if (baseline->size != actual.size || baseline->pixels.size() != actual.pixels.size()) {
        writeBmp(result.actualPath, actual);
        result.passed = false;
        writeText(result.metricsPath, metricsJson(caseName, result));
        CO_CORE_INFO("CORY_VISUAL_RESULT {}", metricsJson(caseName, result));
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

    if (!result.passed || envEnabled("CORY_VISUAL_ALWAYS_WRITE_ACTUAL")) {
        writeBmp(result.actualPath, actual);
    }
    if (!result.passed) {
        std::filesystem::create_directories(artifactDir);
        writeBmp(result.diffPath, makeDiffImage(*baseline, actual));
        writeText(result.metricsPath, metricsJson(caseName, result));
    }
    CO_CORE_INFO("CORY_VISUAL_RESULT {}", metricsJson(caseName, result));
    return result;
}

void requireMatchesReference(std::string_view caseName,
                             const ImageRgba8 &actual,
                             ImageCompareOptions options)
{
    const auto result = compareToReference(caseName, actual, options);
    INFO("Visual actual: " << result.actualPath.string());
    INFO("Visual baseline: " << result.baselinePath.string());
    INFO("Visual diff: " << result.diffPath.string());
    CHECK(result.passed);
}

} // namespace Cory::testing
