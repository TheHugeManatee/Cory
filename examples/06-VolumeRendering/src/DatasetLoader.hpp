#pragma once

#include <Cory/Base/Function.hpp>
#include <Cory/Base/Result.hpp>
#include <Cory/Renderer/StagingUploader.hpp>

#include <cppcoro/cancellation_source.hpp>
#include <cppcoro/cancellation_token.hpp>
#include <cppcoro/static_thread_pool.hpp>
#include <cppcoro/task.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstddef>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace Cory {

struct LoadedVolume {
    glm::uvec3 dimensions{0u};
    std::vector<std::byte> voxelsR8{};
    std::vector<std::filesystem::path> orderedSlicePaths{};
};

struct StreamedVolume {
    glm::uvec3 dimensions{0u};
    std::vector<std::filesystem::path> orderedSlicePaths{};
};

struct SliceLoadUpdate {
    glm::uvec3 volumeDimensions{0u};
    size_t sliceIndex{};
    std::filesystem::path slicePath{};
    std::vector<std::byte> voxelsR8{};
};

struct StagedSliceLoadUpdate {
    glm::uvec3 volumeDimensions{0u};
    size_t sliceIndex{};
    std::filesystem::path slicePath{};
    StagingSlot stagingSlot{};
};

struct LoadStackRequest {
    std::filesystem::path directory{};
    std::string pattern{"*.bmp"};
    size_t maxConcurrency{0};
};

using SliceLoadedCallback = Function<Result<void>(SliceLoadUpdate &&update)>;
using StagedSliceLoadedCallback = Function<Result<void>(StagedSliceLoadUpdate &&update)>;

/// Async dataset loading directly to an AsyncUploader
/// Uses a thread pool to load and convert slices in parallel.
class DatasetLoader {
  public:
    explicit DatasetLoader(size_t workerCount = std::thread::hardware_concurrency());
    ~DatasetLoader();

    [[nodiscard]] cppcoro::task<Result<StreamedVolume>>
    streamBmpStackToUploader(const LoadStackRequest &request,
                             IStagingUploader &uploader,
                             StagedSliceLoadedCallback onSliceLoaded);

  private:
    struct OrderedSlice {
        uint64_t sliceNumber{};
        size_t index{};
        std::filesystem::path path{};
    };

    [[nodiscard]] static Result<std::vector<OrderedSlice>>
    scanSlices(const LoadStackRequest &request);

    [[nodiscard]] cppcoro::task<Result<void>>
    loadSliceR8ToStaging(const std::filesystem::path &bmpPath,
                         glm::uvec2 expectedDimensions,
                         glm::uvec3 volumeDimensions,
                         size_t sliceIndex,
                         cppcoro::cancellation_token cancellationToken,
                         cppcoro::cancellation_source *cancellationSource,
                         IStagingUploader *uploader,
                         StagedSliceLoadedCallback *onSliceLoaded);

    size_t workerCount_{1u};
    cppcoro::static_thread_pool workerPool_;
};

} // namespace Cory
