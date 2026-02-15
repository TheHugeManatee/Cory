#include "DatasetLoader.hpp"

#include <Cory/IO/Bmp.hpp>

#include <cppcoro/cancellation_source.hpp>
#include <cppcoro/schedule_on.hpp>
#include <cppcoro/when_all_ready.hpp>

#include <fmt/format.h>
#include <mio/mmap.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <optional>

namespace Cory {
namespace {

[[nodiscard]] bool wildcardMatch(std::string_view pattern, std::string_view text)
{
    auto lower = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };

    size_t patternIndex = 0;
    size_t textIndex = 0;
    size_t starIndex = std::string_view::npos;
    size_t backtrackTextIndex = 0;

    while (textIndex < text.size()) {
        if (patternIndex < pattern.size() &&
            (pattern[patternIndex] == '?' || lower(pattern[patternIndex]) == lower(text[textIndex]))) {
            ++patternIndex;
            ++textIndex;
            continue;
        }

        if (patternIndex < pattern.size() && pattern[patternIndex] == '*') {
            starIndex = patternIndex++;
            backtrackTextIndex = textIndex;
            continue;
        }

        if (starIndex != std::string_view::npos) {
            patternIndex = starIndex + 1;
            textIndex = ++backtrackTextIndex;
            continue;
        }

        return false;
    }

    while (patternIndex < pattern.size() && pattern[patternIndex] == '*') {
        ++patternIndex;
    }

    return patternIndex == pattern.size();
}

[[nodiscard]] std::optional<uint64_t> parseTrailingIndex(const std::filesystem::path &path)
{
    const auto stem = path.stem().string();
    if (stem.empty()) {
        return std::nullopt;
    }

    size_t firstDigit = stem.size();
    while (firstDigit > 0 && std::isdigit(static_cast<unsigned char>(stem[firstDigit - 1]))) {
        --firstDigit;
    }

    if (firstDigit == stem.size()) {
        return std::nullopt;
    }

    try {
        return static_cast<uint64_t>(std::stoull(stem.substr(firstDigit)));
    } catch (...) {
        return std::nullopt;
    }
}

[[nodiscard]] Result<IO::BmpInfo> querySliceInfo(const std::filesystem::path &bmpPath)
{
    std::error_code ec;
    auto bmpFile = mio::make_mmap_source(bmpPath.string(), ec);
    if (ec) {
        return std::unexpected(
            fmt::format("Failed to memory-map first slice '{}' ({})", bmpPath.string(), ec.message()));
    }

    std::span bmpBytes{reinterpret_cast<const std::byte *>(bmpFile.data()), bmpFile.size()};
    auto bmpInfo = IO::queryBmpInfo(bmpBytes);
    if (!bmpInfo) {
        return std::unexpected(
            fmt::format("Failed to query BMP info from first slice '{}' ({})",
                        bmpPath.string(),
                        bmpInfo.error()));
    }

    return bmpInfo;
}

} // namespace

DatasetLoader::DatasetLoader(size_t workerCount)
    : workerCount_{std::max<size_t>(1u, workerCount)}
    , workerPool_{static_cast<uint32_t>(workerCount_)}
{
}

DatasetLoader::~DatasetLoader() = default;

Result<std::vector<DatasetLoader::OrderedSlice>> DatasetLoader::scanSlices(const LoadStackRequest &request)
{
    if (request.directory.empty()) {
        return std::unexpected("Dataset directory is empty");
    }

    std::error_code ec;
    if (!std::filesystem::exists(request.directory, ec) || ec) {
        return std::unexpected(
            fmt::format("Dataset directory '{}' does not exist", request.directory.string()));
    }

    if (!std::filesystem::is_directory(request.directory, ec) || ec) {
        return std::unexpected(
            fmt::format("Dataset path '{}' is not a directory", request.directory.string()));
    }

    std::vector<OrderedSlice> orderedSlices;
    for (auto it = std::filesystem::directory_iterator(request.directory, ec);
         !ec && it != std::filesystem::directory_iterator{};
         it.increment(ec)) {
        const auto &entry = *it;
        if (!entry.is_regular_file(ec) || ec) {
            continue;
        }

        const auto filename = entry.path().filename().string();
        if (!wildcardMatch(request.pattern, filename)) {
            continue;
        }

        const auto sliceIndex = parseTrailingIndex(entry.path());
        if (!sliceIndex.has_value()) {
            return std::unexpected(
                fmt::format("Matched file '{}' has no trailing numeric slice index",
                            entry.path().string()));
        }

        orderedSlices.push_back(OrderedSlice{
            .index = *sliceIndex,
            .path = entry.path(),
        });
    }

    if (ec) {
        return std::unexpected(
            fmt::format("Failed while scanning directory '{}' ({})",
                        request.directory.string(),
                        ec.message()));
    }

    if (orderedSlices.empty()) {
        return std::unexpected(fmt::format("No files matching '{}' found in '{}'",
                                           request.pattern,
                                           request.directory.string()));
    }

    std::sort(orderedSlices.begin(),
              orderedSlices.end(),
              [](const OrderedSlice &a, const OrderedSlice &b) {
                  if (a.index != b.index) {
                      return a.index < b.index;
                  }
                  return a.path < b.path;
              });

    for (size_t i = 1; i < orderedSlices.size(); ++i) {
        const auto prev = orderedSlices[i - 1].index;
        const auto current = orderedSlices[i].index;
        if (current == prev) {
            return std::unexpected(
                fmt::format("Duplicate slice index {} for '{}' and '{}'",
                            current,
                            orderedSlices[i - 1].path.string(),
                            orderedSlices[i].path.string()));
        }

        if (current != prev + 1u) {
            return std::unexpected(fmt::format("Slice index gap: expected {} after {}, found {} ('{}')",
                                               prev + 1u,
                                               prev,
                                               current,
                                               orderedSlices[i].path.string()));
        }
    }

    return orderedSlices;
}

cppcoro::task<Result<void>> DatasetLoader::loadSliceR8(const std::filesystem::path &bmpPath,
                                                       std::span<std::byte> targetBuffer,
                                                       glm::uvec2 expectedDimensions,
                                                       size_t sliceIndex,
                                                       cppcoro::cancellation_token cancellationToken,
                                                       cppcoro::cancellation_source *cancellationSource)
{
    auto failAndCancel = [&](std::string message) -> Result<void> {
        if (cancellationSource != nullptr) {
            cancellationSource->request_cancellation();
        }
        return std::unexpected(std::move(message));
    };

    if (cancellationToken.is_cancellation_requested()) {
        co_return std::unexpected(
            fmt::format("Slice {} ('{}') cancelled before decode", sliceIndex, bmpPath.string()));
    }

    std::error_code ec;
    auto bmpFile = mio::make_mmap_source(bmpPath.string(), ec);
    if (ec) {
        co_return failAndCancel(fmt::format("Slice {} ('{}') mmap failed: {}",
                                            sliceIndex,
                                            bmpPath.string(),
                                            ec.message()));
    }

    std::span bmpBytes{reinterpret_cast<const std::byte *>(bmpFile.data()), bmpFile.size()};

    auto bmpInfo = IO::queryBmpInfo(bmpBytes);
    if (!bmpInfo) {
        co_return failAndCancel(fmt::format("Slice {} ('{}') info parse failed: {}",
                                            sliceIndex,
                                            bmpPath.string(),
                                            bmpInfo.error()));
    }

    if (bmpInfo->width != expectedDimensions.x || bmpInfo->height != expectedDimensions.y) {
        co_return failAndCancel(
            fmt::format("Slice {} ('{}') dimensions mismatch: expected {}x{}, got {}x{}",
                        sliceIndex,
                        bmpPath.string(),
                        expectedDimensions.x,
                        expectedDimensions.y,
                        bmpInfo->width,
                        bmpInfo->height));
    }

    const auto expectedVoxels = static_cast<size_t>(expectedDimensions.x) *
                                static_cast<size_t>(expectedDimensions.y);
    if (targetBuffer.size() != expectedVoxels) {
        co_return failAndCancel(fmt::format(
            "Slice {} ('{}') target buffer mismatch: expected {} bytes, got {} bytes",
            sliceIndex,
            bmpPath.string(),
            expectedVoxels,
            targetBuffer.size()));
    }

    std::vector<std::byte> rgba8;
    rgba8.resize(bmpInfo->rgba8ByteSize);

    auto decoded = IO::decodeBmp(bmpBytes, rgba8);
    if (!decoded) {
        co_return failAndCancel(fmt::format("Slice {} ('{}') decode failed: {}",
                                            sliceIndex,
                                            bmpPath.string(),
                                            decoded.error()));
    }

    for (size_t pixel = 0; pixel < expectedVoxels; ++pixel) {
        if ((pixel & 4095u) == 0u && cancellationToken.is_cancellation_requested()) {
            co_return std::unexpected(
                fmt::format("Slice {} ('{}') cancelled during conversion",
                            sliceIndex,
                            bmpPath.string()));
        }

        const auto rgbaOffset = pixel * 4u;
        const auto r = static_cast<uint8_t>(rgba8[rgbaOffset + 0u]);
        const auto g = static_cast<uint8_t>(rgba8[rgbaOffset + 1u]);
        const auto b = static_cast<uint8_t>(rgba8[rgbaOffset + 2u]);

        if (r != g || r != b) {
            co_return failAndCancel(
                fmt::format("Slice {} ('{}') is not grayscale-compatible at pixel {} (r={}, g={}, b={})",
                            sliceIndex,
                            bmpPath.string(),
                            pixel,
                            r,
                            g,
                            b));
        }

        targetBuffer[pixel] = static_cast<std::byte>(r);
    }

    co_return Result<void>{};
}

cppcoro::task<Result<LoadedVolume>> DatasetLoader::loadBmpStack(const LoadStackRequest &request)
{
    auto scanResult = scanSlices(request);
    if (!scanResult) {
        co_return std::unexpected(std::move(scanResult.error()));
    }

    auto orderedSlices = std::move(*scanResult);
    auto firstInfo = querySliceInfo(orderedSlices.front().path);
    if (!firstInfo) {
        co_return std::unexpected(std::move(firstInfo.error()));
    }

    const auto x = firstInfo->width;
    const auto y = firstInfo->height;
    const auto z = orderedSlices.size();

    if (x == 0u || y == 0u || z == 0u) {
        co_return std::unexpected(fmt::format("Invalid volume dimensions {}x{}x{}", x, y, z));
    }

    if (z > std::numeric_limits<uint32_t>::max()) {
        co_return std::unexpected(fmt::format("Slice count {} exceeds uint32 limit", z));
    }

    const auto sliceVoxelCount64 = static_cast<uint64_t>(x) * static_cast<uint64_t>(y);
    const auto totalVoxelCount64 = sliceVoxelCount64 * static_cast<uint64_t>(z);
    if (sliceVoxelCount64 > static_cast<uint64_t>(std::numeric_limits<size_t>::max()) ||
        totalVoxelCount64 > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
        co_return std::unexpected(fmt::format("Volume {}x{}x{} exceeds host memory addressable range",
                                              x,
                                              y,
                                              z));
    }

    const auto sliceVoxelCount = static_cast<size_t>(sliceVoxelCount64);

    LoadedVolume loaded{
        .dimensions = glm::uvec3{x, y, static_cast<uint32_t>(z)},
        .voxelsR8 = std::vector<std::byte>(static_cast<size_t>(totalVoxelCount64)),
        .orderedSlicePaths = {},
    };
    loaded.orderedSlicePaths.reserve(orderedSlices.size());
    for (const auto &slice : orderedSlices) {
        loaded.orderedSlicePaths.push_back(slice.path);
    }

    cppcoro::cancellation_source cancellationSource{};

    const auto requestedConcurrency = request.maxConcurrency == 0 ? workerCount_ : request.maxConcurrency;
    const auto effectiveConcurrency = std::max<size_t>(1u, std::min(workerCount_, requestedConcurrency));

    for (size_t batchStart = 0; batchStart < orderedSlices.size(); batchStart += effectiveConcurrency) {
        if (cancellationSource.is_cancellation_requested()) {
            co_return std::unexpected("Volume load cancelled");
        }

        const auto batchEnd = std::min(orderedSlices.size(), batchStart + effectiveConcurrency);
        std::vector<cppcoro::task<Result<void>>> tasks;
        tasks.reserve(batchEnd - batchStart);

        for (size_t i = batchStart; i < batchEnd; ++i) {
            auto sliceSpan = std::span<std::byte>{loaded.voxelsR8}.subspan(i * sliceVoxelCount,
                                                                           sliceVoxelCount);
            tasks.emplace_back(cppcoro::schedule_on(
                workerPool_,
                loadSliceR8(orderedSlices[i].path,
                            sliceSpan,
                            glm::uvec2{x, y},
                            i,
                            cancellationSource.token(),
                            &cancellationSource)));
        }

        auto completed = co_await cppcoro::when_all_ready(std::move(tasks));
        for (auto &task : completed) {
            auto result = task.result();
            if (!result) {
                cancellationSource.request_cancellation();
                co_return std::unexpected(std::move(result.error()));
            }
        }
    }

    co_return loaded;
}

} // namespace Cory
