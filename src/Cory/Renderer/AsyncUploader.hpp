#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Base/Result.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/Gpu.hpp>
#include <Cory/Renderer/StagingUploader.hpp>

#include <KDGpu/buffer.h>
#include <KDGpu/command_recorder.h>

#include <cppcoro/coroutine.hpp>
#include <cppcoro/task.hpp>

#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <vector>

namespace Cory {

class Context;
class ThreadScheduler;

/**
 * Async bulk upload service that is intentionally orthogonal to framegraph execution.
 *
 * Key properties:
 * - Prefers transfer queue submissions with graphics fallback.
 * - Supports queue-family handoff from upload queue to a consumer queue family.
 * - Tracks upload completion via UploadTicket and reuses staging buffers internally.
 *
 * Scope note:
 * - This service does not integrate upload dependencies into framegraph scheduling.
 * - Callers are responsible for sequencing GPU usage against ticket completion.
 */
class AsyncUploader : NoCopy, public IStagingUploader {
  public:
    /// Sentinel value used to request "default consumer family" (graphics queue family).
    static constexpr uint32_t DefaultQueueFamily = std::numeric_limits<uint32_t>::max();

    /**
     * Buffer upload request.
     *
     * Requirements:
     * - destinationBuffer must be a valid GPU buffer handle.
     * - caller-provided staging slot must contain at least byteSize bytes.
     * - dstStages/dstMask describe the first intended consumer access after upload completion.
     *
     * Queue-family handoff:
     * - consumerQueueFamilyIndex defaults to graphics family.
     * - if consumer family differs from upload family, release/acquire barriers are emitted.
     */
    struct BufferUploadRequest {
        Gpu::Handle<Gpu::Buffer_t> destinationBuffer;
        Gpu::DeviceSize byteSize{0};
        Gpu::DeviceSize dstOffset{0};
        Gpu::PipelineStageFlags dstStages;
        Gpu::AccessFlags dstMask;
        uint32_t consumerQueueFamilyIndex{DefaultQueueFamily};
    };

    /**
     * Image upload request.
     *
     * Layout model:
     * - oldLayout -> TransferDstOptimal -> finalLayout.
     * - finalStages/finalMask describe the first intended consumer access after completion.
     *
     * Copy region model:
     * - regions describe staged buffer-to-texture copy regions.
     * - caller-provided staging slot must contain at least byteSize bytes.
     * - range is optional; if not set (aspectMask == None), it is inferred from regions.
     *
     * Queue-family handoff:
     * - consumerQueueFamilyIndex defaults to graphics family.
     * - if consumer family differs from upload family, release/acquire barriers are emitted.
     */
    struct ImageUploadRequest {
        Gpu::Handle<Gpu::Texture_t> destinationTexture;
        Gpu::DeviceSize byteSize{0};
        std::vector<Gpu::BufferTextureCopyRegion> regions;
        Gpu::TextureSubresourceRange range{};
        Gpu::TextureLayout oldLayout{Gpu::TextureLayout::Undefined};
        Gpu::TextureLayout finalLayout{Gpu::TextureLayout::ShaderReadOnlyOptimal};
        Gpu::PipelineStageFlags finalStages{Gpu::PipelineStageFlagBit::AllGraphicsBit};
        Gpu::AccessFlags finalMask{Gpu::AccessFlagBit::ShaderReadBit};
        uint32_t consumerQueueFamilyIndex{DefaultQueueFamily};
    };

    /**
     * Handle for upload completion and staging-resource reclamation.
     *
     * Behavior:
     * - ready() polls fence completion and reclaims upload resources on success.
     * - wait() blocks until completion and then reclaims upload resources.
     * - co_await currently blocks in await_suspend() (no dedicated uploader thread yet).
     *
     * Lifetime:
     * - UploadTicket is lightweight and copyable by value semantics through shared state.
     * - Reclamation is idempotent; multiple calls are safe.
     */
    class UploadTicket {
      public:
        struct UploadRecord;
        struct SharedState;

        UploadTicket() = default;

        /// Non-blocking completion check. Returns true once upload is fully complete.
        bool ready();
        /// Blocking completion wait.
        void wait();

        struct Awaiter {
            UploadTicket &ticket;
            bool await_ready() { return ticket.ready(); }
            bool await_suspend(cppcoro::coroutine_handle<>)
            {
                ticket.wait();
                return false;
            }
            void await_resume() {}
        };

        [[nodiscard]] Awaiter operator co_await() noexcept { return Awaiter{*this}; }

      private:
        UploadTicket(std::weak_ptr<SharedState> owner, std::shared_ptr<UploadRecord> record);

        static bool tryFinalize(const std::weak_ptr<SharedState> &owner,
                                const std::shared_ptr<UploadRecord> &record,
                                bool blockUntilComplete);

        std::weak_ptr<SharedState> owner_;
        std::shared_ptr<UploadRecord> record_;

        friend class AsyncUploader;
    };

    explicit AsyncUploader(Context &ctx, ThreadScheduler *threadScheduler);
    ~AsyncUploader();

    // movable
    AsyncUploader(AsyncUploader &&) noexcept;
    AsyncUploader &operator=(AsyncUploader &&) noexcept;

    /// Enqueue one buffer upload from a pre-filled staging slot.
    UploadTicket enqueueStagedBufferUpload(const BufferUploadRequest &request,
                                           StagingSlot &&stagingSlot);
    /// Enqueue one image upload from a pre-filled staging slot.
    UploadTicket enqueueStagedImageUpload(const ImageUploadRequest &request,
                                          StagingSlot &&stagingSlot);

    /// Acquire/reuse a pre-mapped staging slot that can be written by caller code.
    [[nodiscard]] cppcoro::task<Result<StagingSlot>>
    acquireStaging(Gpu::DeviceSize byteSize) override;
    [[nodiscard]] ThreadScheduler *threadScheduler() const noexcept override;
    /// Return an unused staging slot to the uploader pool.
    void recycleStaging(StagingSlot &&stagingSlot) override;
    /// Check whether a staging slot is usable.
    [[nodiscard]] bool validStaging(const StagingSlot &stagingSlot) const override;

    /**
     * Poll in-flight uploads and reclaim resources for completed uploads.
     *
     * Typical usage:
     * - call periodically from the main/render thread to keep staging pool compact.
     * - required for fire-and-forget uploads where tickets are not explicitly consumed.
     */
    void poll();

    /// Helper: true when transfer and graphics queue families differ.
    [[nodiscard]] static bool hasDedicatedTransferQueue(uint32_t transferFamily,
                                                        uint32_t graphicsFamily) noexcept;
    /// Helper: true when queue-family ownership transfer is required.
    [[nodiscard]] static bool needsQueueFamilyTransfer(uint32_t srcQueueFamily,
                                                       uint32_t dstQueueFamily) noexcept;
    /// Helper: resolves DefaultQueueFamily to graphics queue family.
    [[nodiscard]] static uint32_t resolveConsumerQueueFamily(uint32_t requestedQueueFamily,
                                                             uint32_t graphicsQueueFamily) noexcept;

  private:
    void assertRenderThread(char const *methodName) const;

    UploadTicket enqueueBufferUploadWithRecord(const BufferUploadRequest &request,
                                               std::shared_ptr<UploadTicket::UploadRecord> record);
    UploadTicket enqueueImageUploadWithRecord(const ImageUploadRequest &request,
                                              std::shared_ptr<UploadTicket::UploadRecord> record);

    struct Private;
    std::unique_ptr<Private> data_;
};

} // namespace Cory
