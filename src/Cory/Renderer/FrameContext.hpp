#pragma once

#include <Cory/Renderer/Gpu.hpp>

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
    const Texture *swapchainImage{};
    TextureView *swapchainImageView{};
    Texture *colorImage{};
    TextureView *colorImageView{};
    Texture *depthImage{};
    TextureView *depthImageView{};

    /// Fence to synchronize when the GPU has finished executing the commands associated with this
    /// frame, and its resources can be safely reused.
    Fence *inFlight{};
    /// Semaphore will be signaled when the swapchain image has been acquired (i.e.
    /// when presentation engine has finished with a preceding "present" call
    GpuSemaphore *acquired{};
    /// Semaphore will be signaled when all rendering commands have been executed on the GPU
    GpuSemaphore *rendered{};

    CommandRecorder commandBuffer;
    ResourceDeleter *resourceDeleter; /// Deleter to be enqueue resource destruction
                                      /// when the frame has finished.
};

} // namespace Cory