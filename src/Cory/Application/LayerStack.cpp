#include "Cory/Application/LayerStack.hpp"

#include "Cory/Application/ApplicationLayer.hpp"
#include "Cory/Framegraph/Framegraph.hpp"

#include <range/v3/view/reverse.hpp>

namespace Cory {

LayerStack::LayerStack(Context &ctx)
    : ctx_{ctx}
{
}

LayerStack::~LayerStack()
{
    for (auto &layer : layers_) {
        layer->onDetach(ctx_);
    }
    priorityLayer_->onDetach(ctx_);
}

void LayerStack::attachLayer(std::unique_ptr<ApplicationLayer> layer, LayerAttachInfo attachInfo)
{
    layer->onAttach(ctx_, attachInfo);
    layers_.emplace_back(std::move(layer));
}

std::unique_ptr<ApplicationLayer> LayerStack::detachLayer(const std::string &name)
{
    auto it = std::find_if(layers_.begin(), layers_.end(), [&](const auto &layer) {
        return layer->name.get() == name;
    });
    if (it == layers_.end()) { return nullptr; }
    auto layer = std::move(*it);
    layers_.erase(it);
    layer->onDetach(ctx_);
    return layer;
}

std::unique_ptr<ApplicationLayer> LayerStack::removePriorityLayer()
{
    return std::exchange(priorityLayer_, nullptr);
}

void LayerStack::update()
{
    if (priorityLayer_) { priorityLayer_->onUpdate(); }
    for (auto &layer : layers_) {
        layer->onUpdate();
    }
}

bool LayerStack::onEvent(Event event)
{
    if (priorityLayer_ && priorityLayer_->onEvent(event)) { return true; }
    for (auto &layer : ranges::views::reverse(layers_)) {
        if (layer->onEvent(event)) { return true; }
    }
    return false;
}

LayerPassOutputs LayerStack::declareRenderTasks(Framegraph &framegraph,
                                                LayerPassOutputs previousLayer)
{
    for (auto &layer : layers_) {
        if (layer->hasRenderTask()) {
            previousLayer =
                layer
                    ->renderTask(framegraph.declareTask("TASK_" + layer->name.get()), previousLayer)
                    .output();
        }
    }

    if (priorityLayer_ && priorityLayer_->hasRenderTask()) {
        previousLayer =
            priorityLayer_
                ->renderTask(framegraph.declareTask("TASK_" + priorityLayer_->name.get()),
                             previousLayer)
                .output();
    }
    return previousLayer;
}

} // namespace Cory