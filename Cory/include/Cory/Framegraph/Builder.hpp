#pragma once

#include <Cory/Framegraph/Common.hpp>
#include <Cory/Framegraph/TransientRenderPass.hpp>

#include <cppcoro/coroutine.hpp>
#include <string_view>

namespace Cory::Framegraph {

struct RenderTaskInfo {
    struct InputDesc {
        TextureHandle handle;
        TextureAccessInfo accessInfo;
    };
    struct OutputDesc {
        TextureHandle handle;
        PassOutputKind kind;
        TextureAccessInfo accessInfo;
    };
    std::string name;
    std::vector<InputDesc> inputs;
    std::vector<OutputDesc> outputs;

    // framegraph internal stuff
    cppcoro::coroutine_handle<> coroHandle;
    int32_t executionPriority{-1}; ///< assigned when the render graph is resolved
};

/**
 * a builder that allows a render pass to declare specific dependencies (inputs and outputs).
 *
 * For defaults, see default values in RenderTaskInfo.
 *
 * Meant to be used only locally, hence not copyable.
 */
class Builder : NoCopy {
  public:
    Builder(Framegraph &framegraph, std::string_view passName);
    ~Builder();

    Builder(Builder&&) = default;

    /// declare that a render pass creates a certain texture
    TextureHandle create(std::string name,
                         glm::u32vec3 size,
                         PixelFormat format,
                         Layout initialLayout,
                         PipelineStages writeStage,
                         AccessFlags writeAccess);

    /// declares a dependency to the named resource
    TextureInfo read(TextureHandle &h, TextureAccessInfo readAccess);

    /// declare that a render task writes to a certain texture
    std::pair<TextureHandle, TextureInfo> write(TextureHandle handle,
                                                TextureAccessInfo writeAccess);

    /// declare that a render task reads from and writes to a certain texture
    std::pair<TextureHandle, TextureInfo>
    readWrite(TextureHandle handle, TextureAccessInfo readAccess, TextureAccessInfo writeAccess);

    /**
     * Declares a render pass with a default pipeline setup
     * @param name              name of the render pass
     * @return a builder class to set up the render pass. call finish() to obtain the pass object
     */
    TransientRenderPassBuilder declareRenderPass(std::string_view name = "");

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

  private:
    RenderTaskInfo info_;
    Framegraph &framegraph_;
};

} // namespace Cory::Framegraph
