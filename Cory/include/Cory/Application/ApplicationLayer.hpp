#pragma once

#include <Cory/Framegraph/Common.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>

#include <string>

namespace Cory {
struct LayerPassOutputs {
    TransientTextureHandle color;
    TransientTextureHandle depth;
};

class ApplicationLayer {
  public:
    ApplicationLayer(std::string name)
        : name_(name)
    {
    }

    virtual ~ApplicationLayer() = default;

    virtual void onAttach(Context &ctx) {}
    virtual void onDetach(Context &ctx) {}
    virtual void onEvent() {}
    virtual void onUpdate() {}
    virtual RenderTaskDeclaration<LayerPassOutputs> renderTask(Cory::RenderTaskBuilder builder,
                                                               LayerPassOutputs previousLayer) = 0;

    const std::string &name() { return name_; }

  private:
    std::string name_;
};

} // namespace Cory