#include <Cory/Application/Application.hpp>

#include <Cory/Application/ApplicationLayer.hpp>
#include <Cory/Application/LayerStack.hpp>
#include <Cory/Base/FileWatchManager.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Base/Time.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/ThreadScheduler.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <filesystem>

namespace Cory {

struct ApplicationPrivate {
    Context ctx;
    LayerStack layers;

    ApplicationPrivate(ContextCreationInfo info)
        : ctx{info}
        , layers{ctx}
    {
    }
};

Application::Application() {}

void Application::init(const ContextCreationInfo &info)
{
    data_ = std::make_unique<ApplicationPrivate>(info);
}

// <editor-fold desc="Accessors">
Context &Application::ctx()
{
    return data_->ctx;
}
const Context &Application::ctx() const
{
    return data_->ctx;
}
LayerStack &Application::layers()
{
    return data_->layers;
}
const LayerStack &Application::layers() const
{
    return data_->layers;
}
// </editor-fold>

std::vector<Framegraph> Application::createFramegraphs(uint32_t count)
{
    std::vector<Framegraph> framegraphs;
    framegraphs.reserve(count);
    auto &resources = ctx().framegraphResources();
    for (uint32_t idx = 0; idx < count; ++idx) {
        framegraphs.emplace_back(ctx(), resources, idx);
    }
    return framegraphs;
}

void Application::dumpFramegraph(Framegraph &framegraph,
                                 const ExecutionInfo &executionInfo,
                                 std::string_view prefix,
                                 uint64_t frameNumber) const
{
    std::filesystem::path outputPath =
        std::filesystem::current_path() / fmt::format("{}_Frame_{:04}.html", prefix, frameNumber);
    framegraph.dump(executionInfo, outputPath);
    auto fileLink = "file://" + std::filesystem::absolute(outputPath).string();
    std::replace(fileLink.begin(), fileLink.end(), '\\', '/');

    CO_CORE_INFO("Dumped framegraph to\n{}", fileLink);
}

void Application::runMainLoop(FrameSource &frameSource,
                              int64_t framesToRender,
                              const MainLoopOptions &options,
                              const MainLoopCallback &runFrame,
                              const MainLoopCallback &drawUi)
{
    auto time = AppClock::now();
    const auto startTime = time;
    runFrames(frameSource, framesToRender, [&](FrameContext &frameCtx) {
        if (!options.headless) {
            processEvents(0);
            if (options.pollPlatformEvents) {
                pollPlatformEvents();
            }
        }

        if (options.processFileWatchEvents) {
            ctx().fileWatchManager().processPendingEvents();
        }
        // Resume any coroutines that explicitly hopped back to the render thread.
        ctx().renderThreadScheduler().poll();
        if (options.clearDeferredShaderReleases) {
            ctx().shaders().clearDeferredReleases(frameCtx.frameNumber);
        }

        auto previousFrameTime = std::exchange(time, AppClock::now());
        auto delta = time - previousFrameTime;
        auto updateCtx = LogicUpdateContext{
            .simulationTime = std::chrono::duration<double>(time - startTime).count(),
            .deltaTime = delta.count(),
        };

        if (!options.headless && options.updateLayers) {
            layers().update(updateCtx);
        }
        if (!options.headless && drawUi) {
            drawUi(frameCtx, updateCtx);
        }

        runFrame(frameCtx, updateCtx);
    });

    ctx().device().waitUntilIdle();
}

void Application::pollPlatformEvents() const
{
    glfwPollEvents();
}

Application::~Application() = default;

} // namespace Cory
