#include "VolumeManagerSystem.hpp"

#include <Cory/Base/Log.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Framegraph/ShaderBindingContext.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/PipelineCache.hpp>
#include <Cory/Renderer/Shader.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include <cppcoro/sync_wait.hpp>

#include <KDGpu/command_recorder.h>
#include <KDGpu/compute_pass_command_recorder.h>
#include <KDGpu/texture.h>
#include <KDGpu/texture_options.h>
#include <KDGpu/texture_view_options.h>

#include <fmt/format.h>

#include <Cory/Base/Profiling.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace {

[[nodiscard]] Gpu::Texture createTexture3D(const std::string &label,
                                           glm::uvec3 dimensions,
                                           Cory::Context &ctx,
                                           Gpu::TextureUsageFlags usage)
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
        .usage = usage,
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

struct alignas(16) VolumeGenerationParams {
    glm::vec3 volumeSpacing{1.0f};
    float densityScale{1.0f};
    glm::uvec3 volumeDimensions{64u, 64u, 64u};
    float time{0.0f};
};
static_assert(std::is_trivially_copyable_v<VolumeGenerationParams>);

constexpr size_t kManagerDrawDataBufferSize = 64u * 1024u;
constexpr size_t kMaxSliceUploadsInFlight = 64u;

[[nodiscard]] Gpu::TextureSubresourceRange colorSubresourceRange()
{
    return Gpu::TextureSubresourceRange{
        .aspectMask = Gpu::TextureAspectFlagBits::ColorBit,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
}

} // namespace

VolumeManagerSystem::VolumeManagerSystem(Cory::Context &ctx)
    : ctx_{&ctx}
{
    const auto createVolumePath = Cory::ResourceLocator::Locate("create_volume.comp.slang");
    if (!createVolumePath.has_value()) {
        throw std::runtime_error(createVolumePath.error());
    }

    shaderHotReloader_.initialize(ctx);
    shaderHotReloader_.addShader({
        .path = *createVolumePath,
        .stage = Gpu::ShaderStageFlagBits::ComputeBit,
        .label = "create_volume.comp.slang",
        .shaderHandle = &createVolumeShader_,
    });
}

VolumeManagerSystem::~VolumeManagerSystem()
{
    cppcoro::sync_wait(readScope_.join());
}

void VolumeManagerSystem::tick(Cory::SceneGraph &graph, Cory::TickInfo tickInfo)
{
    Cory::ScopeTimer timer("VolumeManagerSystem::tick");
    if (ctx_ == nullptr) {
        return;
    }
    const auto currentTime = tickInfo.realNow.time_since_epoch().count();

    shaderHotReloader_.processPendingReloads(tickInfo.ticks);
    ctx_->uploader().poll();
    processSliceResults();
    processReadResults(currentTime);
    processUploadCompletion(tickInfo.ticks, currentTime);
    updateStreamedEntities(graph, currentTime);
    updateProceduralEntities(graph, tickInfo.ticks, currentTime);
    retireOldVolumes(tickInfo.ticks);
}

std::vector<std::pair<std::string, std::string>> VolumeManagerSystem::datasetStatuses() const
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

cppcoro::task<void> VolumeManagerSystem::loadAndQueueResult(std::string datasetId,
                                                            VolumeLevel level,
                                                            Cory::LoadStackRequest request)
{
    CO_CORE_ASSERT(ctx_ != nullptr, "VolumeManager context is null");
    const auto datasetKey = datasetId;
    auto result = ReadResult{
        .datasetId = std::move(datasetId),
        .level = level,
    };

    try {
        auto stackResult = co_await datasetLoader_.streamBmpStackToUploader(
            request,
            ctx_->uploader(),
            [this, datasetKey, level](Cory::StagedSliceLoadUpdate &&update) {
                auto sliceResult = SliceResult{
                    .datasetId = datasetKey,
                    .level = level,
                    .dimensions = update.volumeDimensions,
                    .sliceIndex = update.sliceIndex,
                    .stagingSlot = std::move(update.stagingSlot),
                };
                {
                    std::scoped_lock lock(sliceResultMutex_);
                    completedSliceReads_.push_back(std::move(sliceResult));
                }
                return Cory::Result<void>{};
            });
        if (!stackResult) {
            result.error = std::move(stackResult.error());
        }
        else {
            result.dimensions = stackResult->dimensions;
        }
    }
    catch (const std::exception &e) {
        result.error = fmt::format(
            "Unhandled exception while loading dataset '{}': {}", result.datasetId, e.what());
    }
    catch (...) {
        result.error =
            fmt::format("Unhandled unknown exception while loading dataset '{}'", result.datasetId);
    }

    {
        std::scoped_lock lock(resultMutex_);
        completedReads_.push_back(std::move(result));
    }
}

void VolumeManagerSystem::enqueueRead(DatasetRuntime &dataset, VolumeLevel level)
{
    if (!dataset.manifest.bmpStack.has_value()) {
        dataset.state = StreamState::Error;
        dataset.error = fmt::format("Dataset '{}' is missing required bmp_stack configuration",
                                    dataset.manifest.datasetId);
        return;
    }

    const auto isPreview = level == VolumeLevel::Preview;
    const auto stackRequest = Cory::LoadStackRequest{
        .directory = dataset.manifest.bmpStack->directory,
        .pattern = dataset.manifest.bmpStack->pattern,
        .maxConcurrency = dataset.manifest.bmpStack->maxConcurrency,
        .sliceSubsampleFactor = dataset.sliceSubsampleFactor,
    };

    readScope_.spawn(loadAndQueueResult(dataset.manifest.datasetId, level, stackRequest));

    if (isPreview) {
        dataset.previewQueued = true;
        dataset.state = StreamState::PendingPreviewRead;
    }
    else {
        dataset.fullQueued = true;
        dataset.state = StreamState::PendingFullRead;
    }
}

void VolumeManagerSystem::processReadResults(double currentTime)
{
    std::vector<ReadResult> results;
    {
        std::scoped_lock lock(resultMutex_);
        results.swap(completedReads_);
    }

    for (auto &result : results) {
        auto it = datasets_.find(result.datasetId);
        if (it == datasets_.end()) {
            continue;
        }
        auto &dataset = it->second;
        if (!result.error.empty()) {
            dataset.state = StreamState::Error;
            dataset.error = std::move(result.error);
            CO_CORE_ERROR("VolumeManager: dataset '{}' failed read: {}",
                          dataset.manifest.datasetId,
                          dataset.error);
            continue;
        }

        dataset.loadCompleted = true;
        if (dataset.expectedSlices == 0u && result.dimensions.z > 0u) {
            dataset.expectedSlices = result.dimensions.z;
        }

        if (!dataset.fullResident.has_value()) {
            dataset.state = StreamState::Error;
            dataset.error =
                fmt::format("Dataset '{}' load completed but no slice uploads were applied",
                            dataset.manifest.datasetId);
            CO_CORE_ERROR("VolumeManager: {}", dataset.error);
            continue;
        }

        if (dataset.expectedSlices > dataset.uploadedSlices &&
            dataset.inFlightSliceUploads.empty() && dataset.pendingSliceUploads.empty()) {
            dataset.state = StreamState::Error;
            dataset.error = fmt::format(
                "Dataset '{}' load completed with incomplete uploads: uploaded {} of {} slices",
                dataset.manifest.datasetId,
                dataset.uploadedSlices,
                dataset.expectedSlices);
            CO_CORE_ERROR("VolumeManager: {}", dataset.error);
            continue;
        }

        if (dataset.expectedSlices > 0u && dataset.uploadedSlices == dataset.expectedSlices &&
            dataset.inFlightSliceUploads.empty() && dataset.pendingSliceUploads.empty() &&
            dataset.fullResident.has_value() && dataset.state != StreamState::FullReady) {
            dataset.state = StreamState::FullReady;
            const auto loadingTime =
                std::max(currentTime - dataset.loadingStartedTimeSeconds, 1e-6);
            const auto dimensions = dataset.fullResident->dimensions;
            const auto totalBytes = static_cast<double>(dimensions.x) *
                                    static_cast<double>(dimensions.y) *
                                    static_cast<double>(dimensions.z);
            const auto totalMiB = totalBytes / (1024.0 * 1024.0);
            const auto throughputMiBps = totalMiB / loadingTime;
            CO_CORE_INFO("VolumeManager: full volume ready for '{}'. slices={} size={:.2f} MiB "
                         "rate={:.2f} MiB/s time={:.3f}s",
                         dataset.manifest.datasetId,
                         dataset.expectedSlices,
                         totalMiB,
                         throughputMiBps,
                         loadingTime);
        }
    }
}

void VolumeManagerSystem::processSliceResults()
{
    std::vector<SliceResult> results;
    results.reserve(1024);
    {
        std::scoped_lock lock(sliceResultMutex_);
        results.swap(completedSliceReads_);
    }

    for (auto &result : results) {
        auto it = datasets_.find(result.datasetId);
        if (it == datasets_.end()) {
            if (ctx_ != nullptr && result.stagingSlot.valid()) {
                ctx_->uploader().recycleStaging(std::move(result.stagingSlot));
            }
            CO_CORE_WARN("VolumeManager: dropping slice {} for unknown dataset '{}'",
                         result.sliceIndex,
                         result.datasetId);
            continue;
        }
        auto &dataset = it->second;
        if (dataset.state == StreamState::Error) {
            if (ctx_ != nullptr && result.stagingSlot.valid()) {
                ctx_->uploader().recycleStaging(std::move(result.stagingSlot));
            }
            continue;
        }
        if (!result.error.empty()) {
            if (ctx_ != nullptr && result.stagingSlot.valid()) {
                ctx_->uploader().recycleStaging(std::move(result.stagingSlot));
            }
            dataset.state = StreamState::Error;
            dataset.error = std::move(result.error);
            CO_CORE_ERROR("VolumeManager: dataset '{}' failed slice read: {}",
                          dataset.manifest.datasetId,
                          dataset.error);
            continue;
        }

        if (dataset.fullResident.has_value()) {
            if (dataset.fullResident->dimensions != result.dimensions) {
                dataset.state = StreamState::Error;
                dataset.error = fmt::format("Dataset '{}' reported inconsistent dimensions: "
                                            "existing {}x{}x{}, got {}x{}x{}",
                                            dataset.manifest.datasetId,
                                            dataset.fullResident->dimensions.x,
                                            dataset.fullResident->dimensions.y,
                                            dataset.fullResident->dimensions.z,
                                            result.dimensions.x,
                                            result.dimensions.y,
                                            result.dimensions.z);
                CO_CORE_ERROR("VolumeManager: {}", dataset.error);
                if (ctx_ != nullptr && result.stagingSlot.valid()) {
                    ctx_->uploader().recycleStaging(std::move(result.stagingSlot));
                }
                continue;
            }
        }
        else {
            CO_CORE_ASSERT(ctx_ != nullptr, "VolumeManager context is null");
            const auto textureLabel =
                fmt::format("VolumeManager {} texture (full)", dataset.manifest.datasetId);
            auto texture = createTexture3D(textureLabel,
                                           result.dimensions,
                                           *ctx_,
                                           Gpu::TextureUsageFlagBits::SampledBit |
                                               Gpu::TextureUsageFlagBits::TransferDstBit);
            auto view = createTexture3DView(textureLabel + " view", texture);
            dataset.fullResident = ResidentVolume{
                .texture = std::move(texture),
                .view = std::move(view),
                .dimensions = result.dimensions,
            };
            dataset.state = StreamState::FullUploading;
        }

        if (dataset.expectedSlices == 0u) {
            dataset.expectedSlices = result.dimensions.z;
        }
        dataset.pendingSliceUploads.push_back(DatasetRuntime::PendingSliceUpload{
            .sliceIndex = result.sliceIndex,
            .stagingSlot = std::move(result.stagingSlot),
        });
        startNextSliceUpload(dataset);
    }
}

void VolumeManagerSystem::processUploadCompletion(uint64_t frameNumber, double currentTime)
{
    for (auto &[datasetId, dataset] : datasets_) {
        (void)frameNumber;
        if (dataset.state == StreamState::Error) {
            continue;
        }

        auto readyUploads = uint32_t{0u};
        auto uploadIt = dataset.inFlightSliceUploads.begin();
        while (uploadIt != dataset.inFlightSliceUploads.end()) {
            if (uploadIt->ready()) {
                uploadIt = dataset.inFlightSliceUploads.erase(uploadIt);
                ++readyUploads;
                continue;
            }
            ++uploadIt;
        }

        if (readyUploads > 0u) {
            const auto wasUploaded = dataset.uploadedSlices;
            dataset.uploadedSlices += readyUploads;
            if (wasUploaded == 0u) {
                CO_CORE_INFO(
                    "VolumeManager: first slice uploaded for '{}', partial volume now renderable",
                    datasetId);
            }
        }

        startNextSliceUpload(dataset);

        if (dataset.loadCompleted && dataset.expectedSlices > 0u &&
            dataset.uploadedSlices == dataset.expectedSlices &&
            dataset.inFlightSliceUploads.empty() && dataset.pendingSliceUploads.empty() &&
            dataset.fullResident.has_value() && dataset.state != StreamState::FullReady) {
            dataset.state = StreamState::FullReady;
            const auto loadingTime =
                std::max(currentTime - dataset.loadingStartedTimeSeconds, 1e-6);
            const auto dimensions = dataset.fullResident->dimensions;
            const auto totalBytes = static_cast<double>(dimensions.x) *
                                    static_cast<double>(dimensions.y) *
                                    static_cast<double>(dimensions.z);
            const auto totalMiB = totalBytes / (1024.0 * 1024.0);
            const auto throughputMiBps = totalMiB / loadingTime;
            CO_CORE_INFO("VolumeManager: loading of '{}' finished. slices={} size={:.2f} MiB "
                         "rate={:.2f} MiB/s time={:.3f}s",
                         datasetId,
                         dataset.expectedSlices,
                         totalMiB,
                         throughputMiBps,
                         loadingTime);
        }
    }
}

void VolumeManagerSystem::startNextSliceUpload(DatasetRuntime &dataset)
{
    if (dataset.pendingSliceUploads.empty()) {
        return;
    }
    CO_CORE_ASSERT(ctx_ != nullptr, "VolumeManager context is null");
    CO_CORE_ASSERT(dataset.fullResident.has_value(),
                   "Slice upload requested before full resident texture was created");

    while (dataset.inFlightSliceUploads.size() < kMaxSliceUploadsInFlight &&
           !dataset.pendingSliceUploads.empty()) {
        auto pending = std::move(dataset.pendingSliceUploads.front());
        dataset.pendingSliceUploads.pop_front();
        auto sliceIndex = pending.sliceIndex;
        auto stagingSlot = std::move(pending.stagingSlot);

        const auto expectedSliceBytes = static_cast<size_t>(dataset.fullResident->dimensions.x) *
                                        static_cast<size_t>(dataset.fullResident->dimensions.y);
        if (stagingSlot.byteSize != expectedSliceBytes) {
            dataset.state = StreamState::Error;
            dataset.error = fmt::format("Dataset '{}' slice {} has {} bytes, expected {} bytes",
                                        dataset.manifest.datasetId,
                                        sliceIndex,
                                        stagingSlot.byteSize,
                                        expectedSliceBytes);
            ctx_->uploader().recycleStaging(std::move(stagingSlot));
            CO_CORE_ERROR("VolumeManager: {}", dataset.error);
            return;
        }
        if (sliceIndex >= dataset.fullResident->dimensions.z) {
            dataset.state = StreamState::Error;
            dataset.error = fmt::format("Dataset '{}' slice index {} out of bounds (depth={})",
                                        dataset.manifest.datasetId,
                                        sliceIndex,
                                        dataset.fullResident->dimensions.z);
            ctx_->uploader().recycleStaging(std::move(stagingSlot));
            CO_CORE_ERROR("VolumeManager: {}", dataset.error);
            return;
        }

        std::vector<Gpu::BufferTextureCopyRegion> regions;
        regions.push_back(Gpu::BufferTextureCopyRegion{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferTextureHeight = 0,
            .textureSubResource =
                {
                    .aspectMask = Gpu::TextureAspectFlagBits::ColorBit,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            .textureOffset =
                Gpu::Offset3D{
                    .x = 0,
                    .y = 0,
                    .z = static_cast<int32_t>(sliceIndex),
                },
            .textureExtent =
                Gpu::Extent3D{
                    .width = dataset.fullResident->dimensions.x,
                    .height = dataset.fullResident->dimensions.y,
                    .depth = 1,
                },
        });

        auto ticket = ctx_->uploader().enqueueStagedImageUpload(
            Cory::AsyncUploader::ImageUploadRequest{
                .destinationTexture = dataset.fullResident->texture.handle(),
                .byteSize = stagingSlot.byteSize,
                .regions = std::move(regions),
                .oldLayout = dataset.firstSliceSubmitted ? Gpu::TextureLayout::ShaderReadOnlyOptimal
                                                         : Gpu::TextureLayout::Undefined,
                .finalLayout = Gpu::TextureLayout::ShaderReadOnlyOptimal,
                .finalStages = Gpu::PipelineStageFlagBit::ComputeShaderBit,
                .finalMask = Gpu::AccessFlagBit::ShaderReadBit,
            },
            std::move(stagingSlot));
        dataset.inFlightSliceUploads.push_back(std::move(ticket));
        dataset.firstSliceSubmitted = true;
        dataset.state = StreamState::FullUploading;
    }
}

void VolumeManagerSystem::retireOldVolumes(uint64_t frameNumber)
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

std::string VolumeManagerSystem::stateToString(StreamState state)
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

void VolumeManagerSystem::ensureDatasetRegistered(const StreamedVolume &streamedVolume,
                                                  double currentTime)
{
    if (streamedVolume.datasetId.empty() || datasets_.contains(streamedVolume.datasetId)) {
        return;
    }

    VolumeManifest manifest{};
    std::string error{};
    if (!loadVolumeManifest(streamedVolume.manifestPath, manifest, error)) {
        datasets_.emplace(streamedVolume.datasetId,
                          DatasetRuntime{
                              .state = StreamState::Error,
                              .error = error,
                          });
        CO_CORE_ERROR("VolumeManager: {}", error);
        return;
    }

    if (!manifest.datasetId.empty() && manifest.datasetId != streamedVolume.datasetId) {
        CO_CORE_WARN("VolumeManager: dataset id mismatch '{}' != '{}' for '{}'",
                     streamedVolume.datasetId,
                     manifest.datasetId,
                     streamedVolume.manifestPath.string());
    }
    manifest.datasetId = streamedVolume.datasetId;
    auto [it, inserted] = datasets_.emplace(
        streamedVolume.datasetId,
        DatasetRuntime{
            .manifest = std::move(manifest),
            .sliceSubsampleFactor = std::max<size_t>(1u, streamedVolume.sliceSubsampleFactor),
            .loadingStartedTimeSeconds = currentTime,
        });
    if (!inserted) {
        return;
    }

    if (!it->second.manifest.bmpStack.has_value()) {
        it->second.state = StreamState::Error;
        it->second.error =
            fmt::format("Dataset '{}' manifest '{}' is missing required bmp_stack configuration",
                        it->second.manifest.datasetId,
                        streamedVolume.manifestPath.string());
        CO_CORE_ERROR("VolumeManager: {}", it->second.error);
        return;
    }

    enqueueRead(it->second, VolumeLevel::Full);
}

void VolumeManagerSystem::updateStreamedEntities(Cory::SceneGraph &graph, double currentTime)
{
    for (auto entity : graph.depthFirstTraversal()) {
        auto *streamed = graph.getComponent<StreamedVolume>(entity);
        if (streamed == nullptr) {
            continue;
        }
        ensureDatasetRegistered(*streamed, currentTime);

        auto *volume = graph.getComponent<VolumeComponent>(entity);
        if (volume == nullptr) {
            volume = &graph.addComponent<VolumeComponent>(entity, VolumeComponent{});
        }
        volume->datasetId = streamed->datasetId;

        const auto it = datasets_.find(streamed->datasetId);
        if (it == datasets_.end()) {
            volume->hasTexture = false;
            continue;
        }
        const auto &dataset = it->second;
        const bool hasUploadedData = dataset.uploadedSlices > 0u;

        const auto *resident =
            (dataset.fullResident && hasUploadedData)
                ? &(*dataset.fullResident)
                : (dataset.previewResident ? &(*dataset.previewResident) : nullptr);
        if (resident != nullptr) {
            volume->textureView = resident->view.handle();
            volume->textureDimensions = resident->dimensions;
            volume->hasTexture = true;
            volume->fullQuality = dataset.state == StreamState::FullReady;
        }
        else {
            volume->hasTexture = false;
        }
    }
}

void VolumeManagerSystem::enqueueProceduralGeneration(Cory::Entity entity,
                                                      ProceduralVolume procedural,
                                                      uint64_t frameNumber,
                                                      float timeSeconds)
{
    CO_CORE_ASSERT(ctx_ != nullptr, "VolumeManager context is null");
    auto &ctx = *ctx_;
    auto &runtime = proceduralVolumes_[entity];

    const auto needsResize =
        !runtime.resident.has_value() || runtime.generatedDimensions != procedural.dimensions;
    if (needsResize && runtime.resident.has_value()) {
        retiredVolumes_.push_back(
            RetiredVolume{.volume = std::move(*runtime.resident), .retireFrame = frameNumber});
        runtime.resident.reset();
    }

    if (!runtime.resident.has_value()) {
        auto textureLabel =
            fmt::format("VolumeManager procedural {}", static_cast<uint32_t>(entity));
        auto texture = createTexture3D(textureLabel,
                                       procedural.dimensions,
                                       ctx,
                                       Gpu::TextureUsageFlagBits::SampledBit |
                                           Gpu::TextureUsageFlagBits::StorageBit);
        auto view = createTexture3DView(textureLabel + " view", texture);
        runtime.resident = ResidentVolume{
            .texture = std::move(texture),
            .view = std::move(view),
            .dimensions = procedural.dimensions,
        };
    }

    const auto &shader = ctx.shaders()[createVolumeShader_];
    if (!shader.valid()) {
        CO_CORE_ERROR("Invalid volume generation shader in VolumeManagerSystem: {}",
                      shader.error());
        return;
    }

    auto recorder = ctx.device().createCommandRecorder(
        Gpu::CommandRecorderOptions{.queue = ctx.computeQueue().handle()});
    const auto imageRange = colorSubresourceRange();
    recorder.textureMemoryBarrier(Gpu::TextureMemoryBarrierOptions{
        .srcStages = Gpu::PipelineStageFlagBit::TopOfPipeBit,
        .srcMask = Gpu::AccessFlagBit::None,
        .dstStages = Gpu::PipelineStageFlagBit::ComputeShaderBit,
        .dstMask = Gpu::AccessFlagBit::ShaderWriteBit,
        .oldLayout =
            needsResize ? Gpu::TextureLayout::Undefined : Gpu::TextureLayout::ShaderReadOnlyOptimal,
        .newLayout = Gpu::TextureLayout::General,
        .texture = runtime.resident->texture.handle(),
        .range = imageRange,
    });

    auto pass = recorder.beginComputePass(Gpu::ComputePassCommandRecorderOptions{});
    const auto pipelineLayout = ctx.pipelineCache().queryLayout(Gpu::PipelineLayoutOptions{
        .label = "Pipeline Layout VolumeManager procedural generation",
        .bindGroupLayouts = ctx.descriptors().layouts(),
        .pushConstantRanges = {Cory::Shader::globalPushConstantRange},
    });
    pass.setPipelineLayout(pipelineLayout);

    const auto frameIndex = static_cast<uint32_t>(frameNumber % Cory::MAX_FRAMES_IN_FLIGHT);
    Cory::ShaderBindingContext bindingContext{ctx.device(),
                                              ctx.framegraphResources(),
                                              ctx.descriptors(),
                                              frameIndex,
                                              kManagerDrawDataBufferSize};
    {
        auto bindingScope = bindingContext.scoped(pass);
        pass.bindShader(shader.shaderHandle());
        bindingContext.bindStorageImage3D(runtime.resident->view.handle(),
                                          Gpu::TextureLayout::General);

        auto params = bindingContext.alloc<VolumeGenerationParams>();
        *params.cpu = VolumeGenerationParams{
            .volumeSpacing = glm::vec3{1.0f},
            .densityScale = procedural.densityScale,
            .volumeDimensions = procedural.dimensions,
            .time = timeSeconds * std::max(procedural.animationSpeed, 0.0f),
        };
        bindingContext.push(params.gpu);

        constexpr uint32_t kGroupSizeX = 16u;
        constexpr uint32_t kGroupSizeY = 16u;
        const uint32_t groupsX = (procedural.dimensions.x + kGroupSizeX - 1u) / kGroupSizeX;
        const uint32_t groupsY = (procedural.dimensions.y + kGroupSizeY - 1u) / kGroupSizeY;
        const uint32_t groupsZ = procedural.dimensions.z;
        pass.dispatchCompute({groupsX, groupsY, groupsZ});
    }
    pass.end();

    recorder.textureMemoryBarrier(Gpu::TextureMemoryBarrierOptions{
        .srcStages = Gpu::PipelineStageFlagBit::ComputeShaderBit,
        .srcMask = Gpu::AccessFlagBit::ShaderWriteBit,
        .dstStages = Gpu::PipelineStageFlagBit::ComputeShaderBit,
        .dstMask = Gpu::AccessFlagBit::ShaderReadBit,
        .oldLayout = Gpu::TextureLayout::General,
        .newLayout = Gpu::TextureLayout::ShaderReadOnlyOptimal,
        .texture = runtime.resident->texture.handle(),
        .range = imageRange,
    });

    auto commands = recorder.finish();
    auto completionFence = ctx.device().createFence(Gpu::FenceOptions{
        .label = "VolumeManager procedural generation fence",
        .createSignalled = false,
    });
    ctx.computeQueue().submit(Gpu::SubmitOptions{
        .commandBuffers = {commands.handle()},
        .signalFence = completionFence.handle(),
    });
    completionFence.wait();

    runtime.generatedDimensions = procedural.dimensions;
    runtime.generatedDensityScale = procedural.densityScale;
}

void VolumeManagerSystem::updateProceduralEntities(Cory::SceneGraph &graph,
                                                   uint64_t frameNumber,
                                                   double timeSeconds)
{
    for (auto entity : graph.depthFirstTraversal()) {
        auto *procedural = graph.getComponent<ProceduralVolume>(entity);
        if (procedural == nullptr) {
            continue;
        }
        auto &runtime = proceduralVolumes_[entity];
        const auto parametersChanged =
            runtime.generatedDimensions != procedural->dimensions ||
            std::abs(runtime.generatedDensityScale - procedural->densityScale) > 1e-5f;
        const auto needsRebuild = procedural->regenerate || !runtime.resident.has_value() ||
                                  procedural->updateEveryFrame || parametersChanged;
        if (needsRebuild) {
            enqueueProceduralGeneration(
                entity, *procedural, frameNumber, gsl::narrow_cast<float>(timeSeconds));
            procedural->regenerate = false;
        }

        auto *volume = graph.getComponent<VolumeComponent>(entity);
        if (volume == nullptr) {
            volume = &graph.addComponent<VolumeComponent>(entity, VolumeComponent{});
        }

        if (runtime.resident.has_value()) {
            volume->datasetId.clear();
            volume->textureView = runtime.resident->view.handle();
            volume->textureDimensions = runtime.resident->dimensions;
            volume->hasTexture = true;
            volume->fullQuality = true;
        }
        else {
            volume->hasTexture = false;
        }
    }
}
