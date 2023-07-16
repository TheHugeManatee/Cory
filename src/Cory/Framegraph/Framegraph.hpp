#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Framegraph/Common.hpp>
#include <Cory/Framegraph/RenderTaskBuilder.hpp>

#include <glm/vec3.hpp>

#include <cppcoro/generator.hpp>

#include <concepts>
#include <set>
#include <string_view>
#include <unordered_map>

namespace Cory {

struct ExecutionInfo {
    struct TransitionInfo {
        TaskDependencyKind kind;
        RenderTaskHandle task;
        TransientTextureHandle resource;
        Sync::AccessType stateBefore;
        Sync::AccessType stateAfter;
    };
    std::vector<RenderTaskHandle> tasks;
    std::vector<TextureHandle> resources;
    std::vector<TransitionInfo> transitions;
};

/**
 * The framegraph.
 *
 * Is meant to be filled with Cory::RenderTaskBuilder
 */
class Framegraph : NoCopy {
  public:
    explicit Framegraph(Context &ctx);
    ~Framegraph();

    Framegraph(Framegraph &&) noexcept;
    Framegraph &operator=(Framegraph &&) noexcept;

    /**
     * @brief record the commands from all render tasks into the given command buffer
     *
     * Note that this can be only called once. It will cause all relevant render tasks to execute.
     */
    ExecutionInfo record(FrameContext& frameCtx);

    /**
     * @brief immediately retire all resources allocated by the framegraph
     *
     * should be called only when it can be ensured that all resources are no longer in use, e.g.
     * for example when the next frame with the same swapchain image has been rendered.
     */
    void resetForNextFrame();

    /// declare a new render task
    RenderTaskBuilder declareTask(std::string_view name);

    /// declare an external texture as an input
    [[nodiscard]] TransientTextureHandle declareInput(TextureInfo info,
                                                      Sync::AccessType lastWriteAccess,
                                                      Magnum::Vk::Image &image,
                                                      Magnum::Vk::ImageView &imageView);

    /**
     * declare that a resource is to be read afterwards. returns general
     * information and synchronization state of the last write to the
     * texture so external code can synchronize with it
     */
    std::pair<TextureInfo, TextureState> declareOutput(TransientTextureHandle handle);

    [[nodiscard]] const TextureManager &resources() const;
    [[nodiscard]] const std::vector<TransientTextureHandle> &externalInputs() const;
    [[nodiscard]] const std::vector<TransientTextureHandle> &outputs() const;

    [[nodiscard]] std::string dump(const ExecutionInfo &info);

  private: /* member functions */
    RenderTaskHandle finishTaskDeclaration(RenderTaskInfo &&info);

    /// to be called from RenderTaskExecutionAwaiter - the Framegraph takes ownership of the @a
    /// coroHandle
    void enqueueRenderPass(RenderTaskHandle passHandle, cppcoro::coroutine_handle<> coroHandle);
    TextureManager &resources();

    /// to be called from RenderTaskBuilder
    RenderInput renderInput(RenderTaskHandle taskHandle);

    /**
     * @brief resolve which render tasks need to be executed for requested resources
     *
     * Returns the tasks that need to be executed in the given order, and all resources that
     * are required to execute said resources.
     * Updates the internal information about which render pass is required.
     */
    [[nodiscard]] ExecutionInfo
    resolve(const std::vector<TransientTextureHandle> &requestedResources);

    [[nodiscard]] ExecutionInfo compile();
    [[nodiscard]] std::vector<ExecutionInfo::TransitionInfo> executePass(CommandList &cmd,
                                                                         RenderTaskHandle handle);

    [[nodiscard]] cppcoro::generator<std::pair<RenderTaskHandle, const RenderTaskInfo &>>
    renderTasks() const;

  private:                             /* members */
    friend RenderTaskBuilder;                    // convenience so it can call finishTaskDeclaration
    friend RenderTaskExecutionAwaiter; // so it can call enqueueRenderPass
    friend FramegraphVisualizer;       // accesses all the internals

    std::unique_ptr<struct FramegraphPrivate> data_;
};

} // namespace Cory
