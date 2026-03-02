#include "DatasetLoader.hpp"

#include <Cory/Base/CoroBatch.hpp>
#include <Cory/Base/CoroOps.hpp>
#include <Cory/IO/Bmp.hpp>
#include <Cory/Renderer/ThreadScheduler.hpp>

#include <fmt/format.h>
#include <mio/mmap.hpp>

#include <Cory/Base/Log.hpp>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <stop_token>

namespace Cory {
namespace {

[[nodiscard]] bool wildcardMatch(std::string_view pattern, std::string_view text)
{
    auto lower = [](char c) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    };

    size_t patternIndex = 0;
    size_t textIndex = 0;
    size_t starIndex = std::string_view::npos;
    size_t backtrackTextIndex = 0;

    while (textIndex < text.size()) {
        if (patternIndex < pattern.size() &&
            (pattern[patternIndex] == '?' ||
             lower(pattern[patternIndex]) == lower(text[textIndex]))) {
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
    }
    catch (...) {
        return std::nullopt;
    }
}

[[nodiscard]] Result<IO::BmpInfo> querySliceInfo(const std::filesystem::path &bmpPath)
{
    std::error_code ec;
    auto bmpFile = mio::make_mmap_source(bmpPath.string(), ec);
    if (ec) {
        return std::unexpected(fmt::format(
            "Failed to memory-map first slice '{}' ({})", bmpPath.string(), ec.message()));
    }

    std::span bmpBytes{reinterpret_cast<const std::byte *>(bmpFile.data()), bmpFile.size()};
    auto bmpInfo = IO::queryBmpInfo(bmpBytes);
    if (!bmpInfo) {
        return std::unexpected(fmt::format("Failed to query BMP info from first slice '{}' ({})",
                                           bmpPath.string(),
                                           bmpInfo.error()));
    }

    return bmpInfo;
}

} // namespace

DatasetLoader::DatasetLoader(size_t workerCount)
    : workerCount_{std::max<size_t>(1u, workerCount)}
    , workerPool_{workerCount_}
{
}

DatasetLoader::~DatasetLoader() = default;

Result<std::vector<DatasetLoader::OrderedSlice>>
DatasetLoader::scanSlices(const LoadStackRequest &request)
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
            return std::unexpected(fmt::format(
                "Matched file '{}' has no trailing numeric slice index", entry.path().string()));
        }

        orderedSlices.push_back(OrderedSlice{
            .sliceNumber = *sliceIndex,
            .index = 0u,
            .path = entry.path(),
        });
    }

    if (ec) {
        return std::unexpected(fmt::format(
            "Failed while scanning directory '{}' ({})", request.directory.string(), ec.message()));
    }

    if (orderedSlices.empty()) {
        return std::unexpected(fmt::format(
            "No files matching '{}' found in '{}'", request.pattern, request.directory.string()));
    }

    std::sort(orderedSlices.begin(),
              orderedSlices.end(),
              [](const OrderedSlice &a, const OrderedSlice &b) {
                  if (a.sliceNumber != b.sliceNumber) {
                      return a.sliceNumber < b.sliceNumber;
                  }
                  return a.path < b.path;
              });

    for (size_t i = 0; i < orderedSlices.size(); ++i) {
        orderedSlices[i].index = i;
    }

    for (size_t i = 1; i < orderedSlices.size(); ++i) {
        const auto prev = orderedSlices[i - 1].sliceNumber;
        const auto current = orderedSlices[i].sliceNumber;
        if (current == prev) {
            return std::unexpected(fmt::format("Duplicate slice index {} for '{}' and '{}'",
                                               current,
                                               orderedSlices[i - 1].path.string(),
                                               orderedSlices[i].path.string()));
        }

        if (current != prev + 1u) {
            return std::unexpected(
                fmt::format("Slice index gap: expected {} after {}, found {} ('{}')",
                            prev + 1u,
                            prev,
                            current,
                            orderedSlices[i].path.string()));
        }
    }

    return orderedSlices;
}

cppcoro::task<Result<void>>
DatasetLoader::loadSliceR8ToStaging(const std::filesystem::path &bmpPath,
                                    glm::uvec2 expectedDimensions,
                                    glm::uvec3 volumeDimensions,
                                    size_t sliceIndex,
                                    std::stop_token cancellationToken,
                                    std::stop_source *cancellationSource,
                                    IStagingUploader *uploader,
                                    StagedSliceLoadedCallback *onSliceLoaded)
{
    auto failAndCancel = [&](std::string message) -> Result<void> {
        if (cancellationSource != nullptr) {
            cancellationSource->request_stop();
        }
        return std::unexpected(std::move(message));
    };

    if (uploader == nullptr) {
        co_return failAndCancel(
            fmt::format("Slice {} ('{}') staging uploader is null", sliceIndex, bmpPath.string()));
    }
    auto *threadScheduler = uploader->threadScheduler();
    if (threadScheduler == nullptr) {
        co_return failAndCancel(fmt::format(
            "Slice {} ('{}') uploader thread scheduler is null", sliceIndex, bmpPath.string()));
    }

    if (cancellationToken.stop_requested()) {
        co_return std::unexpected(
            fmt::format("Slice {} ('{}') cancelled before decode", sliceIndex, bmpPath.string()));
    }

    std::error_code ec;
    auto bmpFile = mio::make_mmap_source(bmpPath.string(), ec);
    if (ec) {
        co_return failAndCancel(fmt::format(
            "Slice {} ('{}') mmap failed: {}", sliceIndex, bmpPath.string(), ec.message()));
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

    const auto expectedVoxels =
        static_cast<size_t>(expectedDimensions.x) * static_cast<size_t>(expectedDimensions.y);

    co_await threadScheduler->schedule();

    if (cancellationToken.stop_requested()) {
        co_return std::unexpected(fmt::format(
            "Slice {} ('{}') cancelled before staging acquire", sliceIndex, bmpPath.string()));
    }

    auto slotResult = co_await uploader->acquireStaging(expectedVoxels);
    if (!slotResult) {
        co_return failAndCancel(fmt::format("Slice {} ('{}') failed to acquire staging slot: {}",
                                            sliceIndex,
                                            bmpPath.string(),
                                            slotResult.error()));
    }

    auto stagingSlot = std::move(*slotResult);
    if (!uploader->validStaging(stagingSlot) || stagingSlot.byteSize < expectedVoxels) {
        const auto slotByteSize = stagingSlot.byteSize;
        uploader->recycleStaging(std::move(stagingSlot));
        co_return failAndCancel(
            fmt::format("Slice {} ('{}') staging slot is invalid or too small ({} < {})",
                        sliceIndex,
                        bmpPath.string(),
                        slotByteSize,
                        expectedVoxels));
    }

    co_await workerPool_.schedule();

    if (cancellationToken.stop_requested()) {
        co_await threadScheduler->schedule();
        uploader->recycleStaging(std::move(stagingSlot));
        co_return std::unexpected(
            fmt::format("Slice {} ('{}') cancelled before decode", sliceIndex, bmpPath.string()));
    }

    auto *mapped = reinterpret_cast<std::byte *>(stagingSlot.userData);
    if (mapped == nullptr) {
        co_await threadScheduler->schedule();
        uploader->recycleStaging(std::move(stagingSlot));
        co_return failAndCancel(
            fmt::format("Slice {} ('{}') staging map returned null", sliceIndex, bmpPath.string()));
    }
    auto target = std::span<std::byte>{mapped, expectedVoxels};
    auto decoded = IO::decodeBmp(bmpBytes, target);

    if (!decoded) {
        co_await threadScheduler->schedule();
        uploader->recycleStaging(std::move(stagingSlot));
        co_return failAndCancel(fmt::format(
            "Slice {} ('{}') decode failed: {}", sliceIndex, bmpPath.string(), decoded.error()));
    }

    if (onSliceLoaded != nullptr && *onSliceLoaded) {
        auto update = StagedSliceLoadUpdate{
            .volumeDimensions = volumeDimensions,
            .sliceIndex = sliceIndex,
            .slicePath = bmpPath,
            .stagingSlot = std::move(stagingSlot),
        };
        auto callbackResult = (*onSliceLoaded)(std::move(update));
        if (!callbackResult) {
            co_return failAndCancel(fmt::format("Slice {} ('{}') callback failed: {}",
                                                sliceIndex,
                                                bmpPath.string(),
                                                callbackResult.error()));
        }
    }
    else {
        co_await threadScheduler->schedule();
        uploader->recycleStaging(std::move(stagingSlot));
    }

    co_return Result<void>{};
}

cppcoro::task<Result<StreamedVolume>>
DatasetLoader::streamBmpStackToUploader(const LoadStackRequest &request,
                                        IStagingUploader &uploader,
                                        StagedSliceLoadedCallback onSliceLoaded)
{
    auto scanResult = scanSlices(request);
    if (!scanResult) {
        co_return std::unexpected(std::move(scanResult.error()));
    }

    auto orderedSlices = std::move(*scanResult);
    const auto originalSliceCount = orderedSlices.size();
    if (request.sliceSubsampleFactor == 0u) {
        co_return std::unexpected("Slice subsample factor must be >= 1");
    }

    if (request.sliceSubsampleFactor > 1u) {
        std::vector<OrderedSlice> subsampled;
        subsampled.reserve((orderedSlices.size() + request.sliceSubsampleFactor - 1u) /
                           request.sliceSubsampleFactor);

        for (size_t i = 0; i < orderedSlices.size(); i += request.sliceSubsampleFactor) {
            subsampled.push_back(std::move(orderedSlices[i]));
        }
        orderedSlices = std::move(subsampled);

        for (size_t i = 0; i < orderedSlices.size(); ++i) {
            orderedSlices[i].index = i;
        }
    }

    if (orderedSlices.size() != originalSliceCount) {
        CO_CORE_INFO("DatasetLoader: applying slice subsampling factor {} ({} -> {} slices)",
                     request.sliceSubsampleFactor,
                     originalSliceCount,
                     orderedSlices.size());
    }

    // re-shuffle the slices to prioritize those with indices that are multiples of 8, to improve
    // progressive loading quality
    std::ranges::stable_sort(orderedSlices, {}, [](const OrderedSlice &slice) {
        return (slice.sliceNumber % 8) == 0 ? 0 : 1;
    });

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
    if (sliceVoxelCount64 > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
        co_return std::unexpected(
            fmt::format("Volume {}x{}x{} exceeds host memory addressable range", x, y, z));
    }

    auto streamed = StreamedVolume{
        .dimensions = glm::uvec3{x, y, static_cast<uint32_t>(z)},
        .orderedSlicePaths = {},
    };
    streamed.orderedSlicePaths.reserve(orderedSlices.size());
    for (const auto &slice : orderedSlices) {
        streamed.orderedSlicePaths.push_back(slice.path);
    }

    auto cancellationSource = std::stop_source{};

    const auto requestedConcurrency =
        request.maxConcurrency == 0 ? workerCount_ : request.maxConcurrency;
    const auto effectiveConcurrency =
        std::max<size_t>(1u, std::min(workerCount_, requestedConcurrency));

    for (size_t batchStart = 0; batchStart < orderedSlices.size();
         batchStart += effectiveConcurrency) {
        if (cancellationSource.stop_requested()) {
            co_return std::unexpected("Volume load cancelled");
        }

        const auto batchEnd = std::min(orderedSlices.size(), batchStart + effectiveConcurrency);
        std::vector<cppcoro::task<Result<void>>> tasks;
        tasks.reserve(batchEnd - batchStart);

        for (size_t i = batchStart; i < batchEnd; ++i) {
            tasks.emplace_back(resume_on(workerPool_,
                                         loadSliceR8ToStaging(orderedSlices[i].path,
                                                              glm::uvec2{x, y},
                                                              streamed.dimensions,
                                                              orderedSlices[i].index,
                                                              cancellationSource.get_token(),
                                                              &cancellationSource,
                                                              &uploader,
                                                              &onSliceLoaded)));
        }

        auto batchResult = co_await when_all_fail_fast(std::move(tasks), cancellationSource);
        if (!batchResult) {
            co_return std::unexpected(std::move(batchResult.error()));
        }
    }

    co_return streamed;
}

} // namespace Cory
