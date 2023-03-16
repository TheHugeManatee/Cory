#pragma once

#include <Cory/Application/Event.hpp>
#include <Cory/Framegraph/Common.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>

#include <string>

namespace Cory {

/// the outputs of a layer's render task
struct LayerPassOutputs {
    TransientTextureHandle color;
    TransientTextureHandle depth;
};

/// data to be passed to a layer when it is attached
struct LayerAttachInfo {
    uint32_t maxFramesInFlight;
};

/**
 * a base class for application layers
 *
 * Application layers define a layer stack that defines 2D render order and interaction priorities.
 * Each layer can react to events and perform according actions in the event loop.
 * It can optionally enqueue a render task to render on top of the previous layer.
 */
class ApplicationLayer {
  public:
    ApplicationLayer(std::string name)
        : name_(name)
    {
    }

    virtual ~ApplicationLayer() = default;

    virtual void onAttach(Context &ctx, LayerAttachInfo info) {}
    virtual void onDetach(Context &ctx) {}
    virtual bool onEvent(Event event) { return false; }
    virtual void onUpdate() {}
    virtual RenderTaskDeclaration<LayerPassOutputs> renderTask(Cory::RenderTaskBuilder builder,
                                                               LayerPassOutputs previousLayer) = 0;

    const std::string &name() { return name_; }

  private:
    std::string name_;
};

} // namespace Cory