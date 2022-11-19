#pragma once

#include <Cory/Framegraph/Common.hpp>

#include <string_view>
#include <cppcoro/task.hpp>

namespace Cory::Framegraph {
/// a builder that allows a render pass to declare specific dependencies (inputs and outputs)
class Builder : NoCopy, NoMove {
  public:
    Builder(Framegraph &framegraph, std::string_view passName);
    ~Builder();

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

}
