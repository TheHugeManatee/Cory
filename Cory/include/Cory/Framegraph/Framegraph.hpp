#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Framegraph/Builder.hpp>
#include <Cory/Framegraph/Common.hpp>
#include <Cory/Framegraph/TextureManager.hpp>

#include <cppcoro/shared_task.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all_ready.hpp>
#include <glm/vec3.hpp>

#include <concepts>
#include <set>
#include <string_view>
#include <unordered_map>

namespace Cory::Framegraph {

/**
 * passed to the render pass coroutines when they actually execute.
 *
 * A render pass coroutine obtains this object by with a `co_await builder.finishDeclaration()`.
 * It will be (potentially) resumed inside the Framegraph::execute() function,
 * after all resources have been resolved and can be queried through the @a resources
 * member.
 */
struct RenderInput {
    struct RenderPassResources *resources{};
    struct RenderContext *context{};
    CommandList *cmd{};
};

/**
 * an Awaitable that will enqueue the current coroutine for execution on the given framegraph
 * when the render pass gets scheduled.
 *
 * Note that the coroutine may never be resumed if the render pass identified by the @a passHandle
 * does not get scheduled.
 */
struct RenderTaskExecutionAwaiter {
    RenderTaskHandle passHandle;
    Framegraph &fg;
    [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }
    [[nodiscard]] RenderInput await_resume() const noexcept;
    void await_suspend(cppcoro::coroutine_handle<> coroHandle) const noexcept;
};

struct ExecutionInfo {
    struct TransitionInfo {
        enum class Direction { ResourceToTask, TaskToResource };
        Direction direction;
        RenderTaskHandle task;
        TransientTextureHandle resource;
        TextureState stateBefore;
        TextureState stateAfter;
    };
    std::vector<RenderTaskHandle> tasks;
    std::vector<TextureHandle> resources;
    std::vector<TransitionInfo> transitions;
};

/**
 * The framegraph.
 *
 * Is meant to be filled with Cory::Framegraph::Builder
 */
class Framegraph : NoCopy, NoMove {
  public:
    explicit Framegraph(Context &ctx);
    ~Framegraph();

    ExecutionInfo record(Magnum::Vk::CommandBuffer &cmdBuffer);

    void retireImmediate();

    Builder declareTask(std::string_view name);

    /// to be called from Builder
    RenderInput renderInput(RenderTaskHandle passHandle);

    /// declare an external texture as an input
    TransientTextureHandle declareInput(TextureInfo info,
                                        Layout layout,
                                        AccessFlags lastWriteAccess,
                                        PipelineStages lastWriteStage,
                                        Magnum::Vk::Image &image,
                                        Magnum::Vk::ImageView &imageView);

    /**
     * declare that a resource is to be read afterwards. returns general
     * information and synchronization state of the last write to the
     * texture so external code can synchronize with it
     */
    std::pair<TextureInfo, TextureState> declareOutput(TransientTextureHandle handle);

    [[nodiscard]] std::string dump(const ExecutionInfo &info);

  private: /* member functions */
    RenderTaskHandle finishPassDeclaration(RenderTaskInfo &&info)
    {
        return renderTasks_.emplace(info);
    }

    /// to be called from RenderTaskExecutionAwaiter - the Framegraph takes ownership of the @a
    /// coroHandle
    void enqueueRenderPass(RenderTaskHandle passHandle, cppcoro::coroutine_handle<> coroHandle)
    {
        renderTasks_[passHandle].coroHandle = coroHandle;
    }

    /**
     * @brief resolve which render tasks need to be executed for requested resources
     *
     * Returns the tasks that need to be executed in the given order, and all resources that
     * are required to execute said resources.
     * Updates the internal information about which render pass is required.
     */
    ExecutionInfo resolve(const std::vector<TransientTextureHandle> &requestedResources);

    ExecutionInfo compile();
    void executePass(CommandList &cmd, RenderTaskHandle handle);

    cppcoro::generator<std::pair<RenderTaskHandle, const RenderTaskInfo &>> renderTasks() const;

  private:                             /* members */
    friend Builder;                    // convenience so it can call finishPassDeclaration
    friend RenderTaskExecutionAwaiter; // so it can call enqueueRenderPass
    friend FramegraphVisualizer;       // accesses all the internals

    Context *ctx_;
    TextureResourceManager resources_;
    std::vector<TransientTextureHandle> externalInputs_;
    std::vector<TransientTextureHandle> outputs_;

    SlotMap<RenderTaskInfo> renderTasks_;
    CommandList *commandListInProgress_{};
};

} // namespace Cory::Framegraph
