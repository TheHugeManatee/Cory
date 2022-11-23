#pragma once

#include <Cory/Framegraph/Common.hpp>

#include <cppcoro/task.hpp>
#include <string_view>

namespace Cory::Framegraph {


struct RenderTaskInfo {
    std::string name;
    std::vector<TextureHandle> inputs; // todo: needs to include clear value, load/store, layout
    std::vector<std::pair<TextureHandle, PassOutputKind>> outputs;

    DynamicStates states{};

    // framegraph internal stuff
    cppcoro::coroutine_handle<> coroHandle;
    int32_t executionPriority{-1}; ///< assigned when the render graph is resolved
};

/**
 * a builder that allows a render pass to declare specific dependencies (inputs and outputs).
 *
 * For defaults, see default values in RenderTaskInfo.
 *
 * Meant to be used only locally, hence not copy- or movable.
 */
class Builder : NoCopy, NoMove {
  public:
    Builder(Framegraph &framegraph, std::string_view passName);
    ~Builder();

    /// declare that a render pass creates a certain texture
    cppcoro::task<MutableTextureHandle> create(std::string name,
                                               glm::u32vec3 size,
                                               PixelFormat format,
                                               Layout initialLayout,
                                               PipelineStages writeStage);

    /// declares a dependency to the named resource
    cppcoro::task<TextureHandle> read(TextureHandle &h, TextureAccessInfo readAccess);

    /// declare that a render pass writes to a certain texture
    cppcoro::task<MutableTextureHandle> write(TextureHandle handle, TextureAccessInfo writeAccess);

    /**
     * @brief Finish declaration of the render task.
     *
     * co_await'ing on the returned awaiter will suspend execution of the current coroutine
     * and enqueue it to the frame graph. Execution will resume on the framegraph's execution
     * context if the framegraph determines that this render pass will need to be resumed
     * at all (the render pass provides resources that another pass consumes). If the resources
     * of the render pass are not needed, the coroutine will never be resumed.
     */
    RenderTaskExecutionAwaiter finishDeclaration();

    Builder &set(DepthTest depthTest) { info_.states.depthTest = depthTest; }
    Builder &set(DepthWrite depthWrite) { info_.states.depthWrite = depthWrite; }
    Builder &set(CullMode cullMode) { info_.states.cullMode = cullMode; }

  private:
    RenderTaskInfo info_;
    Framegraph &framegraph_;
};

} // namespace Cory::Framegraph
