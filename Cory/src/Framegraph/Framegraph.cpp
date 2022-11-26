#include <Cory/Framegraph/Framegraph.hpp>

#include <Cory/Framegraph/CommandList.hpp>

#include <Magnum/Vk/CommandBuffer.h>

#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/reverse.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/algorithm/transform.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

#include <deque>
#include <unordered_map>

namespace Vk = Magnum::Vk;

namespace Cory::Framegraph {

Builder Framegraph::Framegraph::declarePass(std::string_view name)
{
    //
    return Builder{*this, name};
}

Framegraph::Framegraph(Context &ctx)
    : ctx_{&ctx}
    , resources_{*ctx_}
{
}

Framegraph::~Framegraph()
{
    // destroy all coroutines
    for (RenderTaskInfo &info : renderPasses_) {
        info.coroHandle.destroy();
    }
}

void Framegraph::execute(Vk::CommandBuffer &cmdBuffer)
{
    std::vector<RenderTaskHandle> passesToExecute = compile();

    CommandList cmd{*ctx_, cmdBuffer};
    commandListInProgress_ = &cmd;
    for (const auto &handle : passesToExecute) {
        executePass(cmd, handle);
    }
}

void Framegraph::executePass(CommandList &cmd, RenderTaskHandle handle)
{
    const RenderTaskInfo &rpInfo = renderPasses_[handle];

    CO_CORE_INFO("Setting up Render pass {}", rpInfo.name);
    // ## handle resource transitions
    // TODO

    CO_CORE_INFO("Executing rendering commands for {}", rpInfo.name);
    auto &coroHandle = rpInfo.coroHandle;
    if (!coroHandle.done()) { coroHandle.resume(); }

    CO_CORE_ASSERT(coroHandle.done(),
                   "Render task coroutine seems to have more unnecessary coroutine synchronization "
                   "points! A render task should only have a single co_yield and should wait on "
                   "the builder's finishPassDeclaration() exactly once!");
}

TextureHandle Framegraph::declareInput(TextureInfo info,
                                       Layout layout,
                                       AccessFlags lastWriteAccess,
                                       PipelineStages lastWriteStage,
                                       Vk::Image &image)
{
    auto handle = resources_.registerExternal(info, layout, lastWriteAccess, lastWriteStage, image);
    externalInputs_.push_back(handle);
    return handle;
}

std::pair<TextureInfo, TextureState> Framegraph::declareOutput(TextureHandle handle)
{
    outputs_.push_back(handle);
    return {resources_.info(handle), resources_.state(handle)};
}

std::vector<RenderTaskHandle> Framegraph::compile()
{
    // TODO this is where we would optimize and figure out the dependencies
    CO_CORE_INFO("Framegraph optimization not implemented.");

    auto [passes, requiredResources] = resolve(outputs_);
    resources_.allocate(requiredResources);
    dump(passes, requiredResources);
    return passes;
}

void Framegraph::dump(const std::vector<RenderTaskHandle> &passes,
                      const std::vector<TextureHandle> &realizedTextures)
{
    std::string out{"digraph G {\n"
                    "rankdir=LR;\n"
                    "node [fontsize=12,fontname=\"Courier New\"]\n"};

    std::unordered_map<TextureHandle, TextureInfo> textures;

    for (const auto &[passHandle, passInfo] : renderPasses_.items()) {
        const std::string passCol = (!passInfo.coroHandle) ? "red"
                                    : ranges::contains(passes, RenderTaskHandle{passHandle})
                                        ? "black"
                                        : "gray";
        fmt::format_to(std::back_inserter(out),
                       "  \"{0}\" [shape=ellipse,color={1},fontcolor={1}]\n",
                       passInfo.name,
                       passCol);

        for (const auto &[inputHandle, _] : passInfo.inputs) {
            auto inputInfo = resources_.info(inputHandle);
            fmt::format_to(
                std::back_inserter(out), "  \"{}\" -> \"{}\" \n", inputInfo.name, passInfo.name);
            textures[inputHandle] = inputInfo;
        }

        for (const auto &[outputHandle, kind, _] : passInfo.outputs) {
            auto outputInfo = resources_.info(outputHandle);
            const std::string color = kind == PassOutputKind::Create ? "darkgreen" : "black";
            const std::string label = kind == PassOutputKind::Create ? "creates" : "";
            fmt::format_to(std::back_inserter(out),
                           "  \"{}\" -> \"{}\" [style=dashed,color={},label=\"{}\"]\n",
                           passInfo.name,
                           outputInfo.name,
                           color,
                           label);
            textures[outputHandle] = outputInfo;
        }
    }

    for (const auto &[handle, info] : textures) {
        auto state = resources_.state(handle);
        const std::string color = ranges::contains(externalInputs_, handle)    ? "blue"
                                  : ranges::contains(realizedTextures, handle) ? "black"
                                                                               : "gray";
        const std::string label =
            fmt::format("{}{}\\n[{}x{}x{} {},{}]",
                        info.name,
                        state.status == TextureMemoryStatus::External ? " (ext)" : "",
                        info.size.x,
                        info.size.y,
                        info.size.z,
                        info.format,
                        state.layout);
        const float penWidth =
            ranges::contains(outputs_, handle) || ranges::contains(externalInputs_, handle) ? 3.0f
                                                                                            : 1.0f;
        fmt::format_to(
            std::back_inserter(out),
            "  \"{0}\" [shape=rectangle,label=\"{1}\",color={2},fontcolor={2},penwidth={3}]\n",
            info.name,
            label,
            color,
            penWidth);
    }

    out += "}\n";
    CO_CORE_INFO(out);
}

std::pair<std::vector<RenderTaskHandle>, std::vector<TextureHandle>>
Framegraph::resolve(const std::vector<TextureHandle> &requestedResources)
{
    // counter to assign render passes an increasing execution priority - passes
    // with higher priority should be executed earlier
    int32_t executionPrio{-1};

    // first, reorder the information into a more convenient graph representation
    // essentially, in- and out-edges
    std::unordered_map<TextureHandle, RenderTaskHandle> resourceToPass;
    std::unordered_multimap<RenderTaskHandle, TextureHandle> passInputs;
    std::unordered_map<TextureHandle, TextureInfo> textures;
    for (const auto &[passHandle, passInfo] : renderPasses_.items()) {
        for (const auto &[inputHandle, _] : passInfo.inputs) {
            passInputs.insert({passHandle, inputHandle});
            textures[inputHandle] = resources_.info(inputHandle);
        }
        for (const auto &[outputHandle, kind, _] : passInfo.outputs) {
            resourceToPass[outputHandle] = passHandle;
            textures[outputHandle] = resources_.info(outputHandle);
        }
    }

    std::vector<TextureHandle> requiredResources; // collects all actually required resources

    // flood-fill the graph starting at the resources requested from the outside
    std::deque<TextureHandle> nextResourcesToResolve{requestedResources.cbegin(),
                                                     requestedResources.cend()};
    while (!nextResourcesToResolve.empty()) {
        auto nextResource = nextResourcesToResolve.front();
        nextResourcesToResolve.pop_front();
        requiredResources.push_back(nextResource);

        // if resource is external, we don't have to resolve it
        if (ranges::contains(externalInputs_, nextResource)) { continue; }

        // determine the pass that writes/creates the resource
        auto writingPassIt = resourceToPass.find(nextResource);
        if (writingPassIt == resourceToPass.end()) {
            CO_CORE_ERROR(
                "Could not resolve frame dependency graph: resource '{}' ({}) is not created "
                "by any render pass",
                textures[nextResource].name,
                nextResource);
            return {};
        }

        RenderTaskHandle writingPass = writingPassIt->second;
        CO_CORE_DEBUG(
            "Resolving resource {}: created/written by render pass {}", nextResource, writingPass);
        renderPasses_[writingPass].executionPriority = ++executionPrio;

        // enqueue the inputs of the pass for resolve
        auto rng = passInputs.equal_range(writingPass);
        ranges::transform(
            rng.first, rng.second, std::back_inserter(nextResourcesToResolve), [&](const auto &it) {
                CO_CORE_DEBUG("Requesting input resource for {}: {}", writingPass, it.second);
                return it.second;
            });
    }

    auto items = renderPasses_.items();
    auto passesToExecute =
        items | ranges::views::transform([](const auto &it) {
            return std::make_pair(RenderTaskHandle{it.first}, it.second.executionPriority);
        }) |
        ranges::views::filter([](const auto &it) { return it.second >= 0; }) |
        ranges::to<std::vector>;

    // sort in descending order so the passes with the highest priority come first
    ranges::sort(passesToExecute, {}, [](const auto &it) { return -it.second; });

    CO_APP_DEBUG("Render pass order after resolve:");
    for (const auto &[handle, prio] : passesToExecute) {
        CO_APP_DEBUG("  [{}] {}", prio, renderPasses_[handle].name);
    }

    auto passes = passesToExecute |
                  ranges::views::transform([](const auto &it) { return it.first; }) |
                  ranges::to<std::vector<RenderTaskHandle>>;

    return {std::move(passes), std::move(requiredResources)};
}

RenderInput Framegraph::renderInput(RenderTaskHandle passHandle)
{
    CO_CORE_ASSERT(commandListInProgress_, "No command list recording in progress!");
    return {
        .resources = nullptr,
        .context = nullptr,
        .cmd = commandListInProgress_,
    };
}

////////////////////////////////////////////////////////////////////////////////////////////////////

RenderInput RenderTaskExecutionAwaiter::await_resume() const noexcept
{
    return fg.renderInput(passHandle);
}

void RenderTaskExecutionAwaiter::await_suspend(
    cppcoro::coroutine_handle<> coroHandle) const noexcept
{
    fg.enqueueRenderPass(passHandle, coroHandle);
}

} // namespace Cory::Framegraph
