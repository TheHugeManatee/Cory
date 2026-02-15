#pragma once

#include <Cory/Application/Common.hpp>
#include <Cory/Base/Common.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/FrameSource.hpp>

#include <KDGui/gui_application.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace Cory {

struct LogicUpdateContext;

class Application : NoCopy, NoMove, public KDGui::GuiApplication {
  public:
    Application();
    ~Application() override;

    virtual void run() = 0;

    void init(const ContextCreationInfo &info);

    Context &ctx();
    const Context &ctx() const;

    LayerStack &layers();
    const LayerStack &layers() const;

  protected:
    struct MainLoopOptions {
        bool headless{false};
        bool pollPlatformEvents{false};
        bool processFileWatchEvents{false};
        bool clearDeferredShaderReleases{false};
        bool updateLayers{true};
    };
    using MainLoopCallback = std::function<void(FrameContext &, const LogicUpdateContext &)>;

    struct FramegraphRecordResult {
        Framegraph &framegraph;
        ExecutionInfo executionInfo;
    };

    [[nodiscard]] std::vector<Framegraph> createFramegraphs(uint32_t count = MAX_FRAMES_IN_FLIGHT);

    template <typename FrameFunc>
    void runFrames(FrameSource &frameSource, int64_t framesToRender, FrameFunc &&runFrame)
    {
        auto frames = frameSource.frames();
        for (auto &frameCtx : frames) {
            std::invoke(std::forward<FrameFunc>(runFrame), frameCtx);
            if (framesToRender > 0 &&
                frameCtx.frameNumber >= static_cast<uint64_t>(framesToRender)) {
                break;
            }
        }
    }

    template <typename DeclarePassesFunc>
    [[nodiscard]] FramegraphRecordResult recordFramegraph(std::span<Framegraph> framegraphs,
                                                          FrameContext &frameCtx,
                                                          DeclarePassesFunc &&declarePasses)
    {
        auto &framegraph = framegraphs[frameCtx.inFlightIndex];
        // Retire resources from the last use of this frame slot.
        framegraph.resetForNextFrame(frameCtx.frameNumber);
        std::invoke(std::forward<DeclarePassesFunc>(declarePasses), framegraph, frameCtx);
        return FramegraphRecordResult{framegraph, framegraph.record(frameCtx)};
    }

    void dumpFramegraph(Framegraph &framegraph,
                        const ExecutionInfo &executionInfo,
                        std::string_view prefix,
                        uint64_t frameNumber) const;

    void runMainLoop(FrameSource &frameSource,
                     int64_t framesToRender,
                     const MainLoopOptions &options,
                     const MainLoopCallback &runFrame,
                     const MainLoopCallback &drawUi = {});

  private:
    void pollPlatformEvents() const;

    std::unique_ptr<struct ApplicationPrivate> data_;
};

} // namespace Cory
