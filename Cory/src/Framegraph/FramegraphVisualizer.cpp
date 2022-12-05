#include "FramegraphVisualizer.h"

#include <Cory/Framegraph/TextureManager.hpp>

#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/view/enumerate.hpp>

namespace Cory::Framegraph {

struct Index {
    struct TextureData {
        TransientTextureHandle handle;
        TextureInfo info;
        bool allocated{false};
        bool external{false};
        bool output{false};
    };
    struct TaskData {
        RenderTaskInfo info;
        bool executed{false};
    };
    struct DependencyInfo {
        TransientTextureHandle resource;
        RenderTaskHandle task;
        std::optional<ExecutionInfo::TransitionInfo> transitionInfo;
    };

    std::unordered_map<TransientTextureHandle, TextureData> textures;
    std::unordered_map<RenderTaskHandle, TaskData> tasks;
    std::vector<DependencyInfo> inputDependencies;
    std::vector<DependencyInfo> outputDependencies;
    std::vector<DependencyInfo> createDependencies;
};

void FramegraphVisualizer::build(Index &index, const ExecutionInfo &executionInfo) const
{
    auto findTransitionInfo =
        [&](TransientTextureHandle resource,
            RenderTaskHandle task) -> std::optional<ExecutionInfo::TransitionInfo> {
        auto it = ranges::find_if(executionInfo.transitions,
                                  [&](const ExecutionInfo::TransitionInfo &info) {
                                      return info.resource == resource && info.task == task;
                                  });
        if (it == executionInfo.transitions.end()) { return std::nullopt; }
        return *it;
    };

    for (const auto &[taskHandle, passInfo] : graph_.renderTasks()) {
        index.tasks[taskHandle].info = passInfo;
        index.tasks[taskHandle].executed = ranges::contains(executionInfo.tasks, taskHandle);

        for (const auto &[inputHandle, _] : passInfo.inputs) {
            auto inputInfo = graph_.resources_.info(inputHandle.texture);

            index.textures[inputHandle].handle = inputHandle;
            index.textures[inputHandle].info = inputInfo;

            index.inputDependencies.emplace_back(Index::DependencyInfo{
                .resource = inputHandle,
                .task = taskHandle,
                .transitionInfo = findTransitionInfo(inputHandle, taskHandle)});
        }

        for (const auto &[outputHandle, kind, _] : passInfo.outputs) {
            auto outputInfo = graph_.resources_.info(outputHandle.texture);

            index.textures[outputHandle].handle = outputHandle;
            index.textures[outputHandle].info = outputInfo;

            (kind == TaskOutputKind::Create ? index.createDependencies : index.outputDependencies)
                .emplace_back(Index::DependencyInfo{
                    .resource = outputHandle,
                    .task = taskHandle,
                    .transitionInfo = findTransitionInfo(outputHandle, taskHandle)});
        }
    }
    // mark all external inputs
    for (auto externalInput : graph_.externalInputs_) {
        if (index.textures.contains(externalInput)) {
            index.textures.at(externalInput).external = true;
        }
        else {
            auto inputInfo = graph_.resources_.info(externalInput.texture);
            index.textures.insert(std::make_pair(
                externalInput,
                Index::TextureData{.handle = externalInput,
                                   .info = graph_.resources_.info(externalInput.texture),
                                   .external = true}));
        }
    }
    // mark all output resources
    for (auto externalOutput : graph_.outputs_) {
        index.textures.at(externalOutput).output = true;
    }
    // mark all texture entries that refer to an allocated resource as allocated
    for (auto allocated : executionInfo.resources) {
        for (auto &[h, data] : index.textures) {
            if (data.handle.texture == allocated) { data.allocated = true; }
        }
    }
}

#define append(...) fmt::format_to(std::back_inserter(out), __VA_ARGS__)

std::string FramegraphVisualizer::generateDotGraph(const ExecutionInfo &executionInfo) const
{
    Index index;
    build(index, executionInfo);

    std::string out{"digraph G {\n"
                    "rankdir=LR;\n"
                    "node [fontsize=12,fontname=\"Courier New\"]\n"
                    "edge [fontsize=10,fontname=\"Courier New\"]\n"};

    auto make_label = [](const auto &thing) -> std::string {
        using ThingType = std::decay_t<decltype(thing)>;
        if constexpr (std::is_same_v<ThingType, Index::TextureData>) {
            return fmt::format("{} v{}", thing.info.name, thing.handle.version);
        }
        if constexpr (std::is_same_v<ThingType, Index::TaskData>) { return thing.info.name; }
        if constexpr (std::is_same_v<ThingType, TextureState>) {
            return fmt::format("layout={}\\nstage={}\\naccess={}",
                               thing.layout,
                               thing.lastWriteStage,
                               thing.lastWriteAccess);
            //                label = fmt::format("{} >> {}\\n{} >> {}\\n{} >> {}",
            //                                    thing.transitionInfo->stateBefore.layout,
            //                                    thing.transitionInfo->stateAfter.layout,
            //                                    thing.transitionInfo->stateBefore.lastWriteStage,
            //                                    thing.transitionInfo->stateAfter.lastWriteStage,
            //                                    thing.transitionInfo->stateBefore.lastWriteAccess,
            //                                    thing.transitionInfo->stateAfter.lastWriteAccess);
        }
    };

    for (const auto &[h, taskData] : index.tasks) {
        const auto *const passColor = (!taskData.info.coroHandle) ? "red"
                                      : taskData.executed         ? "black"
                                                                  : "gray";
        append(
            "  \"{0}\" [shape=ellipse,color={1},fontcolor={1}]\n", make_label(taskData), passColor);
    }

    for (const auto &[handle, textureData] : index.textures) {
        const std::string color = textureData.external    ? "blue"
                                  : textureData.allocated ? "black"
                                                          : "gray";
        const std::string label = fmt::format("{} {}\\n[{} {}]",
                                              make_label(textureData),
                                              textureData.external ? " (ext)" : "",
                                              textureData.info.size,
                                              textureData.info.format);
        const float penWidth = textureData.output ? 3.0f : 1.0f;
        append("  \"{0}\" [shape=rectangle,label=\"{1}\",color={2},fontcolor={2},penwidth={3}]\n",
               make_label(textureData),
               label,
               color,
               penWidth);
    }

    for (const Index::DependencyInfo &dep : index.inputDependencies) {
        append("  \"{}\" -> \"{}\" [label=\"{}\"]\n",
               make_label(index.textures[dep.resource]),
               make_label(index.tasks[dep.task]),
               "");
    }
    for (const Index::DependencyInfo &dep : index.createDependencies) {

        const std::string color = "darkgreen";
        const std::string label = fmt::format(
            "{}", dep.transitionInfo ? dep.transitionInfo->stateAfter.layout : Layout::Undefined);
        append("  \"{}\" -> \"{}\" [style=dashed,color={},label=\"{}\"]\n",
               make_label(index.tasks[dep.task]),
               make_label(index.textures[dep.resource]),
               color,
               label);
    }

    for (const auto &[idx, dep] : ranges::views::enumerate(index.outputDependencies)) {

        const std::string barrierName = fmt::format("Barrier_{}", idx);
        if (dep.transitionInfo) {
            const std::string color = "orange";
            append("  \"{}\" [shape=diamond,color={},label=\"Barrier\"]", barrierName, color);

            append("  \"{}\" -> \"{}\" [color={},label=\"{}\"]\n",
                   barrierName,
                   make_label(index.textures[dep.resource]),
                   "black",
                   make_label(dep.transitionInfo->stateBefore));

            append("  \"{}\" -> \"{}\" [color={},label=\"{}\"]\n",
                   make_label(index.tasks[dep.task]),
                   barrierName,
                   "black",
                   make_label(dep.transitionInfo->stateBefore));
        }
        else {
            const std::string color = "red";
            append("  \"{}\" -> \"{}\" [color={},label=\"{}\"]\n",
                   make_label(index.tasks[dep.task]),
                   make_label(index.textures[dep.resource]),
                   color,
                   "<no barrier>");
        }
    }

    out += "}\n";
    return out;
}

} // namespace Cory::Framegraph