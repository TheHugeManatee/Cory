#pragma once

#include <Cory/Base/SimulationClock.hpp>
#include <Cory/SceneGraph/Common.hpp>
#include <Cory/Base/Function.hpp>

#include <memory>

namespace Cory {

/**
 * @brief System to manage editor UI for components.
 */
class ComponentEditorSystem {
public:
    using EditorFunction = Function<void(SceneGraph&, Entity)>;
    ComponentEditorSystem();
    ~ComponentEditorSystem();

    void tick(SceneGraph &graph, TickInfo tickInfo);

    void addComponentEditor(std::string componentName, EditorFunction editorFunction);

private:
    struct ComponentEditorPrivate;
    std::unique_ptr<ComponentEditorPrivate> data_;
};

} // namespace Cory
