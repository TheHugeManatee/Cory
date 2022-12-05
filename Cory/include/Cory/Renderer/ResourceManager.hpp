#pragma once

#include <Cory/Base/SlotMap.hpp>
#include <Cory/Renderer/Common.hpp>

#include <filesystem>
#include <memory>
#include <string_view>

namespace Cory {

enum class ResourceType { Buffer, Shader };

/**
 * Central resource manager that manages all vulkan-related resources.
 *
 * It it exclusively deals in handles.
 * Handle types are declared in RenderCommon.hpp to reduce compile times
 * Currently:
 *  - Shaders
 * Eventually:
 *  - Textures and Buffers
 *  - Pipelines
 *  - RenderPasses
 *  - Descriptors etc.
 */
class ResourceManager : NoCopy {
  public:
    ResourceManager();
    ~ResourceManager();

    ResourceManager(ResourceManager &&) = default;
    ResourceManager &operator=(ResourceManager &&) = default;

    // set up the context to be used - must be called exactly once, before any resources are created
    void setContext(Context &ctx);

    /// @see ShaderSource::ShaderSource(std::filesystem::path, ShaderType)
    [[nodiscard]] ShaderHandle createShader(std::filesystem::path filePath,
                                            ShaderType type = ShaderType::eUnknown);
    /// @see ShaderSource::ShaderSource(std::string, ShaderType, std::filesystem::path)
    [[nodiscard]] ShaderHandle
    createShader(std::string source, ShaderType type, std::filesystem::path filePath = "Unknown");
    /// dereference a shader handle to access the shader. may throw!
    [[nodiscard]] Shader &operator[](ShaderHandle shaderHandle);
    void release(ShaderHandle shaderHandle);

    // buffers
    BufferHandle createBuffer(size_t bufferSizeInBytes, BufferUsage usage, MemoryFlags flags);
    [[nodiscard]] Magnum::Vk::Buffer &operator[](BufferHandle bufferHandle);
    void release(BufferHandle bufferHandle);

    // pipelines
    PipelineHandle createPipeline(std::string_view name,
                                  const Magnum::Vk::RasterizationPipelineCreateInfo &createInfo);
    Magnum::Vk::Pipeline &operator[](PipelineHandle pipelineHandle);
    void release(PipelineHandle pipelineHandle);

    std::unordered_map<ResourceType, size_t> resourcesInUse() const;

  private:
    std::unique_ptr<struct ResourceManagerPrivate> data_;
};
static_assert(std::movable<ResourceManager>);

} // namespace Cory