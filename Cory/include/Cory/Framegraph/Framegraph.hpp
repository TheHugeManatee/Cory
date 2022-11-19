#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
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
    struct RenderPassResources *resources;
    struct RenderContext *context;
};

struct RenderPassInfo {
    std::string name;
    std::vector<TextureHandle> inputs;
    std::vector<std::pair<TextureHandle, PassOutputKind>> outputs;
    cppcoro::coroutine_handle<> coroHandle;
    int32_t executionPriority{-1};
};

/**
 * an Awaitable that will enqueue the current coroutine for execution on the given framegraph
 * when the render pass gets scheduled.
 *
 * Note that the coroutine may never be resumed if the render pass identified by the @a passHandle
 * does not get scheduled.
 */
struct RenderPassExecutionAwaiter {
    RenderPassHandle passHandle;
    Framegraph &fg;
    constexpr bool await_ready() const noexcept { return false; }
    RenderInput await_resume() const noexcept;
    void await_suspend(cppcoro::coroutine_handle<> coroHandle) const noexcept;
};

/// a builder that allows a render pass to declare specific dependencies (inputs and outputs)
class Builder : NoCopy, NoMove {
  public:
    Builder(Framegraph &framegraph, std::string_view passName)
        : passName_{passName}
        , framegraph_{framegraph}
    {
        CO_CORE_TRACE("Pass {}: declaration started", passName);
    }
    ~Builder() { CO_CORE_TRACE("Pass {}: Builder destroyed", passName_); }

    /// declares a dependency to the named resource
    cppcoro::task<TextureHandle> read(TextureHandle &h);

    /// declare that a render pass creates a certain texture
    cppcoro::task<MutableTextureHandle>
    create(std::string name, glm::u32vec3 size, PixelFormat format, Layout finalLayout);

    /// declare that a render pass writes to a certain texture
    cppcoro::task<MutableTextureHandle> write(TextureHandle handle);

    /**
     * @brief Finish declaration of the render pass.
     *
     * co_await'ing on the returned awaiter will suspend exection of the current coroutine
     * and enqueue it to the frame graph. Execution will resume on the framegraph's execution
     * context if the framegraph determines that this render pass will need to be resumed
     * at all (the render pass provides resources that another pass consumes). If the resources
     * of the render pass are not needed, the coroutine will never be resumed.
     */
    RenderPassExecutionAwaiter finishDeclaration();

  private:
    std::string passName_;
    Framegraph &framegraph_;
    std::vector<TextureHandle> inputs;
    std::vector<std::pair<TextureHandle, PassOutputKind>> outputs;
};

class Framegraph : NoCopy, NoMove {
  public:
    friend Builder;                    // convenience so it can access the graph_ object directly
    friend RenderPassExecutionAwaiter; // so it can call enqueueRenderPass

    ~Framegraph();

    void execute();

    Builder declarePass(std::string_view name);

    /// to be called from Builder
    RenderInput renderInput() { return {}; }

    /// declare an external texture as an input - TODO arguments
    TextureHandle declareInput(std::string_view name);
    /// declare that an external resource is to be read afterwards
    void declareOutput(TextureHandle handle);

    void dump();

  private:
    RenderPassHandle finishPassDeclaration(RenderPassInfo &&info)
    {
        return renderPasses_.emplace(info);
    }

    /// to be called from RenderPassExecutionAwaiter - the Framegraph takes ownership of the @a
    /// coroHandle
    void enqueueRenderPass(RenderPassHandle passHandle, cppcoro::coroutine_handle<> coroHandle)
    {
        renderPasses_[passHandle].coroHandle = std::move(coroHandle);
    }

    /**
     * @brief resolve which render passes need to be executed for requested resources
     *
     * Returns the passes that need to be executed in the given order. Updates the internal
     * information about which render pass is required.
     */
    std::vector<RenderPassHandle>
    resolve(const std::vector<TransientTextureHandle> &requestedResources);

    std::vector<RenderPassHandle> compile();

    TextureResourceManager resources_;
    std::vector<TransientTextureHandle> externalInputs_;
    std::vector<TransientTextureHandle> outputs_;

    SlotMap<RenderPassInfo> renderPasses_;
};

} // namespace Cory::Framegraph
