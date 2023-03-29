#pragma once

#include <Cory/Application/Common.hpp>
#include <Cory/Application/Event.hpp>
#include <Cory/Renderer/Common.hpp>

namespace Cory {

/**
 * @brief collects a stack of ApplicationLayers
 *
 * The layer stack defines the order in which layers are rendered and the order in which they
 * receive events.
 *  - Events are passed "downwards" in the layer stack:
 *    Layers earlier in the stack are rendered first and receive events *last*.
 *  - Updates happens bottom-up: Layers earlier in the stack are updated first.
 *  - Layers are rendered bottom-up: Layers earlier in the stack are rendered first.
 */
class LayerStack {
  public:
    explicit LayerStack(Context &ctx);
    ~LayerStack();

    /// emplace a layer
    template <typename T, typename... Args>
        requires std::is_base_of_v<ApplicationLayer, T>
    T &addLayer(LayerAttachInfo attachInfo, Args... args)
    {
        auto &layer = layers_.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
        layer->onAttach(ctx_, attachInfo);
        return *static_cast<T *>(layers_.back().get());
    }

    std::unique_ptr<ApplicationLayer> removeLayer(const std::string &name);

    /// update all layers
    void update();

    /// pass an event top-down to the first layer that accepts it
    bool onEvent(Event event);

    /// queue render tasks for all layers, bottom-up
    [[maybe_unused]] LayerPassOutputs declareRenderTasks(Framegraph &framegraph,
                                                         LayerPassOutputs previousLayer);

    /// list layers
    const std::vector<std::unique_ptr<ApplicationLayer>> &layers() const { return layers_; }

  private:
    Context &ctx_;
    std::vector<std::unique_ptr<ApplicationLayer>> layers_;
};
} // namespace Cory