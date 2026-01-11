#include <Cory/Framegraph/Framegraph.hpp>

#include "FramegraphVisualizer.h"

#include <Cory/Base/Profiling.hpp>
#include <Cory/Framegraph/FramegraphResourceManager.hpp>
#include <Cory/Framegraph/RenderTaskBuilder.hpp>
#include <Cory/Framegraph/ShaderBindingContext.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/MappedCoherentDeviceBuffer.hpp>

#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/algorithm/transform.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

#include <KDGpu/texture.h>
#include <KDGpu/texture_view.h>
#include <KDGpu/vulkan/vulkan_resource_manager.h>

#include <algorithm>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace Cory {

struct FramegraphPrivate {
    FramegraphPrivate(Context &ctx_param, uint32_t instanceIndex)
        : ctx{&ctx_param}
        , resources{ctx_param}
        , shaderBindingContext{
              ctx->device(), resources, ctx->descriptors(), instanceIndex, 200 * 1024 * 1024}
    {
    }

    Context *ctx;
    FramegraphResourceManager resources;
    ShaderBindingContext shaderBindingContext;
    std::vector<TransientTextureHandle> externalInputs;
    std::vector<TransientTextureHandle> outputs;

    SlotMap<RenderTaskInfo> renderTasks;
    CommandRecorder *commandListInProgress{};
    FrameContext *currentFrameCtx{};

    std::unordered_map<TransientTextureHandle, Sync::AccessType> outputFinalAccesses;
};

RenderTaskBuilder Framegraph::declareTask(std::string_view name)
{
    //
    return RenderTaskBuilder{*data_->ctx, *this, name};
}

Framegraph::Framegraph(Context &ctx, uint32_t instanceIndex)
    : data_{std::make_unique<FramegraphPrivate>(ctx, instanceIndex)}
{
}

Framegraph::~Framegraph()
{
    // if data_ is empty, object is moved-from
    if (data_) {
        try {
            resetForNextFrame();
        }
        catch (const std::exception &e) {
            CO_APP_ERROR("Uncaught exception in destructor: {}", e.what());
        }
    }
}

Framegraph::Framegraph(Framegraph &&) noexcept = default;
Framegraph &Framegraph::operator=(Framegraph &&) noexcept = default;

void Framegraph::finalizeOutputs(ExecutionInfo executionInfo)
{
    // After all passes, ensure outputs are transitioned to their requested final access
    std::vector<Sync::ImageBarrier> outputBarriers;
    for (const auto &output : data_->outputs) {
        auto it = data_->outputFinalAccesses.find(output);
        if (it == data_->outputFinalAccesses.end()) continue;
        Sync::AccessType requestedAccess = it->second;
        auto currentState = data_->resources.state(output);
        Sync::AccessType lastAccess = currentState.lastAccess;
        // Only add a barrier if the access actually changes
        if (lastAccess != requestedAccess) {
            auto barrier =
                data_->resources.synchronizeTexture(output, requestedAccess, ImageContents::Retain);
            outputBarriers.push_back(barrier);
            // Record the transition in the execution info
            executionInfo.transitions.push_back(
                ExecutionInfo::TransitionInfo{.kind = TaskDependencyKindBits::Write,
                                              .task = {}, // not associated with a specific task
                                              .resource = output,
                                              .stateBefore = lastAccess,
                                              .stateAfter = requestedAccess});
        }
    }
    if (!outputBarriers.empty()) {
        const auto &rsrc = data_->ctx->resources();
        auto device = rsrc.getDevice(data_->ctx->device());
        auto commandBuffer = rsrc.getCommandRecorder(*data_->commandListInProgress);
        Sync::CmdPipelineBarrier(
            *device, commandBuffer->commandBuffer, nullptr, {}, outputBarriers);
    }
}

ExecutionInfo Framegraph::record(FrameContext &frameCtx)
{
    const Cory::ScopeTimer s1{"Framegraph/Execute"};
    auto executionInfo = compile();

    const Cory::ScopeTimer s2{"Framegraph/Execute/Record"};

    data_->commandListInProgress = &frameCtx.commandBuffer;
    data_->currentFrameCtx = &frameCtx;

    auto resetCmdList = gsl::finally([this]() { data_->commandListInProgress = nullptr; });

    for (const auto &handle : executionInfo.tasks) {
        auto transitions = executePass(*data_->commandListInProgress, handle);
        executionInfo.transitions.insert(executionInfo.transitions.end(),
                                         transitions.imageTransitions.begin(),
                                         transitions.imageTransitions.end());
        executionInfo.bufferTransitions.insert(executionInfo.bufferTransitions.end(),
                                               transitions.bufferTransitions.begin(),
                                               transitions.bufferTransitions.end());
    }

    finalizeOutputs(executionInfo);
    return executionInfo;
}

void Framegraph::resetForNextFrame()
{
    data_->shaderBindingContext.reset();
    data_->resources.clear();
    data_->externalInputs.clear();
    data_->outputs.clear();

    for (RenderTaskInfo &info : data_->renderTasks) { // NOLINT (false positive)
        info.coroHandle.destroy();
    }
    data_->renderTasks.clear();
}

Framegraph::PassTransitions Framegraph::executePass(CommandRecorder &cmd, RenderTaskHandle handle)
{
    PassTransitions transitions;
    const RenderTaskInfo &rpInfo = data_->renderTasks[handle];
    const ScopeTimer s1{fmt::format("Framegraph/Execute/Record/{}", rpInfo.name)};

    CO_CORE_TRACE("Setting up Render pass {}", rpInfo.name);
    {
        auto emitBarrier = [&](const RenderTaskInfo::TextureDependency &resourceInfo) {
            transitions.imageTransitions.push_back(ExecutionInfo::TransitionInfo{
                .kind = resourceInfo.kind,
                .task = handle,
                .resource = resourceInfo.handle,
                .stateBefore = data_->resources.state(resourceInfo.handle).lastAccess,
                .stateAfter = resourceInfo.access});

            // only discard if it is not a read/write dependency
            const auto contentsMode = resourceInfo.kind.is_set(TaskDependencyKindBits::Read)
                                          ? ImageContents::Retain
                                          : ImageContents::Discard;

            return data_->resources.synchronizeTexture(
                resourceInfo.handle, resourceInfo.access, contentsMode);
        };

        auto emitBufferBarrier = [&](const RenderTaskInfo::BufferDependency &resourceInfo) {
            transitions.bufferTransitions.push_back(ExecutionInfo::BufferTransitionInfo{
                .kind = resourceInfo.kind,
                .task = handle,
                .resource = resourceInfo.handle,
                .stateBefore = data_->resources.state(resourceInfo.handle).lastAccess,
                .stateAfter = resourceInfo.access});

            return data_->resources.synchronizeBuffer(resourceInfo.handle, resourceInfo.access);
        };

        // fill the barriers from the inputs and outputs
        const std::vector<Sync::ImageBarrier> imageBarriers =
            rpInfo.textureDependencies | ranges::views::transform(emitBarrier) |
            ranges::to<std::vector>;
        const std::vector<Sync::BufferBarrier> bufferBarriers =
            rpInfo.bufferDependencies | ranges::views::transform(emitBufferBarrier) |
            ranges::to<std::vector>;

        const auto &rsrc = data_->ctx->resources();
        auto device = rsrc.getDevice(data_->ctx->device());
        auto commandBuffer = rsrc.getCommandRecorder(cmd);
        Sync::CmdPipelineBarrier(
            *device, commandBuffer->commandBuffer, nullptr, bufferBarriers, imageBarriers);
    }

    CO_CORE_TRACE("Recording rendering commands for {}", rpInfo.name);
    {
        const auto &coroHandle = rpInfo.coroHandle;
        if (!coroHandle.done()) {
            cmd.beginDebugLabel(Gpu::DebugLabelOptions{
                .label = "Render Task " + rpInfo.name,
                .color = {0.0f, 0.5f, 1.0f, 1.0f},
            });
            coroHandle.resume();
            cmd.endDebugLabel();
        }
    }

    CO_CORE_ASSERT(rpInfo.coroHandle.done(),
                   "Render task coroutine seems to have more unnecessary coroutine synchronization "
                   "points! A render task should only wait on the builder's finishDeclaration() "
                   "exactly once!");

    return transitions;
}

Framegraph::FrameContextHandles Framegraph::importFrameContext(const FrameContext &frameCtx)
{
    auto size = glm::u32vec3{frameCtx.extent, 1};
    FrameContextHandles handles;

    handles.colorImage = declareInput(
        {
            .name = "TEX_SwapCh_Color",
            .size = size,
            .format = frameCtx.colorFormat,
            .sampleCount = frameCtx.sampleCount,
        },
        Sync::AccessType::None,
        *frameCtx.colorImage,
        *frameCtx.colorImageView);

    handles.depthImage = declareInput(
        {
            .name = "TEX_SwapCh_Depth",
            .size = size,
            .format = frameCtx.depthFormat,
            .sampleCount = frameCtx.sampleCount,
        },
        Cory::Sync::AccessType::None,
        *frameCtx.depthImage,
        *frameCtx.depthImageView);

    handles.swapchainImage = declareInput(
        {
            .name = "TEX_SwapCh_Present",
            .size = size,
            .format = frameCtx.colorFormat,
            .sampleCount = frameCtx.sampleCount,
        },
        Cory::Sync::AccessType::None,
        *frameCtx.swapchainImage,
        *frameCtx.swapchainImageView);

    return handles;
}

TransientTextureHandle Framegraph::declareInput(TextureInfo info,
                                                Sync::AccessType lastWriteAccess,
                                                const Texture &image,
                                                const TextureView &imageView)
{
    auto handle =
        data_->resources.registerExternal(std::move(info), lastWriteAccess, image, imageView);

    TransientTextureHandle thandle{handle};

    data_->externalInputs.push_back(thandle);
    return thandle;
}

std::pair<TextureInfo, TextureState> Framegraph::declareOutput(TransientTextureHandle handle,
                                                               Sync::AccessType finalAccess)
{
    data_->outputs.push_back(handle);
    data_->outputFinalAccesses[handle] = finalAccess;
    return {data_->resources.info(handle), data_->resources.state(handle)};
}

ExecutionInfo Framegraph::compile()
{
    const Cory::ScopeTimer s{"Framegraph/Execute/Compile"};

    auto execInfo = resolve(data_->outputs);
    data_->resources.allocate(execInfo.resources);
    data_->resources.allocate(execInfo.buffers);

    return std::move(execInfo);
}

std::string Framegraph::dump(const ExecutionInfo &executionInfo)
{
    const FramegraphVisualizer visualizer(*this);
    return visualizer.generateDotGraph(executionInfo);
}

RenderTaskHandle Framegraph::finishTaskDeclaration(RenderTaskInfo &&info)
{
    return data_->renderTasks.emplace(info);
}

/// to be called from RenderTaskExecutionAwaiter - the Framegraph takes ownership of the @a
/// coroHandle
void Framegraph::enqueueRenderPass(RenderTaskHandle passHandle,
                                   cppcoro::coroutine_handle<> coroHandle)
{
    data_->renderTasks[passHandle].coroHandle = coroHandle;
}

FramegraphResourceManager &Framegraph::resources()
{
    return data_->resources;
}
const FramegraphResourceManager &Framegraph::resources() const
{
    return data_->resources;
}
const std::vector<TransientTextureHandle> &Framegraph::externalInputs() const
{
    return data_->externalInputs;
}
const std::vector<TransientTextureHandle> &Framegraph::outputs() const
{
    return data_->outputs;
}

ExecutionInfo Framegraph::resolve(const std::vector<TransientTextureHandle> &requestedResources)
{
    // first, reorder the information into a more convenient graph representation
    // essentially, in- and out-edges
    std::unordered_map<TransientTextureHandle, RenderTaskHandle> textureToTask;
    std::unordered_multimap<RenderTaskHandle, TransientTextureHandle> taskTextureInputs;
    std::unordered_map<TransientTextureHandle, TextureInfo> textures;
    std::unordered_map<TransientBufferHandle, RenderTaskHandle> bufferToTask;
    std::unordered_multimap<RenderTaskHandle, TransientBufferHandle> taskBufferInputs;
    std::unordered_map<TransientBufferHandle, BufferInfo> buffers;
    std::unordered_set<TransientTextureHandle> resolvedTextures;
    std::unordered_set<TransientBufferHandle> resolvedBuffers;
    for (const auto &[taskHandle, taskInfo] : data_->renderTasks.items()) {
        for (const RenderTaskInfo::TextureDependency &dependency : taskInfo.textureDependencies) {
            const auto kind = dependency.kind;
            // only counts as input if it is a 'pure' read dependency, not read/write
            if (kind.is_set(TaskDependencyKindBits::Read) &&
                !kind.is_set(TaskDependencyKindBits::Write)) {
                taskTextureInputs.insert({taskHandle, dependency.handle});
            }
            if (kind.is_set(TaskDependencyKindBits::Write)) {
                textureToTask[dependency.handle] = taskHandle;
            }
            textures[dependency.handle] = data_->resources.info(dependency.handle);
        }
        for (const RenderTaskInfo::BufferDependency &dependency : taskInfo.bufferDependencies) {
            const auto kind = dependency.kind;
            if (kind.is_set(TaskDependencyKindBits::Read) &&
                !kind.is_set(TaskDependencyKindBits::Write)) {
                taskBufferInputs.insert({taskHandle, dependency.handle});
            }
            if (kind.is_set(TaskDependencyKindBits::Write)) {
                bufferToTask[dependency.handle] = taskHandle;
            }
            buffers[dependency.handle] = data_->resources.info(dependency.handle);
        }
    }

    std::vector<FramegraphTextureHandle>
        requiredResources; // collects all actually required texture resources
    std::vector<FramegraphBufferHandle>
        requiredBuffers; // collects all actually required buffer resources
    std::unordered_set<FramegraphTextureHandle> requiredResourceSet;
    std::unordered_set<FramegraphBufferHandle> requiredBufferSet;
    std::unordered_set<RenderTaskHandle> requiredTasks;

    auto appendCreatedResources = [&](const RenderTaskInfo &taskInfo) {
        for (const RenderTaskInfo::TextureDependency &created :
             taskInfo.textureDependencies | ranges::views::filter([](const auto &outputDesc) {
                 return outputDesc.kind.is_set(TaskDependencyKindBits::Create);
             })) {
            if (requiredResourceSet.insert(created.handle).second) {
                requiredResources.push_back(created.handle);
            }
        }
        for (const RenderTaskInfo::BufferDependency &created :
             taskInfo.bufferDependencies | ranges::views::filter([](const auto &outputDesc) {
                 return outputDesc.kind.is_set(TaskDependencyKindBits::Create);
             })) {
            if (requiredBufferSet.insert(created.handle).second) {
                requiredBuffers.push_back(created.handle);
            }
        }
    };

    auto recordTextureHandle = [&](TransientTextureHandle handle) {
        if (!resolvedTextures.insert(handle).second) return false;
        if (requiredResourceSet.insert(handle).second) {
            requiredResources.push_back(handle);
        }
        return true;
    };

    auto recordBufferHandle = [&](TransientBufferHandle handle) {
        if (!resolvedBuffers.insert(handle).second) return false;
        if (requiredBufferSet.insert(handle).second) {
            requiredBuffers.push_back(handle);
        }
        return true;
    };

    // flood-fill the graph starting at the resources requested from the outside
    std::deque<TransientTextureHandle> nextTexturesToResolve{requestedResources.cbegin(),
                                                             requestedResources.cend()};
    std::deque<TransientBufferHandle> nextBuffersToResolve;
    while (!nextTexturesToResolve.empty() || !nextBuffersToResolve.empty()) {
        while (!nextTexturesToResolve.empty()) {
            auto nextResource = nextTexturesToResolve.front();
            nextTexturesToResolve.pop_front();
            if (!recordTextureHandle(nextResource)) continue;

            auto writingTaskIt = textureToTask.find(nextResource);
            if (writingTaskIt == textureToTask.end()) {
                if (ranges::contains(data_->externalInputs, nextResource)) {
                    continue;
                }

                CO_CORE_ERROR(
                    "Could not resolve frame dependency graph: resource '{} v{}' ({}) is not "
                    "created by any render task",
                    textures[nextResource].name,
                    nextResource.version(),
                    nextResource.texture());
                return {};
            }

            const RenderTaskHandle writingTask = writingTaskIt->second;
            CO_CORE_TRACE("Resolving resource '{} v{}': created/written by render task '{}'",
                          textures[nextResource].name,
                          nextResource.version(),
                          data_->renderTasks[writingTask].name);
            if (requiredTasks.insert(writingTask).second) {
                appendCreatedResources(data_->renderTasks[writingTask]);
            }

            auto texInputs = taskTextureInputs.equal_range(writingTask);
            ranges::transform(texInputs.first,
                              texInputs.second,
                              std::back_inserter(nextTexturesToResolve),
                              [&](const auto &it) {
                                  CO_CORE_TRACE("Requesting input texture for {}: '{} v{}'",
                                                data_->renderTasks[writingTask].name,
                                                textures[it.second].name,
                                                it.second.version());
                                  return it.second;
                              });

            auto bufInputs = taskBufferInputs.equal_range(writingTask);
            ranges::transform(bufInputs.first,
                              bufInputs.second,
                              std::back_inserter(nextBuffersToResolve),
                              [&](const auto &it) {
                                  CO_CORE_TRACE("Requesting input buffer for {}: '{} v{}'",
                                                data_->renderTasks[writingTask].name,
                                                buffers[it.second].name,
                                                it.second.version());
                                  return it.second;
                              });
        }

        while (!nextBuffersToResolve.empty()) {
            auto nextBuffer = nextBuffersToResolve.front();
            nextBuffersToResolve.pop_front();
            if (!recordBufferHandle(nextBuffer)) continue;

            auto writingTaskIt = bufferToTask.find(nextBuffer);
            if (writingTaskIt == bufferToTask.end()) {
                CO_CORE_ERROR(
                    "Could not resolve frame dependency graph: buffer '{} v{}' ({}) is not "
                    "created by any render task",
                    buffers[nextBuffer].name,
                    nextBuffer.version(),
                    nextBuffer.buffer());
                return {};
            }

            const RenderTaskHandle writingTask = writingTaskIt->second;
            CO_CORE_TRACE("Resolving buffer '{} v{}': created/written by render task '{}'",
                          buffers[nextBuffer].name,
                          nextBuffer.version(),
                          data_->renderTasks[writingTask].name);
            if (requiredTasks.insert(writingTask).second) {
                appendCreatedResources(data_->renderTasks[writingTask]);
            }

            auto bufInputs = taskBufferInputs.equal_range(writingTask);
            ranges::transform(bufInputs.first,
                              bufInputs.second,
                              std::back_inserter(nextBuffersToResolve),
                              [&](const auto &it) {
                                  CO_CORE_TRACE("Requesting input buffer for {}: '{} v{}'",
                                                data_->renderTasks[writingTask].name,
                                                buffers[it.second].name,
                                                it.second.version());
                                  return it.second;
                              });
        }
    }

    std::unordered_map<RenderTaskHandle, std::unordered_set<RenderTaskHandle>> adjacency;
    std::unordered_map<RenderTaskHandle, size_t> indegree;
    for (const auto &task : requiredTasks) {
        indegree.emplace(task, 0u);
    }

    auto addEdge = [&](RenderTaskHandle from, RenderTaskHandle to) {
        if (from == to) return;
        if (!requiredTasks.contains(from) || !requiredTasks.contains(to)) return;
        if (adjacency[from].insert(to).second) {
            indegree[to] += 1;
        }
    };

    for (const auto &task : requiredTasks) {
        auto texInputs = taskTextureInputs.equal_range(task);
        for (auto it = texInputs.first; it != texInputs.second; ++it) {
            if (auto writer = textureToTask.find(it->second); writer != textureToTask.end()) {
                addEdge(writer->second, task);
            }
        }

        auto bufInputs = taskBufferInputs.equal_range(task);
        for (auto it = bufInputs.first; it != bufInputs.second; ++it) {
            if (auto writer = bufferToTask.find(it->second); writer != bufferToTask.end()) {
                addEdge(writer->second, task);
            }
        }
    }

    std::vector<RenderTaskHandle> ready;
    ready.reserve(requiredTasks.size());
    for (const auto &[task, degree] : indegree) {
        if (degree == 0) {
            ready.push_back(task);
        }
    }
    auto sortReady = [&]() {
        std::sort(ready.begin(), ready.end(), [&](RenderTaskHandle a, RenderTaskHandle b) {
            return data_->renderTasks[a].name < data_->renderTasks[b].name;
        });
    };
    sortReady();

    std::vector<RenderTaskHandle> tasks;
    tasks.reserve(requiredTasks.size());
    while (!ready.empty()) {
        auto next = ready.front();
        ready.erase(ready.begin());
        tasks.push_back(next);
        auto it = adjacency.find(next);
        if (it == adjacency.end()) {
            continue;
        }
        for (const auto &dest : it->second) {
            auto &deg = indegree[dest];
            if (deg > 0) {
                deg -= 1;
            }
            if (deg == 0) {
                ready.push_back(dest);
            }
        }
        sortReady();
    }

    if (tasks.size() != requiredTasks.size()) {
        CO_CORE_ERROR("Could not resolve frame dependency graph: cyclic task dependency detected");

        std::unordered_set<RenderTaskHandle> unresolvedTasks;
        unresolvedTasks.reserve(requiredTasks.size());
        for (const auto &task : requiredTasks) {
            unresolvedTasks.insert(task);
        }
        for (const auto &task : tasks) {
            unresolvedTasks.erase(task);
        }

        std::unordered_map<RenderTaskHandle, std::vector<RenderTaskHandle>> reverseAdjacency;
        reverseAdjacency.reserve(adjacency.size());
        for (const auto &[from, targets] : adjacency) {
            for (const auto &to : targets) {
                reverseAdjacency[to].push_back(from);
            }
        }

        for (const auto &task : unresolvedTasks) {
            CO_CORE_ERROR("Unresolved task '{}' (indegree {})",
                          data_->renderTasks[task].name,
                          indegree[task]);
            for (const auto &pred : reverseAdjacency[task]) {
                CO_CORE_ERROR("  blocked by '{}'", data_->renderTasks[pred].name);
            }

            auto texInputs = taskTextureInputs.equal_range(task);
            for (auto it = texInputs.first; it != texInputs.second; ++it) {
                const auto handle = it->second;
                if (auto writer = textureToTask.find(handle); writer != textureToTask.end()) {
                    CO_CORE_ERROR("  texture input '{}' v{} written by '{}'",
                                  textures[handle].name,
                                  handle.version(),
                                  data_->renderTasks[writer->second].name);
                }
            }

            auto bufInputs = taskBufferInputs.equal_range(task);
            for (auto it = bufInputs.first; it != bufInputs.second; ++it) {
                const auto handle = it->second;
                if (auto writer = bufferToTask.find(handle); writer != bufferToTask.end()) {
                    CO_CORE_ERROR("  buffer input '{}' v{} written by '{}'",
                                  buffers[handle].name,
                                  handle.version(),
                                  data_->renderTasks[writer->second].name);
                }
            }
        }

        throw std::runtime_error("Cyclic task dependency detected in framegraph");
    }

    CO_CORE_TRACE("Render task order after resolve:");
    for (size_t i = 0; i < tasks.size(); ++i) {
        CO_CORE_TRACE("  [{}] {}", tasks.size() - i - 1, data_->renderTasks[tasks[i]].name);
    }

    return {.tasks = std::move(tasks),
            .resources = std::move(requiredResources),
            .buffers = std::move(requiredBuffers),
            .transitions = {},
            .bufferTransitions = {}};
}

RenderInput Framegraph::renderInput(RenderTaskHandle taskHandle)
{
    CO_CORE_ASSERT(data_->commandListInProgress, "No command list recording in progress!");
    return {
        .ctx = data_->ctx,
        .frameCtx = data_->currentFrameCtx,
        .resources = &data_->resources,
        .bindingContext = &data_->shaderBindingContext,
        .cmd = data_->commandListInProgress,
    };
}

cppcoro::generator<std::pair<RenderTaskHandle, const RenderTaskInfo &>>
Framegraph::renderTasks() const
{
    for (const auto &[passHandle, passInfo] : data_->renderTasks.items()) {
        co_yield std::make_pair(RenderTaskHandle{passHandle}, passInfo);
    }
}

} // namespace Cory
