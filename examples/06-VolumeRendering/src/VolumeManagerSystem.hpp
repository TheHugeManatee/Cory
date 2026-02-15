#pragma once

#include "Common.hpp"
#include "DatasetLoader.hpp"
#include "VolumeManifest.hpp"

#include <Cory/Renderer/AsyncUploader.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/ShaderHotReloader.hpp>
#include <Cory/SceneGraph/SceneGraph.hpp>

#include <KDGpu/texture.h>
#include <KDGpu/texture_view.h>

#include <cppcoro/async_scope.hpp>
#include <cppcoro/task.hpp>

#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Cory {
class Context;
}

/**
 * @brief Owns volume asset lifecycle and writes runtime texture state into scene components.
 *
 * Responsibilities:
 * - Watches `StreamedVolume` components and asynchronously loads dataset preview/full volumes.
 * - Watches `ProceduralVolume` components and generates 3D textures via compute.
 * - Creates/updates `VolumeComponent` on matching entities with runtime texture handles and
 *   dimensions consumed by `VolumeRenderSystem`.
 *
 * Ownership boundary:
 * - This system owns texture resources and streaming/generation state.
 * - `VolumeRenderSystem` is render-only and never allocates/generates volume textures.
 *
 * Tick ordering contract:
 * - Must tick before `VolumeRenderSystem` each frame so `VolumeComponent` is up to date.
 */
class VolumeManagerSystem {
  public:
    explicit VolumeManagerSystem(Cory::Context &ctx);
    ~VolumeManagerSystem();

    /**
     * @brief Advances streaming/generation state and synchronizes component runtime fields.
     *
     * Consumes:
     * - `StreamedVolume`
     * - `ProceduralVolume`
     *
     * Produces/updates:
     * - `VolumeComponent::textureView`
     * - `VolumeComponent::textureDimensions`
     * - `VolumeComponent::hasTexture`
     * - `VolumeComponent::fullQuality`
     */
    void tick(Cory::SceneGraph &graph, Cory::TickInfo tickInfo);

    /// Human-readable status per streamed dataset id (for UI/debugging).
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> datasetStatuses() const;

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

    /// GPU-resident 3D volume texture/view pair.
    struct ResidentVolume {
        Gpu::Texture texture{};
        Gpu::TextureView view{};
        glm::uvec3 dimensions{0u};
    };

    /// Async transfer state for streamed dataset uploads.
    struct InFlightUpload {
        ResidentVolume volume{};
        Cory::AsyncUploader::UploadTicket ticket{};
    };

    /// Runtime state for one streamed dataset (`StreamedVolume::datasetId`).
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

    /// Runtime state for one procedural entity (`ProceduralVolume`).
    struct ProceduralRuntime {
        std::optional<ResidentVolume> resident{};
        glm::uvec3 generatedDimensions{0u};
        float generatedDensityScale{1.0f};
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

    void enqueueRead(DatasetRuntime &dataset, VolumeLevel level);
    [[nodiscard]] cppcoro::task<void>
    loadAndQueueResult(std::string datasetId, VolumeLevel level, Cory::LoadStackRequest request);
    void processReadResults();
    void processUploadCompletion(uint64_t frameNumber);
    void retireOldVolumes(uint64_t frameNumber);
    void uploadBytesToVolume(DatasetRuntime &dataset, const ReadResult &result);
    static std::string stateToString(StreamState state);

    /// Ensures `datasets_` contains runtime state for this `StreamedVolume`.
    void ensureDatasetRegistered(const StreamedVolume &streamedVolume);
    /// Applies streamed dataset runtime textures onto entity `VolumeComponent`.
    void updateStreamedEntities(Cory::SceneGraph &graph);
    /// Generates/updates procedural textures and applies them onto `VolumeComponent`.
    void updateProceduralEntities(Cory::SceneGraph &graph, uint64_t frameNumber, float timeSeconds);
    void enqueueProceduralGeneration(Cory::Entity entity,
                                     ProceduralVolume procedural,
                                     uint64_t frameNumber,
                                     float timeSeconds);

    Cory::Context *ctx_{nullptr};
    Cory::ShaderHandle createVolumeShader_{};
    Cory::ShaderHotReloader shaderHotReloader_{};
    std::unordered_map<std::string, DatasetRuntime> datasets_{};
    std::unordered_map<Cory::Entity, ProceduralRuntime> proceduralVolumes_{};

    std::mutex resultMutex_{};
    std::deque<ReadResult> completedReads_{};

    std::vector<RetiredVolume> retiredVolumes_{};

    cppcoro::async_scope readScope_{};
    Cory::DatasetLoader datasetLoader_{};
};
