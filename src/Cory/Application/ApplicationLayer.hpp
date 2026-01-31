#pragma once

#include <Cory/Application/Common.hpp>
#include <Cory/Application/Event.hpp>
#include <Cory/Framegraph/Common.hpp>
#include <Cory/Framegraph/RenderTaskBuilder.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>

#include <string>

namespace Cory {

struct LogicUpdateContext {
    double simulationTime; ///< the time in seconds since the start of the simulation
    double deltaTime;      ///< time since the last update
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
        : name_(std::move(name))
    {
    }

    virtual ~ApplicationLayer() = default;

    /// called when the layer is attached to the layer stack
    virtual void onAttach([[maybe_unused]] Context &ctx, [[maybe_unused]] LayerAttachInfo info) {}
    /// called when the layer is detached from the layer stack
    virtual void onDetach([[maybe_unused]] Context &ctx) {}

    /// called on any UI event. enqueue any expensive actions and process in the update loop
    virtual bool onEvent([[maybe_unused]] Event event) { return false; }

    /// called once per frame. use to update any state
    virtual void onUpdate([[maybe_unused]] const LogicUpdateContext &updateCtx) {}

    /**
     * used to query whether the layer has a render task. if this returns true, the renderTask
     * method will be called
     */
    virtual bool hasRenderTask() const { return false; }
    /// if hasRenderTask() returns true, this method will be called to get the coroutine render task
    virtual RenderTaskDeclaration<LayerPassOutputs> renderTask(
        [[maybe_unused]] Cory::RenderTaskBuilder builder,
        [[maybe_unused]] LayerPassOutputs previousLayer)
    {
        co_return;
    }

    std::string_view name() const { return name_; }

  private:
    std::string name_;
};

} // namespace Cory
