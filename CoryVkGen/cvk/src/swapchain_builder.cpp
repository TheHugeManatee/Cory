#include <cvk/swapchain_builder.h>

namespace cvk {
std::unique_ptr<swapchain> swapchain_builder::create()
{
    info_.queueFamilyIndexCount = static_cast<uint32_t>(queue_family_indices_.size());
    info_.pQueueFamilyIndices = queue_family_indices_.data();
    return std::make_unique<swapchain>(max_frames_in_flight_, ctx_, *this);
}
}