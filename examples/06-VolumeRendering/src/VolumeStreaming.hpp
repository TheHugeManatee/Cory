#pragma once

#include "VolumeManifest.hpp"

#include <Cory/Renderer/AsyncUploader.hpp>
#include <Cory/Renderer/Common.hpp>

#include <KDGpu/texture.h>
#include <KDGpu/texture_view.h>

#include <glm/vec3.hpp>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Cory {
class Context;
}

class VolumeStreaming {
  public:
    struct ActiveVolume {
        Gpu::TextureViewHandle view{};
        glm::uvec3 dimensions{0u};
        bool fullQuality{false};
    };

    explicit VolumeStreaming(Cory::Context &ctx);
    ~VolumeStreaming();

    bool registerDataset(const std::filesystem::path &manifestPath);

    void poll(uint64_t frameNumber);

    [[nodiscard]] std::optional<ActiveVolume> activeVolume(std::string_view datasetId) const;
    [[nodiscard]] std::string datasetStatus(std::string_view datasetId) const;
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> allDatasetStatuses() const;

  private:
    enum class VolumeLevel : uint8_t { Preview, Full };
    enum class StreamState : uint8_t {
        PendingPreviewRead,
        PreviewUploading,
        PreviewReady,
        PendingFullRead,
        FullUploading,
        FullReady,
        Error,
    };

    struct ResidentVolume {
        Gpu::Texture texture{};
        Gpu::TextureView view{};
        glm::uvec3 dimensions{0u};
    };

    struct InFlightUpload {
        ResidentVolume volume{};
        Cory::AsyncUploader::UploadTicket ticket{};
    };

    struct DatasetRuntime {
        VolumeManifest manifest{};
        StreamState state{StreamState::PendingPreviewRead};
        std::string error{};

        bool previewQueued{false};
        bool fullQueued{false};

        std::optional<InFlightUpload> previewUpload{};
        std::optional<InFlightUpload> fullUpload{};

        std::optional<ResidentVolume> previewResident{};
        std::optional<ResidentVolume> fullResident{};
    };

    struct ReadRequest {
        std::string datasetId{};
        VolumeLevel level{VolumeLevel::Preview};
        std::filesystem::path blobPath{};
        glm::uvec3 dimensions{0u};
        size_t expectedByteSize{0};
    };

    struct ReadResult {
        std::string datasetId{};
        VolumeLevel level{VolumeLevel::Preview};
        glm::uvec3 dimensions{0u};
        std::vector<std::byte> bytes{};
        std::string error{};
    };

    struct RetiredVolume {
        ResidentVolume volume{};
        uint64_t retireFrame{0};
    };

    void workerLoop();
    void enqueueRead(DatasetRuntime &dataset, VolumeLevel level);
    void processReadResults();
    void processUploadCompletion(uint64_t frameNumber);
    void retireOldVolumes(uint64_t frameNumber);
    void uploadBytesToVolume(DatasetRuntime &dataset, const ReadResult &result);
    static std::string stateToString(StreamState state);

    Cory::Context *ctx_{nullptr};
    std::unordered_map<std::string, DatasetRuntime> datasets_{};

    std::mutex requestMutex_{};
    std::condition_variable requestCv_{};
    std::deque<ReadRequest> pendingReads_{};

    std::mutex resultMutex_{};
    std::deque<ReadResult> completedReads_{};

    std::vector<RetiredVolume> retiredVolumes_{};

    bool stopWorker_{false};
    std::jthread worker_;
};
