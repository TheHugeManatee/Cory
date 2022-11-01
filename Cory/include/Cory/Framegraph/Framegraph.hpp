#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
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

struct RenderInput {
    struct RenderPassResources *resources;
    struct RenderContext *context;
};

class Framegraph;

class DependencyGraph {
  public:
    void recordCreates(RenderPassHandle pass, TextureHandle resource)
    {
        CO_CORE_DEBUG("Pass {}: Creates '{}' as {}x{}x{} ({} {})",
                      pass,
                      resource.name,
                      resource.size.x,
                      resource.size.y,
                      resource.size.z,
                      resource.format,
                      resource.layout);
        creates_.emplace_back(pass, resource);
    }
    void recordReads(RenderPassHandle pass, TextureHandle resource)
    {
        CO_CORE_DEBUG("Pass {}: Reads '{}' with layout {}", pass, resource.name, resource.layout);
        reads_.emplace_back(pass, resource);
    }
    void recordWrites(RenderPassHandle pass, TextureHandle resource)
    {
        CO_CORE_DEBUG("Pass {}: Writes '{}' as layout {}", pass, resource.name, resource.layout);
        writes_.emplace_back(pass, resource);
    }

    void dump();

    std::vector<RenderPassHandle>
    resolve(const std::vector<ResourceHandle> &requestedResources) const;

  private:
    struct Record {
        RenderPassHandle pass;
        TextureHandle texture;
    };

    std::vector<Record> creates_;
    std::vector<Record> reads_;
    std::vector<Record> writes_;
};

struct RenderPassExecutionAwaiter {
    RenderPassHandle passHandle;
    Framegraph &fg;
    constexpr bool await_ready() const noexcept { return false; }
    RenderInput await_resume() const noexcept;
    void await_suspend(cppcoro::coroutine_handle<> coroHandle) const noexcept;
};

// a builder that allows a render pass to declare specific dependencies (inputs and outputs)
class Builder {
  public:
    Builder(Framegraph &framegraph, std::string_view passName)
        : passHandle_{passName}
        , framegraph_{framegraph}
    {
        CO_CORE_TRACE("Pass {}: declaration started", passName);
    }
    ~Builder() { CO_CORE_TRACE("Pass {}: Builder destroyed", passHandle_); }

    /// declares a dependency to the named resource
    cppcoro::task<TextureHandle> read(TextureHandle &h);

    /// declare that a render pass creates a certain texture
    cppcoro::task<MutableTextureHandle>
    create(std::string name, glm::u32vec3 size, PixelFormat format, Layout finalLayout);

    /// declare that a render pass writes to a certain texture
    cppcoro::task<MutableTextureHandle> write(TextureHandle handle);

    /// obtain the handle to the render pass itself
    RenderPassHandle passHandle() const noexcept { return passHandle_; }

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
    RenderPassHandle passHandle_;
    Framegraph &framegraph_;
};

class Framegraph : NoCopy, NoMove {
  public:
    friend Builder;
    friend RenderPassExecutionAwaiter;

    ~Framegraph();

    void execute();

    Builder declarePass(std::string_view name);

    // to be called from Builder
    RenderInput renderInput() { return {}; }

    // declare an external texture as an input
    TextureHandle declareInput(std::string_view name);
    // declare that an external resource is to be read afterwards
    void declareOutput(TextureHandle handle);

  private:
    // to be called from Builder
    void enqueueRenderPass(RenderPassHandle passHandle, cppcoro::coroutine_handle<> coroHandle)
    {
        renderPasses_.insert({std::move(passHandle), coroHandle});
    }

    std::vector<RenderPassHandle> compile();

    DependencyGraph graph_;
    TextureResourceManager resources_;
    std::vector<ResourceHandle> outputs_;

    std::unordered_map<RenderPassHandle, cppcoro::coroutine_handle<>> renderPasses_;
};

} // namespace Cory::Framegraph
