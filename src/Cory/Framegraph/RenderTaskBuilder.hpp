#pragma once

#include <Cory/Framegraph/Common.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>
#include <Cory/Framegraph/TransientComputePass.hpp>
#include <Cory/Framegraph/TransientRenderPass.hpp>

#include <cppcoro/coroutine.hpp>

#include <string_view>
#include <utility>

namespace Cory {
/**
 * an Awaitable that will enqueue the current coroutine for execution on the given framegraph
 * when the render task gets scheduled.
 *
 * Note that the coroutine may never be resumed if the render pass identified by the @a passHandle
 * does not get scheduled.
 */
template <typename RenderTaskOutput> struct RenderTaskExecutionAwaiter {
    RenderTaskHandle passHandle;
    Framegraph &fg;
    RenderTaskOutput output;
    [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }
    [[nodiscard]] RenderInput await_resume() const noexcept;
    void await_suspend(
        cppcoro::coroutine_handle<typename RenderTaskDeclaration<RenderTaskOutput>::promise_type>
            coroHandle) noexcept;
};

/// struct summarizing all info collected about a render task
struct RenderTaskInfo {
    struct TextureDependency {
        TaskDependencyKind kind;
        TransientTextureHandle handle;
        Sync::AccessType access;
    };
    struct BufferDependency {
        TaskDependencyKind kind;
        TransientBufferHandle handle;
        Sync::AccessType access;
    };
    std::string name;
    std::vector<TextureDependency> textureDependencies;
    std::vector<BufferDependency> bufferDependencies;

    // framegraph internal stuff
    cppcoro::coroutine_handle<> coroHandle;
    int32_t executionPriority{-1}; ///< assigned when the render graph is resolved
};

/**
 * a builder that allows a render task to declare specific dependencies (inputs and outputs).
 *
 * For defaults, see default values in RenderTaskInfo.
 *
 * Meant to be used only locally, hence not copyable.
 */
class RenderTaskBuilder : NoCopy {
  public:
    RenderTaskBuilder(Context &ctx, Framegraph &framegraph, std::string_view taskName);
    ~RenderTaskBuilder();

    /// move-constructible because it is intended to be provided by-value to the pass coroutine
    RenderTaskBuilder(RenderTaskBuilder &&) = default;

    /// declare that a render pass creates a certain texture
    TransientTextureHandle
    create(std::string name, glm::u32vec3 size, Gpu::Format format, Sync::AccessType writeAccess);

    /// declare that a render pass creates a certain buffer
    TransientBufferHandle create(std::string name,
                                 Gpu::DeviceSize size,
                                 Gpu::BufferUsageFlags usage,
                                 Sync::AccessType writeAccess,
                                 Gpu::MemoryUsage memoryUsage = Gpu::MemoryUsage::GpuOnly);

    /// declares a dependency to the named resource
    TextureInfo read(TransientTextureHandle &h, Sync::AccessType readAccess);

    /// declares a dependency to the named buffer resource
    BufferInfo read(TransientBufferHandle &h, Sync::AccessType readAccess);

    /// declare that a render task writes to a certain texture
    std::pair<TransientTextureHandle, TextureInfo> write(TransientTextureHandle handle,
                                                         Sync::AccessType writeAccess);

    /// declare that a render task writes to a certain buffer
    std::pair<TransientBufferHandle, BufferInfo> write(TransientBufferHandle handle,
                                                       Sync::AccessType writeAccess);

    /// declare that a render task reads from and writes to a certain texture
    std::pair<TransientTextureHandle, TextureInfo> readWrite(TransientTextureHandle handle,
                                                             Sync::AccessType readWriteAccess);

    /// declare that a render task reads from and writes to a certain buffer
    std::pair<TransientBufferHandle, BufferInfo> readWrite(TransientBufferHandle handle,
                                                           Sync::AccessType readWriteAccess);

    /**
     * Declares a render pass and its attachments.
     * @param passDeclaration   the declaration of the pass
     * @return a builder class to set up the render pass. call finish() to obtain the pass object
     */
    TransientRenderPass declareRenderPass(RenderPassDeclaration passDeclaration);

    TransientComputePass declareComputePass(ComputePassDeclaration passDeclaration);

    /**
     * @brief Finish declaration of the render task and provide outputs.
     *
     * co_await'ing on the returned awaiter will suspend execution of the current coroutine
     * and enqueue it to the frame graph. Execution will resume on the framegraph's execution
     * context if the framegraph determines that this render pass will need to be resumed
     * at all (the render pass provides resources that another pass consumes). If the resources
     * of the render pass are not needed, the coroutine will never be resumed.
     */
    template <typename RenderTaskOutput>
    RenderTaskExecutionAwaiter<RenderTaskOutput> finishDeclaration(RenderTaskOutput output);

    /// the name of the render task that is being created
    const std::string &name() const { return info_.name; }

    /// Declare a subtask of the current render task.
    ///
    /// Subpasses are not treated any different from tasks, they just get the parent pass as a
    /// naming prefix and can be used to compose larger render tasks and benefit from the
    /// synchronization.
    [[nodiscard]] RenderTaskBuilder subtask(std::string_view name) const;

  private:
    Context &ctx_;
    RenderTaskInfo info_;
    Framegraph &framegraph_;
};
} // namespace Cory

#include <Cory/Framegraph/Framegraph.hpp>

namespace Cory {
template <typename RenderTaskOutput>
RenderInput RenderTaskExecutionAwaiter<RenderTaskOutput>::await_resume() const noexcept
{
    return fg.renderInput(passHandle);
}

template <typename RenderTaskOutput>
void RenderTaskExecutionAwaiter<RenderTaskOutput>::await_suspend(
    cppcoro::coroutine_handle<typename RenderTaskDeclaration<RenderTaskOutput>::promise_type>
        coroHandle) noexcept
{
    coroHandle.promise().set_output(std::move(output));
    fg.enqueueRenderPass(passHandle, coroHandle);
}

template <typename RenderTaskOutput>
RenderTaskExecutionAwaiter<RenderTaskOutput>
RenderTaskBuilder::finishDeclaration(RenderTaskOutput output)
{
    const RenderTaskHandle passHandle = framegraph_.finishTaskDeclaration(std::move(info_));
    return RenderTaskExecutionAwaiter<RenderTaskOutput>{
        passHandle,
        framegraph_,
        std::move(output),
    };
}

} // namespace Cory
