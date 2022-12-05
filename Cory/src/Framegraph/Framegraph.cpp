#include <Cory/Framegraph/Framegraph.hpp>

#include "FramegraphVisualizer.h"

#include <Cory/Base/Profiling.hpp>
#include <Cory/Framegraph/CommandList.hpp>
#include <Cory/Renderer/Context.hpp>

#include <Magnum/Vk/CommandBuffer.h>

#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/algorithm/transform.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

#include <deque>
#include <unordered_map>
#include <utility>

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

Framegraph::~Framegraph()
{
    try {
        retireImmediate();
    }
    catch (const std::exception &e) {
        CO_APP_ERROR("Uncaught exception in destructor: {}", e.what());
    }
}

ExecutionInfo Framegraph::record(Vk::CommandBuffer &cmdBuffer)
{
    const Cory::ScopeTimer s1{"Framegraph/Execute"};
    auto executionInfo = compile();

    const Cory::ScopeTimer s2{"Framegraph/Execute/Record"};
    CommandList cmd{*ctx_, cmdBuffer};

    commandListInProgress_ = &cmd;
    auto resetCmdList = gsl::finally([this]() { commandListInProgress_ = nullptr; });

    for (const auto &handle : executionInfo.tasks) {
        auto transitions = executePass(cmd, handle);
        executionInfo.transitions.insert(
            executionInfo.transitions.end(), transitions.begin(), transitions.end());
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

std::vector<ExecutionInfo::TransitionInfo> Framegraph::executePass(CommandList &cmd,
                                                                   RenderTaskHandle handle)
{
    std::vector<ExecutionInfo::TransitionInfo> transitions;
    const RenderTaskInfo &rpInfo = renderTasks_[handle];
    const Cory::ScopeTimer s1{fmt::format("Framegraph/Execute/Record/{}", rpInfo.name)};

    CO_CORE_TRACE("Setting up Render pass {}", rpInfo.name);
    std::vector<Sync::ImageBarrier> imageBarriers;
    auto emitBarrier = [&](const RenderTaskInfo::Dependency &resourceInfo) {
        transitions.push_back(ExecutionInfo::TransitionInfo{
            .direction = ExecutionInfo::TransitionInfo::Direction::ResourceToTask,
            .task = handle,
            .resource = resourceInfo.handle,
            .stateBefore = resources_.state(resourceInfo.handle.texture).lastAccess,
            .stateAfter = resourceInfo.access});

        const auto contentsMode = resourceInfo.kind.is_set(TaskDependencyKindBits::Read)
                                      ? ImageContents::Retain
                                      : ImageContents::Discard;

        imageBarriers.push_back(resources_.synchronizeTexture(
            cmd.handle(), resourceInfo.handle.texture, resourceInfo.access, contentsMode));
    };

    // fill the barriers from the inputs and outputs
    ranges::for_each(rpInfo.dependencies, emitBarrier);

    Sync::CmdPipelineBarrier(ctx_->device(), cmd.handle(), nullptr, {}, imageBarriers);

    CO_CORE_TRACE("Executing rendering commands for {}", rpInfo.name);
    const auto &coroHandle = rpInfo.coroHandle;
    if (!coroHandle.done()) { coroHandle.resume(); }

    CO_CORE_ASSERT(coroHandle.done(),
                   "Render task coroutine seems to have more unnecessary coroutine synchronization "
                   "points! A render task should only have a single co_yield and should wait on "
                   "the builder's finishPassDeclaration() exactly once!");

    return transitions;
}

TransientTextureHandle Framegraph::declareInput(TextureInfo info,
                                                Sync::AccessType lastWriteAccess,
                                                Vk::Image &image,
                                                Vk::ImageView &imageView)
{
    auto handle = resources_.registerExternal(std::move(info), lastWriteAccess, image, imageView);

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

    auto execInfo = resolve(outputs_);
    resources_.allocate(execInfo.resources);

    return std::move(execInfo);
}

std::string Framegraph::dump(const ExecutionInfo &executionInfo)
{
    const FramegraphVisualizer visualizer(*this);
    return visualizer.generateDotGraph(executionInfo);
}

ExecutionInfo Framegraph::resolve(const std::vector<TransientTextureHandle> &requestedResources)
{
    // counter to assign render tasks an increasing execution priority - tasks
    // with higher priority should be executed earlier
    int32_t executionPrio{-1};

    // first, reorder the information into a more convenient graph representation
    // essentially, in- and out-edges
    std::unordered_map<TransientTextureHandle, RenderTaskHandle> resourceToTask;
    std::unordered_multimap<RenderTaskHandle, TransientTextureHandle> taskInputs;
    std::unordered_map<TransientTextureHandle, TextureInfo> textures;
    for (const auto &[taskHandle, taskInfo] : renderTasks_.items()) {
        for (const RenderTaskInfo::Dependency &dependency : taskInfo.dependencies) {
            if (dependency.kind.is_set(TaskDependencyKindBits::Read)) {
                taskInputs.insert({taskHandle, dependency.handle});
            }
            if (dependency.kind.is_set(TaskDependencyKindBits::Write)) {
                resourceToTask[dependency.handle] = taskHandle;
            }
            textures[dependency.handle] = resources_.info(dependency.handle.texture);
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

        // determine the task that writes/creates the resource
        auto writingTaskIt = resourceToTask.find(nextResource);
        if (writingTaskIt == resourceToTask.end()) {
            // if resource is external, we don't have to resolve it
            if (ranges::contains(externalInputs_, nextResource)) { continue; }

            CO_CORE_ERROR(
                "Could not resolve frame dependency graph: resource '{} v{}' ({}) is not created "
                "by any render task",
                textures[nextResource].name,
                nextResource.version,
                nextResource.texture);
            return {};
        }

        const RenderTaskHandle writingTask = writingTaskIt->second;
        CO_CORE_TRACE("Resolving resource '{} v{}': created/written by render task '{}'",
                      textures[nextResource].name,
                      nextResource.version,
                      renderTasks_[writingTask].name);
        renderTasks_[writingTask].executionPriority = ++executionPrio;

        // mark the resources created by the task as required
        for (const RenderTaskInfo::Dependency &created :
             renderTasks_[writingTask].dependencies |
                 ranges::views::filter([](const auto &outputDesc) {
                     return outputDesc.kind.is_set(TaskDependencyKindBits::Create);
                 })) {
            requiredResources.push_back(created.handle.texture);
        }

        // enqueue the inputs of the task for resolution
        auto rng = taskInputs.equal_range(writingTask);
        ranges::transform(
            rng.first, rng.second, std::back_inserter(nextResourcesToResolve), [&](const auto &it) {
                CO_CORE_TRACE("Requesting input resource for {}: '{} v{}'",
                              renderTasks_[writingTask].name,
                              textures[it.second].name,
                              it.second.version);
                return it.second;
            });
    }

    auto items = renderTasks_.items();
    auto tasksToExecute =
        items | ranges::views::transform([](const auto &it) {
            return std::make_pair(RenderTaskHandle{it.first}, it.second.executionPriority);
        }) |
        ranges::views::filter([](const auto &it) { return it.second >= 0; }) |
        ranges::to<std::vector>;

    // sort in descending order so the tasks with the highest priority come first
    ranges::sort(tasksToExecute, {}, [](const auto &it) { return -it.second; });

    CO_CORE_TRACE("Render task order after resolve:");
    for (const auto &[handle, prio] : tasksToExecute) {
        CO_CORE_TRACE("  [{}] {}", prio, renderTasks_[handle].name);
    }

    auto tasks = tasksToExecute |
                 ranges::views::transform([](const auto &it) { return it.first; }) |
                 ranges::to<std::vector<RenderTaskHandle>>;

    return {
        .tasks = std::move(tasks), .resources = std::move(requiredResources), .transitions = {}};
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

cppcoro::generator<std::pair<RenderTaskHandle, const RenderTaskInfo &>>
Framegraph::renderTasks() const
{
    for (const auto &[passHandle, passInfo] : renderTasks_.items()) {
        co_yield std::make_pair(RenderTaskHandle{passHandle}, passInfo);
    }
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
