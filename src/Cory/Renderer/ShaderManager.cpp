#include <Cory/Renderer/ShaderManager.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/Shader.hpp>

#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/view/take.hpp>

namespace Cory {

template <typename T> struct ResourceStorage {
    const std::string name;
    const std::source_location loc;
    T resource;
};

struct ResourceManagerPrivate {
    Context *ctx;
    SlotMap<ResourceStorage<Shader>> shaders;
};

ShaderManager::ShaderManager()
    : data_{std::make_unique<ResourceManagerPrivate>()}
{
}

ShaderManager::~ShaderManager()
{
    auto check_empty = [](std::string_view name, const auto &slotMap) {
        if (!slotMap.empty()) {
            CO_CORE_WARN(
                "ResourceManager: There are still {} {} elements in use!", slotMap.size(), name);

            size_t counter{0};
            for (const auto &storage : slotMap) {
                CO_CORE_WARN("  - {}, allocated here: \n    {}:{}",
                             storage.name,
                             storage.loc.file_name(),
                             storage.loc.line());
                if (++counter == 10) {
                    CO_CORE_WARN("  - {} more...", slotMap.size() - counter);
                    break;
                }
            }
        }
    };

    check_empty("shader", data_->shaders);
}

void ShaderManager::setContext(Context &ctx)
{
    CO_CORE_DEBUG_ASSERT(data_->ctx == nullptr, "Context already initialized!");
    data_->ctx = &ctx;
}
size_t ShaderManager::shadersInUse() const
{
    return data_->shaders.size();
}

ShaderHandle ShaderManager::createShader(std::filesystem::path filePath,
                                         Gpu::ShaderStageFlagBits type,
                                         std::source_location loc)
{
    CO_CORE_DEBUG_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->shaders.emplace(ResourceStorage<Shader>{
        .name = filePath.string(),
        .loc = std::move(loc),
        .resource = {std::ref(*data_->ctx), ShaderSource{std::move(filePath), type}}});
}
ShaderHandle ShaderManager::createShader(std::string source,
                                         Gpu::ShaderStageFlagBits type,
                                         std::filesystem::path filePath,
                                         std::source_location loc)
{
    CO_CORE_DEBUG_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->shaders.emplace(ResourceStorage<Shader>{
        .name = filePath.string(),
        .loc = std::move(loc),
        .resource = {std::ref(*data_->ctx),
                     ShaderSource{std::move(source), type, std::move(filePath)}}});
}
Shader &ShaderManager::operator[](ShaderHandle shaderHandle)
{
    CO_CORE_DEBUG_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    return data_->shaders[shaderHandle].resource;
}
void ShaderManager::release(ShaderHandle shaderHandle)
{
    CO_CORE_DEBUG_ASSERT(data_->ctx != nullptr, "Context was not initialized!");
    data_->shaders.release(shaderHandle);
}

} // namespace Cory
