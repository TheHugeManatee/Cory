#include <Cory/Framegraph/Framegraph.hpp>

#include <Cory/Base/Profiling.hpp>
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

Builder Framegraph::Framegraph::declareTask(std::string_view name)
{
    //
    return Builder{*this, name};
}

Framegraph::Framegraph(Context &ctx)
    : ctx_{&ctx}
    , resources_{*ctx_}
{
}

Framegraph::~Framegraph() { retireImmediate(); }

ExecutionInfo Framegraph::record(Vk::CommandBuffer &cmdBuffer)
{
    const Cory::ScopeTimer s1{"Framegraph/Execute"};
    const auto executionInfo = compile();

    const Cory::ScopeTimer s2{"Framegraph/Execute/Record"};
    CommandList cmd{*ctx_, cmdBuffer};

    commandListInProgress_ = &cmd;
    auto resetCmdList = gsl::finally([this]() { commandListInProgress_ = nullptr; });

    for (const auto &handle : executionInfo.passes) {
        executePass(cmd, handle);
    }
    return executionInfo;
}

void Framegraph::retireImmediate()
{
    resources_.clear();
    externalInputs_.clear();
    outputs_.clear();

    for (RenderTaskInfo &info : renderTasks_) {
        info.coroHandle.destroy();
    }
    renderTasks_.clear();
}

void Framegraph::executePass(CommandList &cmd, RenderTaskHandle handle)
{
    const RenderTaskInfo &rpInfo = renderTasks_[handle];
    const Cory::ScopeTimer s1{fmt::format("Framegraph/Execute/Record/{}", rpInfo.name)};

    CO_CORE_TRACE("Setting up Render pass {}", rpInfo.name);
    // handle input resource transitions
    for (auto input : rpInfo.inputs) {
        resources_.readBarrier(cmd.handle(), input.handle.texture, input.accessInfo);
    }
    for (auto input : rpInfo.outputs | ranges::views::filter([](const auto &desc) {
                          return desc.kind == TaskOutputKind::Create;
                      })) {
        resources_.readBarrier(cmd.handle(), input.handle.texture, input.accessInfo);
    }

    CO_CORE_TRACE("Executing rendering commands for {}", rpInfo.name);
    auto &coroHandle = rpInfo.coroHandle;
    if (!coroHandle.done()) { coroHandle.resume(); }

    // record writes to output resources of this render pass
    for (auto output : rpInfo.outputs) {
        resources_.recordWrite(cmd.handle(), output.handle.texture, output.accessInfo);
    }

    CO_CORE_ASSERT(coroHandle.done(),
                   "Render task coroutine seems to have more unnecessary coroutine synchronization "
                   "points! A render task should only have a single co_yield and should wait on "
                   "the builder's finishPassDeclaration() exactly once!");
}

TransientTextureHandle Framegraph::declareInput(TextureInfo info,
                                                Layout layout,
                                                AccessFlags lastWriteAccess,
                                                PipelineStages lastWriteStage,
                                                Vk::Image &image,
                                                Vk::ImageView &imageView)
{
    auto handle = resources_.registerExternal(
        info, layout, lastWriteAccess, lastWriteStage, image, imageView);

    TransientTextureHandle thandle{.texture = handle, .version = 0};

    externalInputs_.push_back(thandle);
    return thandle;
}

std::pair<TextureInfo, TextureState> Framegraph::declareOutput(TransientTextureHandle handle)
{
    outputs_.push_back(handle);
    return {resources_.info(handle.texture), resources_.state(handle.texture)};
}

ExecutionInfo Framegraph::compile()
{
    const Cory::ScopeTimer s{"Framegraph/Execute/Compile"};

    auto [passes, requiredResources] = resolve(outputs_);
    resources_.allocate(requiredResources);

    return {passes, requiredResources};
}

void Framegraph::dump(const ExecutionInfo &executionInfo)
{
    std::string out{"digraph G {\n"
                    "rankdir=LR;\n"
                    "node [fontsize=12,fontname=\"Courier New\"]\n"
                    "edge [fontsize=10,fontname=\"Courier New\"]\n"};

    std::unordered_map<TransientTextureHandle, TextureInfo> textures;

    for (const auto &[passHandle, passInfo] : renderTasks_.items()) {
        const std::string passCol =
            (!passInfo.coroHandle)                                                 ? "red"
            : ranges::contains(executionInfo.passes, RenderTaskHandle{passHandle}) ? "black"
                                                                                   : "gray";
        fmt::format_to(std::back_inserter(out),
                       "  \"{0}\" [shape=ellipse,color={1},fontcolor={1}]\n",
                       passInfo.name,
                       passCol);

        for (const auto &[inputHandle, _] : passInfo.inputs) {
            auto inputInfo = resources_.info(inputHandle.texture);
            fmt::format_to(std::back_inserter(out),
                           "  \"{} v{}\" -> \"{}\" \n",
                           inputInfo.name,
                           inputHandle.version,
                           passInfo.name);
            textures[inputHandle] = inputInfo;
        }

        for (const auto &[outputHandle, kind, _] : passInfo.outputs) {
            auto outputInfo = resources_.info(outputHandle.texture);
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
        auto state = resources_.state(handle.texture);
        const std::string color = ranges::contains(externalInputs_, handle) ? "blue"
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
        const float penWidth = ranges::contains(outputs_, handle) ? 3.0f : 1.0f;
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
    CO_CORE_INFO(out);
}

ExecutionInfo Framegraph::resolve(const std::vector<TransientTextureHandle> &requestedResources)
{
    // counter to assign render passes an increasing execution priority - passes
    // with higher priority should be executed earlier
    int32_t executionPrio{-1};

    // first, reorder the information into a more convenient graph representation
    // essentially, in- and out-edges
    std::unordered_map<TransientTextureHandle, RenderTaskHandle> resourceToPass;
    std::unordered_multimap<RenderTaskHandle, TransientTextureHandle> passInputs;
    std::unordered_map<TransientTextureHandle, TextureInfo> textures;
    for (const auto &[passHandle, passInfo] : renderTasks_.items()) {
        for (const auto &[inputHandle, _] : passInfo.inputs) {
            passInputs.insert({passHandle, inputHandle});
            textures[inputHandle] = resources_.info(inputHandle.texture);
        }
        for (const auto &[outputHandle, kind, _] : passInfo.outputs) {
            resourceToPass[outputHandle] = passHandle;
            textures[outputHandle] = resources_.info(outputHandle.texture);
        }
    }

    std::vector<TextureHandle> requiredResources; // collects all actually required resources

    // flood-fill the graph starting at the resources requested from the outside
    std::deque<TransientTextureHandle> nextResourcesToResolve{requestedResources.cbegin(),
                                                              requestedResources.cend()};
    while (!nextResourcesToResolve.empty()) {
        auto nextResource = nextResourcesToResolve.front();
        nextResourcesToResolve.pop_front();
        requiredResources.push_back(nextResource.texture);

        // determine the pass that writes/creates the resource
        auto writingPassIt = resourceToPass.find(nextResource);
        if (writingPassIt == resourceToPass.end()) {
            // if resource is external, we don't have to resolve it
            if (ranges::contains(externalInputs_, nextResource)) { continue; }

            CO_CORE_ERROR(
                "Could not resolve frame dependency graph: resource '{} v{}' ({}) is not created "
                "by any render pass",
                textures[nextResource].name,
                nextResource.version,
                nextResource.texture);
            return {};
        }

        RenderTaskHandle writingPass = writingPassIt->second;
        CO_CORE_TRACE("Resolving resource '{} v{}': created/written by render pass '{}'",
                      textures[nextResource].name,
                      nextResource.version,
                      renderTasks_[writingPass].name);
        renderTasks_[writingPass].executionPriority = ++executionPrio;

        // mark the resources created by the task as required
        for (const RenderTaskInfo::OutputDesc &created :
             renderTasks_[writingPass].outputs | ranges::views::filter([](const auto &outputDesc) {
                 return outputDesc.kind == TaskOutputKind::Create;
             })) {
            requiredResources.push_back(created.handle.texture);
        }

        // enqueue the inputs of the task for resolution
        auto rng = passInputs.equal_range(writingPass);
        ranges::transform(
            rng.first, rng.second, std::back_inserter(nextResourcesToResolve), [&](const auto &it) {
                CO_CORE_TRACE("Requesting input resource for {}: '{} v{}'",
                              renderTasks_[writingPass].name,
                              textures[it.second].name,
                              it.second.version);
                return it.second;
            });
    }

    auto items = renderTasks_.items();
    auto passesToExecute =
        items | ranges::views::transform([](const auto &it) {
            return std::make_pair(RenderTaskHandle{it.first}, it.second.executionPriority);
        }) |
        ranges::views::filter([](const auto &it) { return it.second >= 0; }) |
        ranges::to<std::vector>;

    // sort in descending order so the passes with the highest priority come first
    ranges::sort(passesToExecute, {}, [](const auto &it) { return -it.second; });

    CO_CORE_TRACE("Render pass order after resolve:");
    for (const auto &[handle, prio] : passesToExecute) {
        CO_CORE_TRACE("  [{}] {}", prio, renderTasks_[handle].name);
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
