#include "FramegraphVisualizer.h"

#include "RenderTaskBuilder.hpp"

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Framegraph/FramegraphResourceManager.hpp>

#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/view/enumerate.hpp>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_to_string.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string_view>
#include <utility>
#include <unordered_map>

namespace Cory {

struct Index {
    struct TextureData {
        TransientTextureHandle handle;
        TextureInfo info;
        bool allocated{false};
        bool external{false};
        bool output{false};
    };
    struct BufferData {
        TransientBufferHandle handle;
        BufferInfo info;
        bool allocated{false};
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
    struct BufferDependencyInfo {
        TransientBufferHandle resource;
        RenderTaskHandle task;
        std::optional<ExecutionInfo::BufferTransitionInfo> transitionInfo;
    };

    std::unordered_map<TransientTextureHandle, TextureData> textures;
    std::unordered_map<TransientBufferHandle, BufferData> buffers;
    std::unordered_map<RenderTaskHandle, TaskData> tasks;
    std::vector<DependencyInfo> inputDependencies;
    std::vector<DependencyInfo> outputDependencies;
    std::vector<DependencyInfo> createDependencies;
    std::vector<BufferDependencyInfo> inputBufferDependencies;
    std::vector<BufferDependencyInfo> outputBufferDependencies;
    std::vector<BufferDependencyInfo> createBufferDependencies;
};

void FramegraphVisualizer::build([[maybe_unused]] Index &index,
                                 [[maybe_unused]] const ExecutionInfo &executionInfo) const
{
    auto findTransitionInfo = [&](TransientTextureHandle resource,
                                  RenderTaskHandle task) -> std::optional<ExecutionInfo::TransitionInfo> {
        auto it = ranges::find_if(executionInfo.transitions,
                                  [&](const ExecutionInfo::TransitionInfo &info) {
                                      return info.resource == resource && info.task == task;
                                  });
        if (it == executionInfo.transitions.end()) { return std::nullopt; }
        return *it;
    };

    auto findBufferTransitionInfo = [&](TransientBufferHandle resource,
                                        RenderTaskHandle task) -> std::optional<ExecutionInfo::BufferTransitionInfo> {
        auto it = ranges::find_if(executionInfo.bufferTransitions,
                                  [&](const ExecutionInfo::BufferTransitionInfo &info) {
                                      return info.resource == resource && info.task == task;
                                  });
        if (it == executionInfo.bufferTransitions.end()) { return std::nullopt; }
        return *it;
    };

    for (const auto &[taskHandle, passInfo] : graph_.renderTasks()) {
        index.tasks[taskHandle].info = passInfo;
        index.tasks[taskHandle].executed = ranges::contains(executionInfo.tasks, taskHandle);

        for (const RenderTaskInfo::TextureDependency &dependency : passInfo.textureDependencies) {
            auto info = graph_.resources().info(dependency.handle);

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

        for (const RenderTaskInfo::BufferDependency &dependency : passInfo.bufferDependencies) {
            auto info = graph_.resources().info(dependency.handle);

            index.buffers[dependency.handle].handle = dependency.handle;
            index.buffers[dependency.handle].info = info;

            std::vector<Index::BufferDependencyInfo> &dependencyList =
                (dependency.kind.is_set(TaskDependencyKindBits::Create)
                     ? index.createBufferDependencies
                     : (dependency.kind.is_set(TaskDependencyKindBits::Write)
                            ? index.outputBufferDependencies
                            : index.inputBufferDependencies));

            dependencyList.emplace_back(Index::BufferDependencyInfo{
                .resource = dependency.handle,
                .task = taskHandle,
                .transitionInfo = findBufferTransitionInfo(dependency.handle, taskHandle)});
        }
    }

    // mark all external inputs
    for (auto externalInput : graph_.externalInputs()) {
        if (index.textures.contains(externalInput)) {
            index.textures.at(externalInput).external = true;
        }
        else {
            index.textures.insert(std::make_pair(
                externalInput,
                Index::TextureData{.handle = externalInput,
                                   .info = graph_.resources().info(externalInput),
                                   .external = true}));
        }
    }
    // mark all output resources
    for (auto externalOutput : graph_.outputs()) {
        if (index.textures.contains(externalOutput)) {
            index.textures.at(externalOutput).output = true;
        }
        else {
            index.textures.insert(std::make_pair(
                externalOutput,
                Index::TextureData{.handle = externalOutput,
                                   .info = graph_.resources().info(externalOutput),
                                   .output = true}));
        }
    }
    // mark all texture entries that refer to an allocated resource as allocated
    for (auto allocated : executionInfo.resources) {
        for (auto &[h, data] : index.textures) {
            if (data.handle.texture() == allocated) { data.allocated = true; }
        }
    }
    // mark all buffer entries that refer to an allocated resource as allocated
    for (auto allocated : executionInfo.buffers) {
        for (auto &[h, data] : index.buffers) {
            if (data.handle.buffer() == allocated) { data.allocated = true; }
        }
    }
}

namespace {
std::string escapeDotHtmlLabel(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        // Support both real newlines and literal "\\n" sequences.
        if (s[i] == '\n') {
            out += "<BR/>";
            continue;
        }
        if (s[i] == '\\' && (i + 1) < s.size() && s[i + 1] == 'n') {
            out += "<BR/>";
            ++i;
            continue;
        }

        switch (s[i]) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default: out += s[i]; break;
        }
    }
    return out;
}

std::string escapeDotString(std::string_view s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (const char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        default: out += c; break;
        }
    }
    return out;
}

std::string readFileToString(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in) { return {}; }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
} // namespace

std::string FramegraphVisualizer::generateDotGraph(const ExecutionInfo &executionInfo) const
{
    Index index;
    build(index, executionInfo);

    std::string out;
    out.reserve(16 * 1024);

    std::unordered_map<RenderTaskHandle, size_t> executionRank;
    executionRank.reserve(executionInfo.tasks.size());
    for (size_t idx = 0; idx < executionInfo.tasks.size(); ++idx) {
        executionRank.emplace(executionInfo.tasks[idx], idx);
    }

    auto append = [&]<typename... Args>(fmt::format_string<Args...> format, Args &&...args) {
        out += fmt::format(format, std::forward<Args>(args)...);
    };

    auto appendRaw = [&](std::string_view s) {
        out.append(s.data(), s.size());
    };

    constexpr std::string_view bgColor = "#1a1b26";
    constexpr std::string_view fgColor = "#a9b1d6";
    constexpr std::string_view lineColor = "#3d59a1";
    constexpr std::string_view accentColor = "#7aa2f7";
    constexpr std::string_view mutedColor = "#565f89";
    constexpr std::string_view surfaceColor = "#292e42";

    appendRaw("digraph Framegraph {\n");
    appendRaw("  rankdir=TB;\n");
    append("  bgcolor=\"{0}\";\n", bgColor);
    append("  graph [fontname=\"Inter\", fontcolor=\"{0}\", color=\"{1}\"];\n", fgColor, mutedColor);
    append("  node  [fontname=\"Inter\", fontcolor=\"{0}\", style=filled, fillcolor=\"{1}\"];\n", fgColor, surfaceColor);
    append("  edge  [fontname=\"Inter\", color=\"{0}\"];\n\n", lineColor);

    auto node_id = [&](const auto &h) {
        using HType = std::decay_t<decltype(h)>;
        return fmt::format("N{}", std::hash<HType>{}(h));
    };

    auto make_label = [](const auto &thing) -> std::string {
        using ThingType = std::decay_t<decltype(thing)>;
        if constexpr (std::is_same_v<ThingType, Index::TextureData>) {
            return fmt::format("{} v{}", thing.info.name, thing.handle.version());
        }
        else if constexpr (std::is_same_v<ThingType, Index::BufferData>) {
            return fmt::format("{} v{}", thing.info.name, thing.handle.version());
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
        const std::string tid = node_id(h);
        const std::string tlabel = escapeDotString(make_label(taskData));
        const std::string strokeColor = taskData.executed ? std::string(accentColor) : std::string(mutedColor);
        const std::string textColor = taskData.executed ? std::string(fgColor) : std::string(mutedColor);
        const auto rankIt = executionRank.find(h);
        const std::string execRankLabel = rankIt != executionRank.end()
                                              ? fmt::format("#{0}", rankIt->second + 1)
                                              : std::string{};
        append("  {0} [shape=ellipse, penwidth=2, color=\"{1}\", fontcolor=\"{2}\", fillcolor=\"{3}\", label=\"{4}\", xlabel=\"{5}\"];\n",
               tid,
               strokeColor,
               textColor,
               surfaceColor,
               tlabel,
               execRankLabel);
    }

    for (const auto &[handle, textureData] : index.textures) {
        const std::string color = textureData.external    ? std::string(accentColor)
                                      : textureData.allocated ? std::string(fgColor)
                                                              : std::string(mutedColor);
        const std::string label = fmt::format("{} {}\\n[{} {}]",
                                              make_label(textureData),
                                              textureData.external ? " (ext)" : "",
                                              textureData.info.size,
                                              vk::to_string(static_cast<vk::Format>(textureData.info.format)));
        const float penWidth = textureData.output ? 3.0f : 1.0f;
        const std::string nid = node_id(handle);
        const std::string nlabel = escapeDotString(label);
        append("  {0} [shape=box3d, penwidth={2}, color=\"{1}\", fontcolor=\"{3}\", fillcolor=\"{4}\", label=\"{5}\"];\n",
               nid,
               color,
               penWidth,
               fgColor,
               surfaceColor,
               nlabel);
    }

    for (const auto &[handle, bufferData] : index.buffers) {
        const std::string color = bufferData.allocated ? std::string(fgColor) : std::string(mutedColor);
        const std::string label = fmt::format("{}\\n[{} bytes]", make_label(bufferData), bufferData.info.size);
        const std::string bid = node_id(handle);
        const std::string blabel = escapeDotString(label);
        append("  {0} [shape=box, penwidth=1.5, color=\"{1}\", fontcolor=\"{3}\", fillcolor=\"{4}\", label=\"{5}\"];\n",
               bid,
               color,
               1.5f,
               fgColor,
               surfaceColor,
               blabel);
    }

    if (executionInfo.tasks.size() > 1) {
        for (size_t idx = 1; idx < executionInfo.tasks.size(); ++idx) {
            append("  {0} -> {1} [style=dashed, color=\"{2}\", penwidth=1, constraint=true, minlen=2, weight=4];\n",
                   node_id(executionInfo.tasks[idx - 1]),
                   node_id(executionInfo.tasks[idx]),
                   accentColor);
        }
    }

    appendRaw("\n");

    for (const Index::DependencyInfo &dep : index.inputDependencies) {
        append("  {0} -> {1};\n", node_id(index.textures[dep.resource].handle), node_id(dep.task));
    }

    for (const Index::BufferDependencyInfo &dep : index.inputBufferDependencies) {
        append("  {0} -> {1};\n", node_id(index.buffers[dep.resource].handle), node_id(dep.task));
    }

    for (const Index::DependencyInfo &dep : index.createDependencies) {
        const std::string label = fmt::format(
            "{}", dep.transitionInfo ? dep.transitionInfo->stateAfter : Sync::AccessType::None);
        append("  {0} -> {1} [style=dashed, color=\"{3}\", fontcolor=\"{3}\", label=\"{2}\"];\n",
               node_id(dep.task),
               node_id(index.textures[dep.resource].handle),
               escapeDotString(label),
               accentColor);
    }
    for (const Index::BufferDependencyInfo &dep : index.createBufferDependencies) {
        const std::string label = fmt::format(
            "{}", dep.transitionInfo ? dep.transitionInfo->stateAfter : Sync::AccessType::None);
        append("  {0} -> {1} [style=dashed, color=\"{3}\", fontcolor=\"{3}\", label=\"{2}\"];\n",
               node_id(dep.task),
               node_id(index.buffers[dep.resource].handle),
               escapeDotString(label),
               accentColor);
    }

    for (const auto &[idx, dep] : ranges::views::enumerate(index.outputDependencies)) {
        const std::string barrierName = fmt::format("Barrier_{}", idx);
        if (dep.transitionInfo) {
            append("  {0} [shape=diamond, color=\"{1}\", fontcolor=\"{1}\", label=\"Barrier\"];\n",
                   barrierName,
                   accentColor);
            append("  {0} -> {1} [color=\"{3}\", fontcolor=\"{3}\", label=\"{2}\"];\n",
                   barrierName,
                   node_id(index.textures[dep.resource].handle),
                   escapeDotString(fmt::format("{}", dep.transitionInfo->stateBefore)),
                   accentColor);
            append("  {0} -> {1} [color=\"{3}\", fontcolor=\"{3}\", label=\"{2}\"];\n",
                   node_id(dep.task),
                   barrierName,
                   escapeDotString(fmt::format("{}", dep.transitionInfo->stateBefore)),
                   accentColor);
        }
        else {
            append("  {0} -> {1} [color=\"{2}\", fontcolor=\"{3}\", label=\"<no barrier>\"];\n",
                   node_id(dep.task),
                   node_id(index.textures[dep.resource].handle),
                   lineColor,
                   mutedColor);
        }
    }

    for (const auto &[idx, dep] : ranges::views::enumerate(index.outputBufferDependencies)) {
        const std::string barrierName = fmt::format("BufferBarrier_{}", idx);
        if (dep.transitionInfo) {
            append("  {0} [shape=diamond, color=\"{1}\", fontcolor=\"{1}\", label=\"Barrier\"];\n",
                   barrierName,
                   accentColor);
            append("  {0} -> {1} [color=\"{3}\", fontcolor=\"{3}\", label=\"{2}\"];\n",
                   barrierName,
                   node_id(index.buffers[dep.resource].handle),
                   escapeDotString(fmt::format("{}", dep.transitionInfo->stateBefore)),
                   accentColor);
            append("  {0} -> {1} [color=\"{3}\", fontcolor=\"{3}\", label=\"{2}\"];\n",
                   node_id(dep.task),
                   barrierName,
                   escapeDotString(fmt::format("{}", dep.transitionInfo->stateBefore)),
                   accentColor);
        }
        else {
            append("  {0} -> {1} [color=\"{2}\", fontcolor=\"{3}\", label=\"<no barrier>\"];\n",
                   node_id(dep.task),
                   node_id(index.buffers[dep.resource].handle),
                   lineColor,
                   mutedColor);
        }
    }

    appendRaw("\n}\n");
    return out;
}

void FramegraphVisualizer::writeGraphHtml(const ExecutionInfo &executionInfo, std::filesystem::path outputPath) const
{
    const std::string dot = generateDotGraph(executionInfo);

    const auto outputDir = outputPath.has_parent_path() ? outputPath.parent_path() : std::filesystem::path{"."};
    const bool jsNextToHtml = std::filesystem::exists(outputDir / "js" / "viz-global.js")
                             && std::filesystem::exists(outputDir / "js" / "viz-wrapper.js");
    const std::string vizGlobalSrc = jsNextToHtml ? "./js/viz-global.js" : "./graphviz/js/viz-global.js";
    const std::string vizWrapperSrc = jsNextToHtml ? "./js/viz-wrapper.js" : "./graphviz/js/viz-wrapper.js";

    // Prefer deployed assets next to the executable (CMAKE_RUNTIME_OUTPUT_DIRECTORY/bin)
    // but fall back to source tree when running from repo root.
    std::filesystem::path templatePath = std::filesystem::path("graphviz") / "preview.html";
    std::string content = readFileToString(templatePath);
    if (content.empty()) {
        templatePath = std::filesystem::path("tools") / "graphviz" / "preview.html";
        content = readFileToString(templatePath);
    }

    if (content.empty()) {
        content = "<!doctype html>\n<html><head><meta charset=\"utf-8\"/>\n"
                  "<title>Graphviz (Viz.js)</title></head><body>\n"
                  "<pre class=\"dot\">\n" + dot + "\n</pre>\n"
                  "<script src=\"" + vizGlobalSrc + "\"></script>\n"
                  "<script src=\"" + vizWrapperSrc + "\"></script>\n"
                  "</body></html>\n";
    }
    else {
        const std::string openTag = "<pre class=\"dot\">";
        auto pos = content.find(openTag);
        if (pos != std::string::npos) {
            auto start = pos + openTag.size();
            auto end = content.find("</pre>", start);
            if (end != std::string::npos) {
                content.replace(start, end - start, "\n" + dot + "\n");
            }
        }

        // Ensure script paths resolve from where the HTML is written.
        auto replaceAll = [](std::string &s, std::string_view from, std::string_view to) {
            size_t at = 0;
            while ((at = s.find(from, at)) != std::string::npos) {
                s.replace(at, from.size(), to);
                at += to.size();
            }
        };

        replaceAll(content, "./js/viz-global.js", vizGlobalSrc);
        replaceAll(content, "./js/viz-wrapper.js", vizWrapperSrc);
    }

    // Ensure output directory exists
    if (outputPath.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(outputPath.parent_path(), ec);
    }

    std::ofstream out(outputPath, std::ios::out | std::ios::binary);
    if (!out) {
        CO_CORE_ERROR("Failed to open output path for writing: {}", outputPath.string());
        return;
    }
    out << content;
}

} // namespace Cory
