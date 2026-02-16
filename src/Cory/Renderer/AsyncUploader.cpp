#include <Cory/Renderer/AsyncUploader.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Context.hpp>

#include <KDGpu/buffer_options.h>

#include <algorithm>
#include <atomic>
#include <limits>
#include <mutex>
#include <optional>
#include <utility>

namespace Cory {
namespace {

Gpu::TextureSubresourceRange
createRangeFromRegions(std::span<const Gpu::BufferTextureCopyRegion> regions)
{
    if (regions.empty()) {
        return Gpu::TextureSubresourceRange{.aspectMask = Gpu::TextureAspectFlagBits::ColorBit,
                                            .baseMipLevel = 0,
                                            .levelCount = 1,
                                            .baseArrayLayer = 0,
                                            .layerCount = 1};
    }

    auto maxMipLevel = uint32_t{0};
    auto maxLayer = uint32_t{0};
    auto aspectMask = regions.front().textureSubResource.aspectMask;

    for (const auto &region : regions) {
        maxMipLevel = std::max(maxMipLevel, region.textureSubResource.mipLevel);
        maxLayer = std::max(maxLayer,
                            region.textureSubResource.baseArrayLayer +
                                region.textureSubResource.layerCount - 1);
        aspectMask |= region.textureSubResource.aspectMask;
    }

    return Gpu::TextureSubresourceRange{.aspectMask = aspectMask,
                                        .baseMipLevel = 0,
                                        .levelCount = maxMipLevel + 1,
                                        .baseArrayLayer = 0,
                                        .layerCount = maxLayer + 1};
}

} // namespace

struct AsyncUploader::UploadTicket::UploadRecord {
    Gpu::Fence completionFence;
    Gpu::Buffer stagingBuffer;
    std::optional<Gpu::CommandBuffer> uploadCommands;
    std::optional<Gpu::CommandBuffer> acquireCommands;
    std::optional<Gpu::GpuSemaphore> queueHandoffSemaphore;
    Gpu::DeviceSize stagingByteSize{0};
    std::atomic<bool> reclaimed{false};
};

struct AsyncUploader::UploadTicket::SharedState {
    struct StagingBufferEntry {
        Gpu::Buffer buffer;
        Gpu::DeviceSize byteSize{0};
    };

    std::mutex mutex;
    std::vector<StagingBufferEntry> stagingPool;
    std::vector<std::shared_ptr<UploadRecord>> inFlight;
};

struct AsyncUploader::Private {
    Context *ctx{nullptr};
    std::shared_ptr<UploadTicket::SharedState> sharedState;
};

AsyncUploader::UploadTicket::UploadTicket(std::weak_ptr<SharedState> owner,
                                          std::shared_ptr<UploadRecord> record)
    : owner_(std::move(owner))
    , record_(std::move(record))
{
}

bool AsyncUploader::UploadTicket::ready()
{
    return tryFinalize(owner_, record_, false);
}

void AsyncUploader::UploadTicket::wait()
{
    (void)tryFinalize(owner_, record_, true);
}

bool AsyncUploader::UploadTicket::tryFinalize(const std::weak_ptr<SharedState> &owner,
                                              const std::shared_ptr<UploadRecord> &record,
                                              bool blockUntilComplete)
{
    if (!record) {
        return true;
    }

    if (record->reclaimed.load(std::memory_order_acquire)) {
        return true;
    }

    if (blockUntilComplete) {
        record->completionFence.wait();
    }
    else if (record->completionFence.status() != Gpu::FenceStatus::Signalled) {
        return false;
    }

    // Ensure reclamation runs once even if multiple tickets/polls race to finalize.
    if (record->reclaimed.exchange(true, std::memory_order_acq_rel)) {
        return true;
    }

    auto shared = owner.lock();
    if (!shared) {
        return true;
    }

    std::scoped_lock lock(shared->mutex);

    if (record->stagingBuffer.isValid()) {
        shared->stagingPool.push_back(UploadTicket::SharedState::StagingBufferEntry{
            .buffer = std::move(record->stagingBuffer), .byteSize = record->stagingByteSize});
    }

    auto &inFlight = shared->inFlight;
    inFlight.erase(std::remove_if(inFlight.begin(),
                                  inFlight.end(),
                                  [&](const auto &candidate) { return candidate == record; }),
                   inFlight.end());

    return true;
}

AsyncUploader::AsyncUploader(Context &ctx)
    : data_(std::make_unique<Private>())
{
    data_->ctx = &ctx;
    data_->sharedState = std::make_shared<UploadTicket::SharedState>();
}

AsyncUploader::~AsyncUploader() = default;
AsyncUploader::AsyncUploader(AsyncUploader &&) noexcept = default;
AsyncUploader &AsyncUploader::operator=(AsyncUploader &&) noexcept = default;

bool AsyncUploader::hasDedicatedTransferQueue(uint32_t transferFamily,
                                              uint32_t graphicsFamily) noexcept
{
    return transferFamily != graphicsFamily;
}

bool AsyncUploader::needsQueueFamilyTransfer(uint32_t srcQueueFamily,
                                             uint32_t dstQueueFamily) noexcept
{
    return srcQueueFamily != dstQueueFamily;
}

uint32_t AsyncUploader::resolveConsumerQueueFamily(uint32_t requestedQueueFamily,
                                                   uint32_t graphicsQueueFamily) noexcept
{
    return requestedQueueFamily == DefaultQueueFamily ? graphicsQueueFamily : requestedQueueFamily;
}

namespace {

Gpu::Queue &queueFromFamily(Context &ctx, uint32_t queueFamily)
{
    if (queueFamily == ctx.graphicsQueueFamilyIndex()) {
        return ctx.graphicsQueue();
    }
    if (queueFamily == ctx.computeQueueFamilyIndex()) {
        return ctx.computeQueue();
    }
    if (queueFamily == ctx.transferQueueFamilyIndex()) {
        return ctx.transferQueue();
    }

    CO_CORE_WARN("AsyncUploader: unknown queue family {} requested, falling back to graphics.",
                 queueFamily);
    return ctx.graphicsQueue();
}

Gpu::Queue &selectUploadQueue(Context &ctx)
{
    if (AsyncUploader::hasDedicatedTransferQueue(ctx.transferQueueFamilyIndex(),
                                                 ctx.graphicsQueueFamilyIndex())) {
        return ctx.transferQueue();
    }

    return ctx.graphicsQueue();
}

Gpu::Buffer acquireStagingBuffer(AsyncUploader::UploadTicket::SharedState &state,
                                 Context &ctx,
                                 Gpu::DeviceSize byteSize)
{
    // Best-fit reuse keeps long-lived staging memory from growing too aggressively.
    auto smallestFit = state.stagingPool.end();
    for (auto it = state.stagingPool.begin(); it != state.stagingPool.end(); ++it) {
        if (it->byteSize < byteSize) {
            continue;
        }
        if (smallestFit == state.stagingPool.end() || it->byteSize < smallestFit->byteSize) {
            smallestFit = it;
        }
    }

    if (smallestFit != state.stagingPool.end()) {
        auto buffer = std::move(smallestFit->buffer);
        state.stagingPool.erase(smallestFit);
        return buffer;
    }

    return ctx.device().createBuffer(
        Gpu::BufferOptions{.label = "AsyncUploadStagingBuffer",
                           .size = byteSize,
                           .usage = Gpu::BufferUsageFlagBits::TransferSrcBit,
                           .memoryUsage = Gpu::MemoryUsage::CpuOnly});
}

} // namespace

AsyncUploader::UploadTicket
AsyncUploader::enqueueStagedBufferUpload(const BufferUploadRequest &request,
                                         StagingSlot &&stagingSlot)
{
    CO_CORE_ASSERT(request.destinationBuffer.isValid(),
                   "AsyncUploader: destination buffer must be valid.");
    CO_CORE_ASSERT(stagingSlot.valid(), "AsyncUploader: staging slot must contain a valid buffer.");
    CO_CORE_ASSERT(request.byteSize <= stagingSlot.byteSize,
                   "AsyncUploader: request byte size exceeds staging slot capacity.");

    auto record = std::make_shared<UploadTicket::UploadRecord>();
    record->stagingBuffer = std::move(stagingSlot.buffer);
    record->stagingByteSize = stagingSlot.byteSize;
    return enqueueBufferUploadWithRecord(request, std::move(record));
}

AsyncUploader::UploadTicket
AsyncUploader::enqueueStagedImageUpload(const ImageUploadRequest &request,
                                        StagingSlot &&stagingSlot)
{
    CO_CORE_ASSERT(stagingSlot.valid(), "AsyncUploader: staging slot must contain a valid buffer.");
    CO_CORE_ASSERT(request.byteSize <= stagingSlot.byteSize,
                   "AsyncUploader: request byte size exceeds staging slot capacity.");

    std::shared_ptr<UploadTicket::UploadRecord> record =
        std::make_shared<UploadTicket::UploadRecord>();
    record->stagingBuffer = std::move(stagingSlot.buffer);
    record->stagingByteSize = stagingSlot.byteSize;
    return enqueueImageUploadWithRecord(request, std::move(record));
}

AsyncUploader::UploadTicket
AsyncUploader::enqueueBufferUploadWithRecord(const BufferUploadRequest &request,
                                             std::shared_ptr<UploadTicket::UploadRecord> record)
{
    auto &ctx = *data_->ctx;
    auto &uploadQueue = selectUploadQueue(ctx);
    const auto uploadQueueFamily = uploadQueue.queueTypeIndex();
    const auto consumerQueueFamily = resolveConsumerQueueFamily(request.consumerQueueFamilyIndex,
                                                                ctx.graphicsQueueFamilyIndex());
    auto &consumerQueue = queueFromFamily(ctx, consumerQueueFamily);

    const bool queueFamilyTransfer =
        needsQueueFamilyTransfer(uploadQueueFamily, consumerQueueFamily);

    auto uploadRecorder = ctx.device().createCommandRecorder(
        Gpu::CommandRecorderOptions{.queue = uploadQueue.handle()});
    uploadRecorder.copyBuffer(Gpu::BufferCopy{.src = record->stagingBuffer,
                                              .srcOffset = 0,
                                              .dst = request.destinationBuffer,
                                              .dstOffset = request.dstOffset,
                                              .byteSize = request.byteSize});

    if (queueFamilyTransfer) {
        uploadRecorder.bufferMemoryBarrier(Gpu::BufferMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::TransferBit,
            .srcMask = Gpu::AccessFlagBit::TransferWriteBit,
            .dstStages = Gpu::PipelineStageFlagBit::BottomOfPipeBit,
            .dstMask = Gpu::AccessFlagBit::None,
            .srcQueueTypeIndex = uploadQueueFamily,
            .dstQueueTypeIndex = consumerQueueFamily,
            .buffer = request.destinationBuffer,
            .offset = request.dstOffset,
            .size = request.byteSize,
        });
    }
    else {
        uploadRecorder.bufferMemoryBarrier(Gpu::BufferMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::TransferBit,
            .srcMask = Gpu::AccessFlagBit::TransferWriteBit,
            .dstStages = request.dstStages,
            .dstMask = request.dstMask,
            .buffer = request.destinationBuffer,
            .offset = request.dstOffset,
            .size = request.byteSize,
        });
    }

    auto uploadCommands = uploadRecorder.finish();
    auto completionFence = ctx.device().createFence(
        Gpu::FenceOptions{.label = "AsyncBufferUploadFence", .createSignalled = false});

    if (queueFamilyTransfer) {
        auto queueHandoff = ctx.device().createGpuSemaphore(
            Gpu::GpuSemaphoreOptions{.label = "AsyncUploadQueueHandoff"});

        uploadQueue.submit(Gpu::SubmitOptions{.commandBuffers = {uploadCommands.handle()},
                                              .signalSemaphores = {queueHandoff.handle()}});

        auto acquireRecorder = ctx.device().createCommandRecorder(
            Gpu::CommandRecorderOptions{.queue = consumerQueue.handle()});
        acquireRecorder.bufferMemoryBarrier(Gpu::BufferMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::TopOfPipeBit,
            .srcMask = Gpu::AccessFlagBit::None,
            .dstStages = request.dstStages,
            .dstMask = request.dstMask,
            .srcQueueTypeIndex = uploadQueueFamily,
            .dstQueueTypeIndex = consumerQueueFamily,
            .buffer = request.destinationBuffer,
            .offset = request.dstOffset,
            .size = request.byteSize,
        });
        auto acquireCommands = acquireRecorder.finish();

        consumerQueue.submit(Gpu::SubmitOptions{.commandBuffers = {acquireCommands.handle()},
                                                .waitSemaphores = {queueHandoff.handle()},
                                                .signalFence = completionFence.handle()});

        record->queueHandoffSemaphore = std::move(queueHandoff);
        record->acquireCommands = std::move(acquireCommands);
    }
    else {
        uploadQueue.submit(Gpu::SubmitOptions{.commandBuffers = {uploadCommands.handle()},
                                              .signalFence = completionFence.handle()});
    }

    record->completionFence = std::move(completionFence);
    record->uploadCommands = std::move(uploadCommands);

    {
        std::scoped_lock lock(data_->sharedState->mutex);
        data_->sharedState->inFlight.push_back(record);
    }

    return UploadTicket{data_->sharedState, std::move(record)};
}

AsyncUploader::UploadTicket
AsyncUploader::enqueueImageUploadWithRecord(const ImageUploadRequest &request,
                                            std::shared_ptr<UploadTicket::UploadRecord> record)
{
    CO_CORE_ASSERT(request.destinationTexture.isValid(),
                   "AsyncUploader: destination texture must be valid.");
    CO_CORE_ASSERT(record != nullptr && record->stagingBuffer.isValid(),
                   "AsyncUploader: image upload requires a valid staging buffer.");

    auto &ctx = *data_->ctx;
    auto &uploadQueue = selectUploadQueue(ctx);
    const auto uploadQueueFamily = uploadQueue.queueTypeIndex();
    const auto consumerQueueFamily = resolveConsumerQueueFamily(request.consumerQueueFamilyIndex,
                                                                ctx.graphicsQueueFamilyIndex());
    auto &consumerQueue = queueFromFamily(ctx, consumerQueueFamily);

    const bool queueFamilyTransfer =
        needsQueueFamilyTransfer(uploadQueueFamily, consumerQueueFamily);

    const auto range = request.range.aspectMask == Gpu::TextureAspectFlagBits::None
                           ? createRangeFromRegions(request.regions)
                           : request.range;

    auto uploadRecorder = ctx.device().createCommandRecorder(
        Gpu::CommandRecorderOptions{.queue = uploadQueue.handle()});

    uploadRecorder.textureMemoryBarrier(Gpu::TextureMemoryBarrierOptions{
        .srcStages = Gpu::PipelineStageFlagBit::TopOfPipeBit,
        .srcMask = Gpu::AccessFlagBit::None,
        .dstStages = Gpu::PipelineStageFlagBit::TransferBit,
        .dstMask = Gpu::AccessFlagBit::TransferWriteBit,
        .oldLayout = request.oldLayout,
        .newLayout = Gpu::TextureLayout::TransferDstOptimal,
        .texture = request.destinationTexture,
        .range = range,
    });

    uploadRecorder.copyBufferToTexture(Gpu::BufferToTextureCopy{
        .srcBuffer = record->stagingBuffer,
        .dstTexture = request.destinationTexture,
        .dstTextureLayout = Gpu::TextureLayout::TransferDstOptimal,
        .regions = request.regions,
    });

    if (queueFamilyTransfer) {
        // Release ownership on upload queue, transition into the caller-requested final layout.
        uploadRecorder.textureMemoryBarrier(Gpu::TextureMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::TransferBit,
            .srcMask = Gpu::AccessFlagBit::TransferWriteBit,
            .dstStages = Gpu::PipelineStageFlagBit::BottomOfPipeBit,
            .dstMask = Gpu::AccessFlagBit::None,
            .oldLayout = Gpu::TextureLayout::TransferDstOptimal,
            .newLayout = request.finalLayout,
            .srcQueueTypeIndex = uploadQueueFamily,
            .dstQueueTypeIndex = consumerQueueFamily,
            .texture = request.destinationTexture,
            .range = range,
        });
    }
    else {
        uploadRecorder.textureMemoryBarrier(Gpu::TextureMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::TransferBit,
            .srcMask = Gpu::AccessFlagBit::TransferWriteBit,
            .dstStages = request.finalStages,
            .dstMask = request.finalMask,
            .oldLayout = Gpu::TextureLayout::TransferDstOptimal,
            .newLayout = request.finalLayout,
            .texture = request.destinationTexture,
            .range = range,
        });
    }

    auto uploadCommands = uploadRecorder.finish();
    auto completionFence = ctx.device().createFence(
        Gpu::FenceOptions{.label = "AsyncImageUploadFence", .createSignalled = false});

    if (queueFamilyTransfer) {
        auto queueHandoff = ctx.device().createGpuSemaphore(
            Gpu::GpuSemaphoreOptions{.label = "AsyncUploadQueueHandoff"});

        uploadQueue.submit(Gpu::SubmitOptions{.commandBuffers = {uploadCommands.handle()},
                                              .signalSemaphores = {queueHandoff.handle()}});

        auto acquireRecorder = ctx.device().createCommandRecorder(
            Gpu::CommandRecorderOptions{.queue = consumerQueue.handle()});
        // Acquire ownership on the consumer queue while keeping layout unchanged.
        acquireRecorder.textureMemoryBarrier(Gpu::TextureMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::TopOfPipeBit,
            .srcMask = Gpu::AccessFlagBit::None,
            .dstStages = request.finalStages,
            .dstMask = request.finalMask,
            .oldLayout = request.finalLayout,
            .newLayout = request.finalLayout,
            .srcQueueTypeIndex = uploadQueueFamily,
            .dstQueueTypeIndex = consumerQueueFamily,
            .texture = request.destinationTexture,
            .range = range,
        });
        auto acquireCommands = acquireRecorder.finish();

        consumerQueue.submit(Gpu::SubmitOptions{.commandBuffers = {acquireCommands.handle()},
                                                .waitSemaphores = {queueHandoff.handle()},
                                                .signalFence = completionFence.handle()});

        record->queueHandoffSemaphore = std::move(queueHandoff);
        record->acquireCommands = std::move(acquireCommands);
    }
    else {
        uploadQueue.submit(Gpu::SubmitOptions{.commandBuffers = {uploadCommands.handle()},
                                              .signalFence = completionFence.handle()});
    }

    record->completionFence = std::move(completionFence);
    record->uploadCommands = std::move(uploadCommands);

    {
        std::scoped_lock lock(data_->sharedState->mutex);
        data_->sharedState->inFlight.push_back(record);
    }

    return UploadTicket{data_->sharedState, std::move(record)};
}

cppcoro::task<Result<StagingSlot>> AsyncUploader::acquireStaging(Gpu::DeviceSize byteSize)
{
    auto slot = StagingSlot{};
    {
        std::scoped_lock lock(data_->sharedState->mutex);
        slot.buffer = acquireStagingBuffer(*data_->sharedState, *data_->ctx, byteSize);
    }
    slot.byteSize = byteSize;
    co_return slot;
}

void AsyncUploader::recycleStaging(StagingSlot &&stagingSlot)
{
    if (!validStaging(stagingSlot)) {
        return;
    }
    CO_CORE_ASSERT(stagingSlot.userData == nullptr,
                   "AsyncUploader::recycleStaging received unexpected userData payload");

    std::scoped_lock lock(data_->sharedState->mutex);
    data_->sharedState->stagingPool.push_back(UploadTicket::SharedState::StagingBufferEntry{
        .buffer = std::move(stagingSlot.buffer),
        .byteSize = stagingSlot.byteSize,
    });
}

std::byte *AsyncUploader::mapStaging(StagingSlot &stagingSlot)
{
    CO_CORE_ASSERT(stagingSlot.buffer.isValid(),
                   "AsyncUploader::mapStaging requires a valid buffer");
    CO_CORE_ASSERT(stagingSlot.userData == nullptr,
                   "AsyncUploader::mapStaging received unexpected userData payload");
    return reinterpret_cast<std::byte *>(stagingSlot.buffer.map());
}

void AsyncUploader::unmapStaging(StagingSlot &stagingSlot)
{
    CO_CORE_ASSERT(stagingSlot.buffer.isValid(),
                   "AsyncUploader::unmapStaging requires a valid buffer");
    CO_CORE_ASSERT(stagingSlot.userData == nullptr,
                   "AsyncUploader::unmapStaging received unexpected userData payload");
    stagingSlot.buffer.unmap();
}

bool AsyncUploader::validStaging(const StagingSlot &stagingSlot) const
{
    return stagingSlot.buffer.isValid();
}

void AsyncUploader::poll()
{
    std::vector<std::shared_ptr<UploadTicket::UploadRecord>> records;
    {
        std::scoped_lock lock(data_->sharedState->mutex);
        records = data_->sharedState->inFlight;
    }

    for (const auto &record : records) {
        (void)UploadTicket::tryFinalize(data_->sharedState, record, false);
    }
}

} // namespace Cory
