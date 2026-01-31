#pragma once

#include <Cory/Renderer/Common.hpp>

namespace Cory {

template <typename Handle> class FrameResourceLifetimeHelper {
  public:
    void scheduleForRelease(Handle handle, uint64_t lastUsedFrameIndex);

    std::vector<Handle> collectReleasableResources(uint64_t currentFrameIndex);

  private:
    struct ScheduledResource {
        Handle handle;
        uint64_t lastUsedFrameIndex;
    };

    std::vector<ScheduledResource> scheduledResources_;
};
template <typename Handle>
void FrameResourceLifetimeHelper<Handle>::scheduleForRelease(Handle handle,
                                                             uint64_t lastUsedFrameIndex)
{
    scheduledResources_.push_back({std::move(handle), lastUsedFrameIndex});
}

template <typename Handle>
std::vector<Handle>
FrameResourceLifetimeHelper<Handle>::collectReleasableResources(uint64_t currentFrameIndex)
{
    std::vector<Handle> releasableResources;

    auto it = scheduledResources_.begin();
    while (it != scheduledResources_.end()) {
        // Resources can be released if they have not been used for at least MAX_FRAMES_IN_FLIGHT
        // frames
        if (currentFrameIndex >= it->lastUsedFrameIndex &&
            currentFrameIndex - it->lastUsedFrameIndex >= MAX_FRAMES_IN_FLIGHT) {
            releasableResources.push_back(std::move(it->handle));
            it = scheduledResources_.erase(it);
        }
        else {
            ++it;
        }
    }

    return releasableResources;
}

} // namespace Cory
