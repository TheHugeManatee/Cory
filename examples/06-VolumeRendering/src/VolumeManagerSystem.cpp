#include "VolumeManagerSystem.hpp"

#include <Cory/Base/Log.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Framegraph/ShaderBindingContext.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/PipelineCache.hpp>
#include <Cory/Renderer/Shader.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include <KDGpu/command_recorder.h>
#include <KDGpu/compute_pass_command_recorder.h>
#include <KDGpu/texture.h>
#include <KDGpu/texture_options.h>
#include <KDGpu/texture_view_options.h>

#include <fmt/format.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <utility>

namespace {

[[nodiscard]] Gpu::Texture
createTexture3D(const std::string &label,
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

struct alignas(16) VolumeGenerationParams {
    glm::vec3 volumeSpacing{1.0f};
    float densityScale{1.0f};
    glm::uvec3 volumeDimensions{64u, 64u, 64u};
    float time{0.0f};
};
static_assert(std::is_trivially_copyable_v<VolumeGenerationParams>);

constexpr size_t kManagerDrawDataBufferSize = 64u * 1024u;

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
    , worker_{[this]() { workerLoop(); }}
{
    shaderHotReloader_.initialize(ctx);
    shaderHotReloader_.addShader({
        .path = Cory::ResourceLocator::Locate("create_volume.comp.slang"),
        .stage = Gpu::ShaderStageFlagBits::ComputeBit,
        .label = "create_volume.comp.slang",
        .shaderHandle = &createVolumeShader_,
    });
}

VolumeManagerSystem::~VolumeManagerSystem()
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

void VolumeManagerSystem::tick(Cory::SceneGraph &graph, Cory::TickInfo tickInfo)
{
    if (ctx_ == nullptr) {
        return;
    }

    shaderHotReloader_.processPendingReloads(tickInfo.ticks);
    ctx_->uploader().poll();
    processReadResults();
    processUploadCompletion(tickInfo.ticks);
    updateStreamedEntities(graph);
    const auto timeSeconds = static_cast<float>(tickInfo.now.time_since_epoch().count());
    updateProceduralEntities(graph, tickInfo.ticks, timeSeconds);
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

void VolumeManagerSystem::workerLoop()
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
        } else if (!result.bytes.empty() && request.expectedByteSize != 0 &&
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
    }
}

void VolumeManagerSystem::enqueueRead(DatasetRuntime &dataset, VolumeLevel level)
{
    const auto isPreview = level == VolumeLevel::Preview;
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
        } else {
            dataset.fullQueued = true;
            dataset.state = StreamState::PendingFullRead;
        }
    }
    requestCv_.notify_one();
}

void VolumeManagerSystem::processReadResults()
{
    std::deque<ReadResult> results;
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
        uploadBytesToVolume(dataset, result);
    }
}

void VolumeManagerSystem::uploadBytesToVolume(DatasetRuntime &dataset, const ReadResult &result)
{
    CO_CORE_ASSERT(ctx_ != nullptr, "VolumeManager context is null");
    const auto isPreview = result.level == VolumeLevel::Preview;
    const auto levelName = isPreview ? "preview" : "full";

    const auto textureLabel =
        fmt::format("VolumeManager {} texture ({})", dataset.manifest.datasetId, levelName);
    auto texture = createTexture3D(textureLabel,
                                   result.dimensions,
                                   *ctx_,
                                   Gpu::TextureUsageFlagBits::SampledBit |
                                       Gpu::TextureUsageFlagBits::TransferDstBit);
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
    } else {
        dataset.fullUpload = std::move(upload);
        dataset.state = StreamState::FullUploading;
    }
}

void VolumeManagerSystem::processUploadCompletion(uint64_t frameNumber)
{
    for (auto &[datasetId, dataset] : datasets_) {
        if (dataset.state == StreamState::Error) {
            continue;
        }

        if (dataset.previewUpload.has_value() && dataset.previewUpload->ticket.ready()) {
            dataset.previewResident = std::move(dataset.previewUpload->volume);
            dataset.previewUpload.reset();
            dataset.state = StreamState::PreviewReady;
            CO_CORE_INFO("VolumeManager: preview ready for '{}'", datasetId);

            if (!dataset.fullQueued) {
                enqueueRead(dataset, VolumeLevel::Full);
            }
        }

        if (dataset.fullUpload.has_value() && dataset.fullUpload->ticket.ready()) {
            dataset.fullResident = std::move(dataset.fullUpload->volume);
            dataset.fullUpload.reset();
            dataset.state = StreamState::FullReady;
            CO_CORE_INFO("VolumeManager: full volume ready for '{}'", datasetId);

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

void VolumeManagerSystem::retireOldVolumes(uint64_t frameNumber)
{
    auto it = retiredVolumes_.begin();
    while (it != retiredVolumes_.end()) {
        if (frameNumber >= it->retireFrame &&
            frameNumber - it->retireFrame >= Cory::MAX_FRAMES_IN_FLIGHT) {
            it = retiredVolumes_.erase(it);
        } else {
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

void VolumeManagerSystem::ensureDatasetRegistered(const StreamedVolume &streamedVolume)
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
    auto [it, inserted] =
        datasets_.emplace(streamedVolume.datasetId, DatasetRuntime{.manifest = std::move(manifest)});
    if (!inserted) {
        return;
    }

    enqueueRead(it->second, VolumeLevel::Preview);
}

void VolumeManagerSystem::updateStreamedEntities(Cory::SceneGraph &graph)
{
    for (auto entity : graph.depthFirstTraversal()) {
        auto *streamed = graph.getComponent<StreamedVolume>(entity);
        if (streamed == nullptr) {
            continue;
        }
        ensureDatasetRegistered(*streamed);

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

        const auto *resident = dataset.fullResident ? &(*dataset.fullResident)
                                                    : (dataset.previewResident ? &(*dataset.previewResident)
                                                                               : nullptr);
        if (resident != nullptr) {
            volume->textureView = resident->view.handle();
            volume->textureDimensions = resident->dimensions;
            volume->hasTexture = true;
            volume->fullQuality = dataset.fullResident.has_value();
        } else {
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

    const auto needsResize = !runtime.resident.has_value() ||
                             runtime.generatedDimensions != procedural.dimensions;
    if (needsResize && runtime.resident.has_value()) {
        retiredVolumes_.push_back(
            RetiredVolume{.volume = std::move(*runtime.resident), .retireFrame = frameNumber});
        runtime.resident.reset();
    }

    if (!runtime.resident.has_value()) {
        auto textureLabel = fmt::format("VolumeManager procedural {}", static_cast<uint32_t>(entity));
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
        CO_CORE_ERROR("Invalid volume generation shader in VolumeManagerSystem: {}", shader.error());
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
        .oldLayout = needsResize ? Gpu::TextureLayout::Undefined : Gpu::TextureLayout::ShaderReadOnlyOptimal,
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
    Cory::ShaderBindingContext bindingContext{
        ctx.device(), ctx.framegraphResources(), ctx.descriptors(), frameIndex, kManagerDrawDataBufferSize};
    {
        auto bindingScope = bindingContext.scoped(pass);
        pass.bindShader(shader.shaderHandle());
        bindingContext.bindStorageImage3D(runtime.resident->view.handle(), Gpu::TextureLayout::General);

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
    auto completionFence = ctx.device().createFence(
        Gpu::FenceOptions{
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
                                                   float timeSeconds)
{
    for (auto entity : graph.depthFirstTraversal()) {
        auto *procedural = graph.getComponent<ProceduralVolume>(entity);
        if (procedural == nullptr) {
            continue;
        }
        auto &runtime = proceduralVolumes_[entity];
        const auto parametersChanged = runtime.generatedDimensions != procedural->dimensions ||
                                       std::abs(runtime.generatedDensityScale -
                                                procedural->densityScale) > 1e-5f;
        const auto needsRebuild =
            procedural->regenerate || !runtime.resident.has_value() || procedural->updateEveryFrame ||
            parametersChanged;
        if (needsRebuild) {
            enqueueProceduralGeneration(entity, *procedural, frameNumber, timeSeconds);
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
        } else {
            volume->hasTexture = false;
        }
    }
}
