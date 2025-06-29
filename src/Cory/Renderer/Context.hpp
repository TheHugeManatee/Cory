#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/KDGpuFwd.hpp>

#include <KDGpu/instance.h>
#include <KDGpu/surface.h>

#include <memory>
#include <string>
#include <string_view>

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

    [[nodiscard]] KDGpu::GpuSemaphore createSemaphore(std::string_view name = "");
    [[nodiscard]] KDGpu::Fence createFence(std::string_view name = "", FenceCreateMode mode = {});

    bool isHeadless() const;

    KDGpu::Surface createSurface(std::string_view name, KDGpuKDGui::View &view);
    KDGpu::Instance &instance();

    KDGpu::GraphicsApi &graphicsApi();
    const KDGpu::AdapterProperties &physicalDevice();
    KDGpu::Device &device();
    // DescriptorSets &descriptorSets();

    KDGpu::Queue &graphicsQueue();

    KDGpu::VulkanResourceManager &resources();
    const KDGpu::VulkanResourceManager &resources() const;

  private:
    KDGpu::AdapterAndDevice createDefaultDevice(
        const KDGpu::Surface &surface,
        DeviceFeatures features = DeviceFeatures::RequiredOnly,
        KDGpu::AdapterDeviceType deviceType = KDGpu::AdapterDeviceType::Default) const;

    KDGpu::AdapterFeatures getRequiredFeatures() const;

    std::unique_ptr<struct ContextPrivate> data_;
};
// static_assert(std::movable<Context>, "Context must be movable");

} // namespace Cory