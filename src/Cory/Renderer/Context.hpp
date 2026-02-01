#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Base/Function.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/Gpu.hpp>

#include <KDGpu/instance.h>
#include <KDGpu/surface.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Cory {

struct DebugMessageInfo {
    DebugMessageSeverity severity;
    DebugMessageType messageType;
    int32_t messageIdNumber;
    std::string message;
};

enum class ValidationLayers { Enabled, Disabled };
enum class DeviceFeatures { RequiredOnly, All };
struct ContextCreationInfo {
    ValidationLayers validation{ValidationLayers::Enabled};
    std::span<const char *> args;
};

struct DeviceMemoryReportHeapStats {
    uint32_t heapIndex{0};
    uint64_t currentBytes{0};
    uint64_t totalAllocatedBytes{0};
    uint64_t totalFreedBytes{0};
    uint64_t allocationCount{0};
    uint64_t freeCount{0};
    uint64_t importCount{0};
    uint64_t unimportCount{0};
    uint64_t allocationFailedCount{0};
};

struct DeviceMemoryReportStats {
    bool supported{false};
    uint64_t currentBytes{0};
    uint64_t totalAllocatedBytes{0};
    uint64_t totalFreedBytes{0};
    uint64_t allocationCount{0};
    uint64_t freeCount{0};
    uint64_t importCount{0};
    uint64_t unimportCount{0};
    uint64_t allocationFailedCount{0};
    std::vector<DeviceMemoryReportHeapStats> heaps;
};

/**
 * The main context for cory (collects pretty much everything).
 */
class Context : NoCopy {
  public:
    Context(ContextCreationInfo creationInfo = {});
    ~Context();

    // movable
    Context(Context &&rhs) noexcept;
    Context &operator=(Context &&rhs) noexcept;

    std::string name() const;

    [[nodiscard]] Gpu::GpuSemaphore createSemaphore(std::string_view name = "");
    [[nodiscard]] Gpu::Fence createFence(std::string_view name = "", FenceCreateMode mode = {});

    /// register a callback that gets called on vulkan validation messages etc.
    void onVulkanDebugMessageReceived(Function<void(const DebugMessageInfo &)> callback);

    bool isHeadless() const;

    // Set up the device and queue for a given surface
    void setupDeviceFromSurface(const Gpu::Surface &surface);
    // Set up a headless device, i.e. a device not tied to a specific surface
    void setupHeadlessDevice();
    /// Sets up the descriptor sets
    void setupDescriptors();

    Gpu::Instance &instance();

    Gpu::GraphicsApi &graphicsApi();
    const Gpu::AdapterProperties &physicalDevice();
    Gpu::Device &device();

    Gpu::Queue &graphicsQueue();

    Gpu::VulkanResourceManager &resources();
    const Gpu::VulkanResourceManager &resources() const;

    PipelineCache &pipelineCache();

    ShaderManager &shaders();
    const ShaderManager &shaders() const;

    DescriptorSets &descriptors();
    const DescriptorSets &descriptors() const;

    FileWatchManager &fileWatchManager();
    const FileWatchManager &fileWatchManager() const;

    [[nodiscard]] DeviceMemoryReportStats deviceMemoryReportStats() const;

  private:
    Gpu::AdapterAndDevice
    createDefaultDevice(const Gpu::Surface &surface,
                        DeviceFeatures features = DeviceFeatures::RequiredOnly,
                        Gpu::AdapterDeviceType deviceType = Gpu::AdapterDeviceType::Default) const;

    static Gpu::AdapterFeatures getRequiredFeatures();

    std::unique_ptr<struct ContextPrivate> data_;
};
// static_assert(std::movable<Context>, "Context must be movable");

} // namespace Cory
