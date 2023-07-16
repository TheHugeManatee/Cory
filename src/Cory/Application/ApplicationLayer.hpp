#pragma once

#include "kdbindings/property.h"
#include <Cory/Application/Common.hpp>
#include <Cory/Application/Event.hpp>
#include <Cory/Framegraph/Common.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>

#include <string>

namespace Cory {

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
        : name(name)
    {
    }

    virtual ~ApplicationLayer() = default;

    /// called when the layer is attached to the layer stack
    virtual void onAttach(Context &ctx, LayerAttachInfo info) {}
    /// called when the layer is detached from the layer stack
    virtual void onDetach(Context &ctx) {}

    /// called on any UI event. enqueue any expensive actions and process in the update loop
    virtual bool onEvent(Event event) { return false; }

    /// called once per frame. use to update any state
    virtual void onUpdate() {}

    /**
     * used to query whether the layer has a render task. if this returns true, the renderTask
     * method will be called
     */
    virtual bool hasRenderTask() const { return false; }
    /// if hasRenderTask() returns true, this method will be called to get the coroutine render task
    virtual RenderTaskDeclaration<LayerPassOutputs> renderTask(Cory::RenderTaskBuilder builder,
                                                               LayerPassOutputs previousLayer) = 0;

    kdb::Property<std::string> name;
};

} // namespace Cory