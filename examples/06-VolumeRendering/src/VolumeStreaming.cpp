#include "VolumeStreaming.hpp"

#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Context.hpp>

#include <KDGpu/texture.h>
#include <KDGpu/texture_options.h>
#include <KDGpu/texture_view_options.h>

#include <fmt/format.h>

#include <array>
#include <fstream>
#include <utility>

namespace {

[[nodiscard]] Gpu::Texture
createTexture3D(const std::string &label, const glm::uvec3 dimensions, Cory::Context &ctx)
{
    return ctx.device().createTexture(Gpu::TextureOptions{
        .label = label,
        .type = Gpu::TextureType::TextureType3D,
        .format = Gpu::Format::R8_UNORM,
        .extent =
            Gpu::Extent3D{
                .width = dimensions.x,
                .height = dimensions.y,
                .depth = dimensions.z,
            },
        .mipLevels = 1,
        .arrayLayers = 1,
        .usage = Gpu::TextureUsageFlagBits::SampledBit | Gpu::TextureUsageFlagBits::TransferDstBit,
        .memoryUsage = Gpu::MemoryUsage::GpuOnly,
        .sharingMode = Gpu::SharingMode::Exclusive,
        .queueTypeIndices = {},
        .initialLayout = Gpu::TextureLayout::Undefined,
        .externalMemoryHandleType = Gpu::ExternalMemoryHandleTypeFlagBits::None,
        .drmFormatModifiers = {},
        .createFlags = {},
    });
}

[[nodiscard]] Gpu::TextureView createTexture3DView(const std::string &label, Gpu::Texture &texture)
{
    return texture.createView(Gpu::TextureViewOptions{
        .label = label,
        .viewType = Gpu::ViewType::ViewType3D,
        .format = Gpu::Format::R8_UNORM,
        .range =
            Gpu::TextureSubresourceRange{
                .aspectMask = Gpu::TextureAspectFlagBits::ColorBit,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .yCbCrConversion = {},
    });
}

[[nodiscard]] std::vector<std::byte> readFileBytes(const std::filesystem::path &path,
                                                   std::string &errorOut)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        errorOut = fmt::format("Failed to open file '{}'", path.string());
        return {};
    }

    const auto endPos = file.tellg();
    if (endPos < 0) {
        errorOut = fmt::format("Failed to query file size '{}'", path.string());
        return {};
    }
    const auto size = static_cast<size_t>(endPos);

    std::vector<std::byte> bytes(size);
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size));
    if (!file.good() && !file.eof()) {
        errorOut = fmt::format("Failed while reading '{}'", path.string());
        return {};
    }
    return bytes;
}

} // namespace

VolumeStreaming::VolumeStreaming(Cory::Context &ctx)
    : ctx_{&ctx}
    , worker_{[this]() { workerLoop(); }}
{
}

VolumeStreaming::~VolumeStreaming()
{
    {
        std::scoped_lock lock(requestMutex_);
        stopWorker_ = true;
    }
    requestCv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool VolumeStreaming::registerDataset(const std::filesystem::path &manifestPath)
{
    VolumeManifest manifest{};
    std::string error{};
    if (!loadVolumeManifest(manifestPath, manifest, error)) {
        CO_CORE_ERROR("VolumeStreaming: {}", error);
        return false;
    }

    if (datasets_.contains(manifest.datasetId)) {
        CO_CORE_WARN("VolumeStreaming: dataset '{}' already registered, ignoring duplicate '{}'",
                     manifest.datasetId,
                     manifestPath.string());
        return false;
    }

    const auto datasetId = manifest.datasetId;
    auto [it, inserted] =
        datasets_.emplace(datasetId, DatasetRuntime{.manifest = std::move(manifest)});
    if (!inserted) {
        return false;
    }

    auto &dataset = it->second;
    dataset.state = StreamState::PendingPreviewRead;
    enqueueRead(dataset, VolumeLevel::Preview);
    CO_CORE_INFO("VolumeStreaming: queued preview load for '{}'", dataset.manifest.datasetId);
    return true;
}

void VolumeStreaming::poll(uint64_t frameNumber)
{
    if (ctx_ == nullptr) {
        return;
    }

    ctx_->uploader().poll();
    processReadResults();
    processUploadCompletion(frameNumber);
    retireOldVolumes(frameNumber);
}

std::optional<VolumeStreaming::ActiveVolume>
VolumeStreaming::activeVolume(std::string_view datasetId) const
{
    auto it = datasets_.find(std::string{datasetId});
    if (it == datasets_.end()) {
        return std::nullopt;
    }
    const auto &dataset = it->second;
    if (dataset.fullResident.has_value()) {
        return ActiveVolume{
            .view = dataset.fullResident->view.handle(),
            .dimensions = dataset.fullResident->dimensions,
            .fullQuality = true,
        };
    }
    if (dataset.previewResident.has_value()) {
        return ActiveVolume{
            .view = dataset.previewResident->view.handle(),
            .dimensions = dataset.previewResident->dimensions,
            .fullQuality = false,
        };
    }
    return std::nullopt;
}

std::string VolumeStreaming::datasetStatus(std::string_view datasetId) const
{
    auto it = datasets_.find(std::string{datasetId});
    if (it == datasets_.end()) {
        return "Unknown dataset";
    }
    if (it->second.state == StreamState::Error) {
        return it->second.error;
    }
    return stateToString(it->second.state);
}

std::vector<std::pair<std::string, std::string>> VolumeStreaming::allDatasetStatuses() const
{
    std::vector<std::pair<std::string, std::string>> statuses;
    statuses.reserve(datasets_.size());
    for (const auto &[datasetId, dataset] : datasets_) {
        statuses.emplace_back(datasetId,
                              dataset.state == StreamState::Error ? dataset.error
                                                                  : stateToString(dataset.state));
    }
    return statuses;
}

void VolumeStreaming::workerLoop()
{
    while (true) {
        ReadRequest request{};
        {
            std::unique_lock lock(requestMutex_);
            requestCv_.wait(lock, [this]() { return stopWorker_ || !pendingReads_.empty(); });
            if (stopWorker_ && pendingReads_.empty()) {
                return;
            }
            request = std::move(pendingReads_.front());
            pendingReads_.pop_front();
        }

        ReadResult result{
            .datasetId = request.datasetId,
            .level = request.level,
            .dimensions = request.dimensions,
        };

        std::string readError{};
        result.bytes = readFileBytes(request.blobPath, readError);
        if (!readError.empty()) {
            result.error = std::move(readError);
        }
        else if (!result.bytes.empty() && request.expectedByteSize != 0 &&
                 request.expectedByteSize != result.bytes.size()) {
            result.error = fmt::format("File '{}' size mismatch: expected {} bytes, got {} bytes",
                                       request.blobPath.string(),
                                       request.expectedByteSize,
                                       result.bytes.size());
        }

        {
            std::scoped_lock lock(resultMutex_);
            completedReads_.push_back(std::move(result));
        }
        CO_CORE_INFO("VolumeStreaming: worker completed {} read for '{}'",
                     request.level == VolumeLevel::Preview ? "preview" : "full",
                     request.datasetId);
    }
}

void VolumeStreaming::enqueueRead(DatasetRuntime &dataset, VolumeLevel level)
{
    const bool isPreview = level == VolumeLevel::Preview;
    const auto &blob = isPreview ? dataset.manifest.preview : dataset.manifest.full;

    {
        std::scoped_lock lock(requestMutex_);
        pendingReads_.push_back(ReadRequest{
            .datasetId = dataset.manifest.datasetId,
            .level = level,
            .blobPath = blob.path,
            .dimensions = blob.dimensions,
            .expectedByteSize = blob.byteSize,
        });
        if (isPreview) {
            dataset.previewQueued = true;
            dataset.state = StreamState::PendingPreviewRead;
        }
        else {
            dataset.fullQueued = true;
            dataset.state = StreamState::PendingFullRead;
        }
    }
    requestCv_.notify_one();
}

void VolumeStreaming::processReadResults()
{
    std::deque<ReadResult> results;
    {
        std::scoped_lock lock(resultMutex_);
        results.swap(completedReads_);
    }

    for (auto &result : results) {
        auto it = datasets_.find(result.datasetId);
        if (it == datasets_.end()) {
            CO_CORE_WARN("VolumeStreaming: result for unknown dataset '{}'", result.datasetId);
            continue;
        }
        auto &dataset = it->second;
        if (!result.error.empty()) {
            dataset.state = StreamState::Error;
            dataset.error = std::move(result.error);
            CO_CORE_ERROR("VolumeStreaming: dataset '{}' failed read: {}",
                          dataset.manifest.datasetId,
                          dataset.error);
            continue;
        }

        uploadBytesToVolume(dataset, result);
    }
}

void VolumeStreaming::uploadBytesToVolume(DatasetRuntime &dataset, const ReadResult &result)
{
    CO_CORE_ASSERT(ctx_ != nullptr, "VolumeStreaming context is null");
    const auto isPreview = result.level == VolumeLevel::Preview;
    const auto levelName = isPreview ? "preview" : "full";

    const auto textureLabel =
        fmt::format("VolumeStreaming {} texture ({})", dataset.manifest.datasetId, levelName);
    auto texture = createTexture3D(textureLabel, result.dimensions, *ctx_);
    auto view = createTexture3DView(textureLabel + " view", texture);

    std::vector<Gpu::BufferTextureCopyRegion> regions;
    regions.push_back(Gpu::BufferTextureCopyRegion{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .textureSubResource =
            {
                .aspectMask = Gpu::TextureAspectFlagBits::ColorBit,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .textureExtent =
            Gpu::Extent3D{
                .width = result.dimensions.x,
                .height = result.dimensions.y,
                .depth = result.dimensions.z,
            },
    });

    auto ticket = ctx_->uploader().enqueueImageUpload(Cory::AsyncUploader::ImageUploadRequest{
        .destinationTexture = texture.handle(),
        .data = result.bytes.data(),
        .byteSize = result.bytes.size(),
        .regions = std::move(regions),
        .oldLayout = Gpu::TextureLayout::Undefined,
        .finalLayout = Gpu::TextureLayout::ShaderReadOnlyOptimal,
        .finalStages = Gpu::PipelineStageFlagBit::ComputeShaderBit,
        .finalMask = Gpu::AccessFlagBit::ShaderReadBit,
    });

    auto upload = InFlightUpload{
        .volume =
            ResidentVolume{
                .texture = std::move(texture),
                .view = std::move(view),
                .dimensions = result.dimensions,
            },
        .ticket = std::move(ticket),
    };

    if (isPreview) {
        dataset.previewUpload = std::move(upload);
        dataset.state = StreamState::PreviewUploading;
        CO_CORE_INFO("VolumeStreaming: uploading preview for '{}'", dataset.manifest.datasetId);
    }
    else {
        dataset.fullUpload = std::move(upload);
        dataset.state = StreamState::FullUploading;
        CO_CORE_INFO("VolumeStreaming: uploading full volume for '{}'", dataset.manifest.datasetId);
    }
}

void VolumeStreaming::processUploadCompletion(uint64_t frameNumber)
{
    for (auto &[datasetId, dataset] : datasets_) {
        if (dataset.state == StreamState::Error) {
            continue;
        }

        if (dataset.previewUpload.has_value() && dataset.previewUpload->ticket.ready()) {
            dataset.previewResident = std::move(dataset.previewUpload->volume);
            dataset.previewUpload.reset();
            dataset.state = StreamState::PreviewReady;
            CO_CORE_INFO("VolumeStreaming: preview ready for '{}'", datasetId);

            if (!dataset.fullQueued) {
                enqueueRead(dataset, VolumeLevel::Full);
            }
        }

        if (dataset.fullUpload.has_value() && dataset.fullUpload->ticket.ready()) {
            dataset.fullResident = std::move(dataset.fullUpload->volume);
            dataset.fullUpload.reset();
            dataset.state = StreamState::FullReady;
            CO_CORE_INFO("VolumeStreaming: full volume ready for '{}'", datasetId);

            if (dataset.previewResident.has_value()) {
                retiredVolumes_.push_back(RetiredVolume{
                    .volume = std::move(*dataset.previewResident),
                    .retireFrame = frameNumber,
                });
                dataset.previewResident.reset();
            }
        }
    }
}

void VolumeStreaming::retireOldVolumes(uint64_t frameNumber)
{
    auto it = retiredVolumes_.begin();
    while (it != retiredVolumes_.end()) {
        if (frameNumber >= it->retireFrame &&
            frameNumber - it->retireFrame >= Cory::MAX_FRAMES_IN_FLIGHT) {
            it = retiredVolumes_.erase(it);
        }
        else {
            ++it;
        }
    }
}

std::string VolumeStreaming::stateToString(StreamState state)
{
    switch (state) {
    case StreamState::PendingPreviewRead:
        return "Loading preview (disk)";
    case StreamState::PreviewUploading:
        return "Uploading preview";
    case StreamState::PreviewReady:
        return "Preview ready";
    case StreamState::PendingFullRead:
        return "Loading full (disk)";
    case StreamState::FullUploading:
        return "Uploading full";
    case StreamState::FullReady:
        return "Full ready";
    case StreamState::Error:
        return "Error";
    }
    return "Unknown";
}
