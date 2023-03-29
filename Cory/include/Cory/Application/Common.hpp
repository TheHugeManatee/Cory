#pragma once

#include <glm/vec2.hpp>

#include <stdint.h>

#include <Cory/Framegraph/Common.hpp>

// forward-declare so we can declare our namespace alias here
namespace KDBindings {
} // namespace KDBindings
namespace kdb = KDBindings;

namespace Cory {
class Window;
class Application;
class LayerStack;
class ApplicationLayer;

/// data to be passed to a layer when it is attached
struct LayerAttachInfo {
    uint32_t maxFramesInFlight;
    glm::i32vec2 viewportDimensions;
};
/// the outputs of a layer's render task
struct LayerPassOutputs {
    TransientTextureHandle color;
    TransientTextureHandle depth;
};

class ImGuiLayer;
class DepthDebugLayer;

} // namespace Cory
