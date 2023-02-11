#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Framegraph/Builder.hpp>
#include <Cory/Framegraph/Common.hpp>

#include <glm/vec3.hpp>

#include <cppcoro/generator.hpp>

#include <concepts>
#include <set>
#include <string_view>
#include <unordered_map>

namespace Cory {

// provides access to the framegraph resources and metadata
struct RenderContext {
    // TBD
};

/**
 * passed to the render pass coroutines when they actually execute.
 *
 * A render pass coroutine obtains this object by with a `co_await builder.finishDeclaration()`.
 * It will be (potentially) resumed inside the Framegraph::execute() function,
 * after all resources have been resolved and can be queried through the @a resources
 * member.
 */
struct RenderInput {
    struct TextureManager *resources{};
    RenderContext *context{};
    // eventually, add accessors modify descriptors, push constants etc
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
 * Is meant to be filled with Cory::Builder
 */
class Framegraph : NoCopy {
  public:
    explicit Framegraph(Context &ctx);
    ~Framegraph();

    Framegraph(Framegraph &&);
    Framegraph &operator=(Framegraph &&);

    /**
     * @brief record the commands from all render tasks into the given command buffer
     *
     * Note that this can be only called once. It will cause all relevant render tasks to execute.
     */
    ExecutionInfo record(Magnum::Vk::CommandBuffer &cmdBuffer);

    /**
     * @brief immediately retire all resources allocated by the framegraph
     *
     * should be called only when it can be ensured that all resources are no longer in use, e.g.
     * for example when the next frame with the same swapchain image has been rendered.
     */
    void reset();

    /// declare a new render task
    Builder declareTask(std::string_view name);

    /// to be called from Builder
    RenderInput renderInput(RenderTaskHandle passHandle);

    /// declare an external texture as an input
    TransientTextureHandle declareInput(TextureInfo info,
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
    RenderTaskHandle finishPassDeclaration(RenderTaskInfo &&info);

    /// to be called from RenderTaskExecutionAwaiter - the Framegraph takes ownership of the @a
    /// coroHandle
    void enqueueRenderPass(RenderTaskHandle passHandle, cppcoro::coroutine_handle<> coroHandle);
    TextureManager &resources();

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

    cppcoro::generator<std::pair<RenderTaskHandle, const RenderTaskInfo &>> renderTasks() const;

  private:                             /* members */
    friend Builder;                    // convenience so it can call finishPassDeclaration
    friend RenderTaskExecutionAwaiter; // so it can call enqueueRenderPass
    friend FramegraphVisualizer;       // accesses all the internals

    std::unique_ptr<struct FramegraphPrivate> data_;
};

} // namespace Cory
