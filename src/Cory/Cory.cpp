#include <Cory/Cory.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/Base/SimulationClock.hpp>
#include <Cory/Base/Time.hpp>
#include <Cory/Renderer/Context.hpp>

#include <fmt/core.h>

namespace Cory {

void Init()
{
    // initialize all static objects in the correct order
    AppClock::Init();
    SimulationClock::Init();
    Log::Init();
}

void Deinit() {}

std::string queryVulkanInstanceVersion()
{
    return "<Unknown>";
    //     auto version = Magnum::Vk::enumerateInstanceVersion();
    // return fmt::format(
    //     "{}.{}.{}", versionMajor(version), versionMinor(version), versionPatch(version));
}

void dumpInstanceInformation()
{
    // CO_CORE_INFO("Instance version: {}", queryVulkanInstanceVersion());
    //
    // Vk::LayerProperties layers = Vk::enumerateLayerProperties();
    // CO_CORE_INFO("Supported layers  [{}]", layers.count());
    // for (const auto name : layers.names()) {
    //     CO_CORE_INFO("    {:<25}: {}", name.data(), layers.isSupported(name));
    // }
    // Vk::InstanceExtensionProperties extensions =
    //     /* ... including extensions exposed only by the extra layers */
    //     Vk::enumerateInstanceExtensionProperties(layers.names());
    // CO_CORE_INFO("Supported extensions  [{}]", extensions.count());
    // for (const auto name : extensions.names()) {
    //     CO_CORE_INFO("    {:<25}: {}", name.data(), extensions.isSupported(name));
    // }
}

} // namespace Cory
