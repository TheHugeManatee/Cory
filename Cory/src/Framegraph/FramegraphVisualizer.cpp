#include "FramegraphVisualizer.h"

#include <Cory/Framegraph/TextureManager.hpp>

#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/view/enumerate.hpp>

namespace Cory {

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

        for (const RenderTaskInfo::Dependency &dependency : passInfo.dependencies) {
            auto info = graph_.resources().info(dependency.handle.texture);

            index.textures[dependency.handle].handle = dependency.handle;
            index.textures[dependency.handle].info = info;

            std::vector<Index::DependencyInfo> &dependencyList =
                (dependency.kind.is_set(TaskDependencyKindBits::Create)
                     ? index.createDependencies
                     : (dependency.kind.is_set(TaskDependencyKindBits::Write)
                            ? index.outputDependencies
                            : index.inputDependencies));

            dependencyList.emplace_back(Index::DependencyInfo{
                .resource = dependency.handle,
                .task = taskHandle,
                .transitionInfo = findTransitionInfo(dependency.handle, taskHandle)});
        }
    }
    // mark all external inputs
    for (auto externalInput : graph_.externalInputs()) {
        if (index.textures.contains(externalInput)) {
            index.textures.at(externalInput).external = true;
        }
        else {
            auto inputInfo = graph_.resources().info(externalInput.texture);
            index.textures.insert(std::make_pair(
                externalInput,
                Index::TextureData{.handle = externalInput,
                                   .info = graph_.resources().info(externalInput.texture),
                                   .external = true}));
        }
    }
    // mark all output resources
    for (auto externalOutput : graph_.outputs()) {
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
        else if constexpr (std::is_same_v<ThingType, Index::TaskData>) {
            return thing.info.name;
        }
        else if constexpr (std::is_same_v<ThingType, TextureState>) {
            return fmt::format("layout={}\\nstage={}\\naccess={}",
                               thing.layout,
                               thing.lastWriteStage,
                               thing.lastAccess);
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
            "{}", dep.transitionInfo ? dep.transitionInfo->stateAfter : Sync::AccessType::None);
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
                   dep.transitionInfo->stateBefore);

            append("  \"{}\" -> \"{}\" [color={},label=\"{}\"]\n",
                   make_label(index.tasks[dep.task]),
                   barrierName,
                   "black",
                   dep.transitionInfo->stateBefore);
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

} // namespace Cory