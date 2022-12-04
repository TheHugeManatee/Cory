#include "FramegraphVisualizer.h"

#include <Cory/Framegraph/TextureManager.hpp>
#include <range/v3/algorithm/contains.hpp>

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
    struct DependencyInfo {};

    std::unordered_map<TransientTextureHandle, TextureData> textures;
    std::unordered_map<RenderTaskHandle, TaskData> tasks;
    std::vector<DependencyInfo> inputDependencies;
    std::vector<DependencyInfo> outputDependencies;
};

void FramegraphVisualizer::build(Index &index, const ExecutionInfo &executionInfo) const
{
    for (const auto &[passHandle, passInfo] : graph_.renderTasks()) {
        index.tasks[passHandle].info = passInfo;
        index.tasks[passHandle].executed = ranges::contains(executionInfo.tasks, passHandle);

        for (const auto &[inputHandle, _] : passInfo.inputs) {
            auto inputInfo = graph_.resources_.info(inputHandle.texture);

            index.textures[inputHandle].handle = inputHandle;
            index.textures[inputHandle].info = inputInfo;
        }

        for (const auto &[outputHandle, kind, _] : passInfo.outputs) {
            auto outputInfo = graph_.resources_.info(outputHandle.texture);
            const std::string color = kind == TaskOutputKind::Create ? "darkgreen" : "black";
            const std::string label = kind == TaskOutputKind::Create ? "creates" : "";

            index.textures[outputHandle].handle = outputHandle;
            index.textures[outputHandle].info = outputInfo;
        }
    }
    for (auto externalInput : graph_.externalInputs_) {
        index.textures.at(externalInput).external = true;
    }
    for (auto externalOutput : graph_.outputs_) {
        index.textures.at(externalOutput).output = true;
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
    };

    for (const auto &[h, taskData] : index.tasks) {
        const auto *const passColor = (!taskData.info.coroHandle) ? "red"
                                      : taskData.executed         ? "black"
                                                                  : "gray";
        append(
            "  \"{0}\" [shape=ellipse,color={1},fontcolor={1}]\n", make_label(taskData), passColor);

        for (const auto &[inputHandle, accessInfo] : taskData.info.inputs) {
            const auto &textureData = index.textures[inputHandle];
            append("  \"{}\" -> \"{}\" \n", make_label(textureData), make_label(taskData));
        }

        for (const auto &[outputHandle, kind, _] : taskData.info.outputs) {
            const auto &textureData = index.textures[outputHandle];
            const std::string color = kind == TaskOutputKind::Create ? "darkgreen" : "black";
            const std::string label = kind == TaskOutputKind::Create ? "creates" : "";
            append("  \"{}\" -> \"{}\" [style=dashed,color={},label=\"{}\"]\n",
                   make_label(taskData),
                   make_label(textureData),
                   color,
                   label);
        }
    }

    for (const auto &[handle, textureData] : index.textures) {
        auto state = graph_.resources_.state(handle.texture);
        const std::string color = ranges::contains(graph_.externalInputs_, handle) ? "blue"
                                  : ranges::contains(executionInfo.resources, handle.texture)
                                      ? "black"
                                      : "gray";
        const std::string label =
            fmt::format("{} {}\\n[{} {},{}]",
                        make_label(textureData),
                        state.status == TextureMemoryStatus::External ? " (ext)" : "",
                        textureData.info.size,
                        textureData.info.format,
                        state.layout);
        const float penWidth = ranges::contains(graph_.outputs_, handle) ? 3.0f : 1.0f;
        append("  \"{0}\" [shape=rectangle,label=\"{1}\",color={2},fontcolor={2},penwidth={3}]\n",
               make_label(textureData),
               label,
               color,
               penWidth);
    }

    out += "}\n";

    return out;
}

} // namespace Cory::Framegraph