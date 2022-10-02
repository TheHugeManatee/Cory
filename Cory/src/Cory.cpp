#include <Cory/Cory.hpp>

#include <Corrade/Containers/StringView.h>
#include <Magnum/Vk/ExtensionProperties.h>
#include <Magnum/Vk/Extensions.h>
#include <Magnum/Vk/LayerProperties.h>
#include <Magnum/Vk/Version.h>

#include <fmt/core.h>

#include <Cory/Core/Context.hpp>
#include <Cory/Core/Log.hpp>

namespace Cory {

void Init() {
    // initialize all static objects in the correct order
    Cory::Log::Init();
}

std::string queryVulkanInstanceVersion()
{
    auto version = Magnum::Vk::enumerateInstanceVersion();
    return fmt::format(
        "{}.{}.{}", versionMajor(version), versionMinor(version), versionPatch(version));
}

void dumpInstanceInformation();

void dumpInstanceInformation()
{
    CO_CORE_INFO("Instance version: {}", queryVulkanInstanceVersion());

    Magnum::Vk::LayerProperties layers = Magnum::Vk::enumerateLayerProperties();
    CO_CORE_INFO("Supported layers  [{}]", layers.count());
    for (const auto name : layers.names()) {
        CO_CORE_INFO("    {:<25}: {}", name.data(), layers.isSupported(name));
    }
    Magnum::Vk::InstanceExtensionProperties extensions =
        /* ... including extensions exposed only by the extra layers */
        Magnum::Vk::enumerateInstanceExtensionProperties(layers.names());
    CO_CORE_INFO("Supported extensions  [{}]", extensions.count());
    for (const auto name : extensions.names()) {
        CO_CORE_INFO("    {:<25}: {}", name.data(), extensions.isSupported(name));
    }
}

void playground_main()
{
    dumpInstanceInformation();

    Cory::Context context;
}

} // namespace Cory
