#pragma once

#include <Cory/Base/Result.hpp>

#include <cppcoro/cancellation_source.hpp>
#include <cppcoro/cancellation_token.hpp>
#include <cppcoro/static_thread_pool.hpp>
#include <cppcoro/task.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace Cory {

struct LoadedVolume {
    glm::uvec3 dimensions{0u};
    std::vector<std::byte> voxelsR8{};
    std::vector<std::filesystem::path> orderedSlicePaths{};
};

struct LoadStackRequest {
    std::filesystem::path directory{};
    std::string pattern{"*.bmp"};
    size_t maxConcurrency{0};
};

/// Async dataset loading
class DatasetLoader {
  public:
    explicit DatasetLoader(size_t workerCount = std::thread::hardware_concurrency());
    ~DatasetLoader();

    [[nodiscard]] cppcoro::task<Result<LoadedVolume>> loadBmpStack(const LoadStackRequest &request);

  private:
    struct OrderedSlice {
        uint64_t index{};
        std::filesystem::path path{};
    };

    [[nodiscard]] static Result<std::vector<OrderedSlice>> scanSlices(const LoadStackRequest &request);

    [[nodiscard]] cppcoro::task<Result<void>> loadSliceR8(const std::filesystem::path &bmpPath,
                                                          std::span<std::byte> targetBuffer,
                                                          glm::uvec2 expectedDimensions,
                                                          size_t sliceIndex,
                                                          cppcoro::cancellation_token cancellationToken,
                                                          cppcoro::cancellation_source *cancellationSource);

    size_t workerCount_{1u};
    cppcoro::static_thread_pool workerPool_;
};

} // namespace Cory
