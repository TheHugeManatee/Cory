#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Framegraph/Common.hpp>
#include <Cory/Renderer/Gpu.hpp>

#include <cppcoro/generator.hpp>

#include <filesystem>
#include <string_view>

namespace Cory {

struct ExecutionInfo {
    struct TransitionInfo {
        TaskDependencyKind kind;
        RenderTaskHandle task;
        TransientTextureHandle resource;
        Sync::AccessType stateBefore;
        Sync::AccessType stateAfter;
    };
    struct BufferTransitionInfo {
        TaskDependencyKind kind;
        RenderTaskHandle task;
        TransientBufferHandle resource;
        Sync::AccessType stateBefore;
        Sync::AccessType stateAfter;
    };
    std::vector<RenderTaskHandle> tasks;
    std::vector<FramegraphTextureHandle> resources;
    std::vector<FramegraphBufferHandle> buffers;
    std::vector<TransitionInfo> transitions;
    std::vector<BufferTransitionInfo> bufferTransitions;
};

/**
 * The framegraph.
 *
 * Is meant to be filled with Cory::RenderTaskBuilder
 */
class Framegraph : NoCopy {
  public:
    // Create a new framegraph with the given context, using the given instance index for resource
    // allocation
    explicit Framegraph(Context &ctx, FramegraphResourceManager &resources, uint32_t instanceIndex);
    ~Framegraph();

    Framegraph(Framegraph &&) noexcept;
    Framegraph &operator=(Framegraph &&) noexcept;

    /**
     * @brief record the commands from all render tasks into the given command buffer
     *
     * Note that this can be only called once. It will cause all relevant render tasks to execute.
     */
    ExecutionInfo record(FrameContext &frameCtx);

    /**
     * @brief immediately retire all resources allocated by the framegraph
     *
     * should be called only when it can be ensured that all resources are no longer in use, e.g.
     * for example when the next frame with the same swapchain image has been rendered.
     */
    void resetForNextFrame(uint64_t frameNumber);

    /// declare a new render task
    RenderTaskBuilder declareTask(std::string_view name);

    struct FrameContextHandles {
        TransientTextureHandle colorImage;
        TransientTextureHandle depthImage;
        TransientTextureHandle swapchainImage;
    };
    /// Import the external frame context (e.g. from a swapchain) as input resources
    [[nodiscard]] FrameContextHandles importFrameContext(const FrameContext &frameCtx);

    /// declare an external texture as an input
    [[nodiscard]] TransientTextureHandle declareInput(TextureInfo info,
                                                      Sync::AccessType lastWriteAccess,
                                                      const Texture &image,
                                                      const TextureView &imageView);
    [[nodiscard]] TransientTextureHandle declareInput(TextureInfo info,
                                                      Sync::AccessType lastWriteAccess,
                                                      Gpu::TextureHandle image,
                                                      Gpu::TextureViewHandle imageView);
    [[nodiscard]] TransientTextureHandle declareInput(TransientTextureHandle handle);

    /// @brief declare an external resource dependency for the framegraph
    /// @param finalAccess The desired final access type for the output resource
    /// @return Information and synchronization state after the last task using the texture
    ///
    /// Declares a texture as the output of the frame graph, intended for further use externally
    /// (e.g. present to a swap chain).
    /// The framegraph will only execute tasks that contribute to requested outputs, and skip over
    /// any tasks that are not required to produce said outputs.
    ///
    /// The framegraph will explicitly transition the texture to the requested final access type
    /// after all tasks have executed.
    std::pair<TextureInfo, TextureState>
    declareOutput(TransientTextureHandle handle,
                  Sync::AccessType finalAccess = Sync::AccessType::Present);

    [[nodiscard]] const FramegraphResourceManager &resources() const;
    [[nodiscard]] const std::vector<TransientTextureHandle> &externalInputs() const;
    [[nodiscard]] const std::vector<TransientTextureHandle> &outputs() const;

    /// Mostly for observability
    [[nodiscard]] cppcoro::generator<std::pair<RenderTaskHandle, const RenderTaskInfo &>>
    renderTasks() const;

    void dump(const ExecutionInfo &info, std::filesystem::path outputPath) const;

  protected:
    RenderTaskHandle finishTaskDeclaration(RenderTaskInfo &&info);

    /// to be called from RenderTaskExecutionAwaiter - the Framegraph takes ownership of the @a
    /// coroHandle
    void enqueueRenderPass(RenderTaskHandle passHandle, cppcoro::coroutine_handle<> coroHandle);
    FramegraphResourceManager &resources();

    /// to be called from RenderTaskBuilder
    RenderInput renderInput(RenderTaskHandle taskHandle);

    /**
     * @brief Resolve the execution plan for requested output resources.
     *
     * This performs three steps:
     * 1) Builds a dependency view of tasks/resources (writers and pure-read inputs).
     * 2) Traverses dependencies from requested outputs to find required tasks/resources.
     * 3) Topologically sorts required tasks and aggregates resource usage flags.
     *
     * Returns the tasks in execution order plus the resources/buffers they require.
     */
    [[nodiscard]] ExecutionInfo
    resolve(const std::vector<TransientTextureHandle> &requestedResources);

    [[nodiscard]] ExecutionInfo compile();
    struct PassTransitions {
        std::vector<ExecutionInfo::TransitionInfo> imageTransitions;
        std::vector<ExecutionInfo::BufferTransitionInfo> bufferTransitions;
    };
    [[nodiscard]] PassTransitions executePass(CommandRecorder &cmd, RenderTaskHandle handle);

    /// Ensure that all output resources are transitioned to their final access states
    void finalizeOutputs(ExecutionInfo &executionInfo);

  private:                    /* members */
    friend RenderTaskBuilder; // convenience so it can call finishTaskDeclaration
    template <typename>
    friend struct RenderTaskExecutionAwaiter; // so it can call enqueueRenderPass
    friend FramegraphVisualizer;              // accesses all the internals

    std::unique_ptr<struct FramegraphPrivate> data_;
};

} // namespace Cory
