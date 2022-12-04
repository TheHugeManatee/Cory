#include "FramegraphVisualizer.h"

#include <Cory/Framegraph/TextureManager.hpp>
#include <range/v3/algorithm/contains.hpp>

namespace Cory::Framegraph {

std::string FramegraphVisualizer::generateDotGraph(const ExecutionInfo &executionInfo) const
{
    std::string out{"digraph G {\n"
                    "rankdir=LR;\n"
                    "node [fontsize=12,fontname=\"Courier New\"]\n"
                    "edge [fontsize=10,fontname=\"Courier New\"]\n"};

    std::unordered_map<TransientTextureHandle, TextureInfo> textures;

    for (const auto &[passHandle, passInfo] : graph_.renderTasks()) {
        const std::string passCol = (!passInfo.coroHandle)                              ? "red"
                                    : ranges::contains(executionInfo.tasks, passHandle) ? "black"
                                                                                        : "gray";
        fmt::format_to(std::back_inserter(out),
                       "  \"{0}\" [shape=ellipse,color={1},fontcolor={1}]\n",
                       passInfo.name,
                       passCol);

        for (const auto &[inputHandle, _] : passInfo.inputs) {
            auto inputInfo = graph_.resources_.info(inputHandle.texture);
            fmt::format_to(std::back_inserter(out),
                           "  \"{} v{}\" -> \"{}\" \n",
                           inputInfo.name,
                           inputHandle.version,
                           passInfo.name);
            textures[inputHandle] = inputInfo;
        }

        for (const auto &[outputHandle, kind, _] : passInfo.outputs) {
            auto outputInfo = graph_.resources_.info(outputHandle.texture);
            const std::string color = kind == TaskOutputKind::Create ? "darkgreen" : "black";
            const std::string label = kind == TaskOutputKind::Create ? "creates" : "";
            fmt::format_to(std::back_inserter(out),
                           "  \"{}\" -> \"{} v{}\" [style=dashed,color={},label=\"{}\"]\n",
                           passInfo.name,
                           outputInfo.name,
                           outputHandle.version,
                           color,
                           label);
            textures[outputHandle] = outputInfo;
        }
    }

    for (const auto &[handle, info] : textures) {
        auto state = graph_.resources_.state(handle.texture);
        const std::string color = ranges::contains(graph_.externalInputs_, handle) ? "blue"
                                  : ranges::contains(executionInfo.resources, handle.texture)
                                      ? "black"
                                      : "gray";
        const std::string label =
            fmt::format("{} v{}{}\\n[{} {},{}]",
                        info.name,
                        handle.version,
                        state.status == TextureMemoryStatus::External ? " (ext)" : "",
                        info.size,
                        info.format,
                        state.layout);
        const float penWidth = ranges::contains(graph_.outputs_, handle) ? 3.0f : 1.0f;
        fmt::format_to(
            std::back_inserter(out),
            "  \"{0} v{1}\" [shape=rectangle,label=\"{2}\",color={3},fontcolor={3},penwidth={4}]\n",
            info.name,
            handle.version,
            label,
            color,
            penWidth);
    }

    out += "}\n";

    return out;
}
} // namespace Cory::Framegraph