#pragma once

#include <Cory/Renderer/KDGpuFwd.hpp>

#include <KDGpu/command_recorder.h>

#include <glm/vec2.hpp>

#include <cstdint>

namespace Cory {

struct FrameContext {
    uint32_t inFlightIndex{};       ///< the current swapchain image index
    uint32_t swapchainImageIndex{}; ///< the current swapchain image index
    uint64_t frameNumber{};         ///< the (monotonically increasing) frame number
    glm::u32vec2 extent{};          ///< the size of the swapchain image in pixels

    // The resources for the current frame
    const KDGpu::Texture *swapchainImage{};
    KDGpu::TextureView *swapchainImageView{};
    KDGpu::Texture *colorImage{};
    KDGpu::TextureView *colorImageView{};
    KDGpu::Texture *depthImage{};
    KDGpu::TextureView *depthImageView{};

    /// Fence to synchronize when the GPU has finished executing the commands associated with this
    /// frame, and its resources can be safely reused.
    KDGpu::Fence *inFlight{};
    /// Semaphore will be signaled when the swapchain image has been acquired (i.e.
    /// when presentation engine has finished with a preceding "present" call
    KDGpu::GpuSemaphore *acquired{};
    /// Semaphore will be signaled when all rendering commands have been executed on the GPU
    KDGpu::GpuSemaphore *rendered{};

    KDGpu::CommandRecorder commandBuffer;
    KDGpuUtils::ResourceDeleter *resourceDeleter; /// Deleter to be enqueue resource destruction
                                                  /// when the frame has finished.
};

} // namespace Cory