#pragma once

#include <Cory/Base/SlotMap.hpp>
#include <Cory/Renderer/Common.hpp>

#include <filesystem>
#include <memory>
#include <string_view>

namespace Cory {

enum class ResourceType { Buffer, Shader, Pipeline, Sampler };

/**
 * Central resource manager that manages all (low-level) vulkan-related resources.
 *
 * At the public interface, it exclusively deals provides and uses handles (essentially,
 * type-safe SlotMap indices).
 *
 * The available Handle types are declared in RenderCommon.hpp to reduce compile times.
 *
 * Currently, manages:
 *  - Buffers
 *  - Shaders
 *  - Pipelines
 *  - Samplers
 *  - descriptor layouts
 *
 * Eventually also:
 *  - Textures and Buffers?
 *  - RenderPasses?
 *  - Descriptors? etc
 */
class ResourceManager : NoCopy {
  public:
    ResourceManager();
    ~ResourceManager();

    ResourceManager(ResourceManager &&) = default;
    ResourceManager &operator=(ResourceManager &&) = default;

    // set up the context to be used - must be called exactly once, before any resources are created
    void setContext(Context &ctx);

    /// query the number of resources in use
    std::unordered_map<ResourceType, size_t> resourcesInUse() const;

    // <editor-fold desc="Shaders">
    /**
     * @name Shaders
     */
    ///@{
    /// @see ShaderSource::ShaderSource(std::filesystem::path, ShaderType)
    [[nodiscard]] ShaderHandle createShader(std::filesystem::path filePath,
                                            ShaderType type = ShaderType::eUnknown);
    /// @see ShaderSource::ShaderSource(std::string, ShaderType, std::filesystem::path)
    [[nodiscard]] ShaderHandle
    createShader(std::string source, ShaderType type, std::filesystem::path filePath = "Unknown");
    /// dereference a shader handle to access the shader. may throw!
    [[nodiscard]] Shader &operator[](ShaderHandle shaderHandle);
    void release(ShaderHandle shaderHandle);
    ///@}
    // </editor-fold>

    // <editor-fold desc="Buffers">
    /**
     * @name Buffers
     */
    ///@{
    BufferHandle createBuffer(size_t bufferSizeInBytes, BufferUsage usage, MemoryFlags flags);
    [[nodiscard]] Magnum::Vk::Buffer &operator[](BufferHandle handle);
    void release(BufferHandle handle);
    ///@}
    // </editor-fold>

    // <editor-fold desc="Pipelines">
    /**
     * @name Pipelines
     */
    ///@{
    PipelineHandle createPipeline(std::string_view name,
                                  const Magnum::Vk::RasterizationPipelineCreateInfo &createInfo);
    Magnum::Vk::Pipeline &operator[](PipelineHandle handle);
    void release(PipelineHandle handle);
    ///@}
    // </editor-fold>

    // <editor-fold desc="Images">
    /**
     * @name Images
     */
    ///@{
    ImageHandle createImage(std::string_view name,
                            const Magnum::Vk::ImageCreateInfo &createInfo,
                            Magnum::Vk::MemoryFlags memoryFlags);
    ImageHandle wrapImage(std::string_view name, Magnum::Vk::Image &resource);
    Magnum::Vk::Image &operator[](ImageHandle handle);
    void release(ImageHandle handle);
    ///@}
    // </editor-fold>

    // <editor-fold desc="ImageViews">
    /**
     * @name ImageViews
     */
    ///@{
    ImageViewHandle createImageView(std::string_view name,
                                    const Magnum::Vk::ImageViewCreateInfo &createInfo);
    ImageViewHandle wrapImageView(std::string_view name, Magnum::Vk::ImageView &resource);
    Magnum::Vk::ImageView &operator[](ImageViewHandle handle);
    void release(ImageViewHandle handle);
    ///@}
    // </editor-fold>

    // <editor-fold desc="Samplers">
    /**
     * @name Samplers
     */
    ///@{
    SamplerHandle createSampler(std::string_view name,
                                const Magnum::Vk::SamplerCreateInfo &createInfo);
    Magnum::Vk::Sampler &operator[](SamplerHandle handle);
    void release(SamplerHandle handle);
    ///@}
    // </editor-fold>

    // <editor-fold desc="Descriptor layouts">
    /**
     * @name Descriptor layouts
     */
    ///@{
    DescriptorSetLayoutHandle
    createDescriptorLayout(std::string_view name,
                           const Magnum::Vk::DescriptorSetLayoutCreateInfo &createInfo);
    Magnum::Vk::DescriptorSetLayout &operator[](DescriptorSetLayoutHandle handle);
    void release(DescriptorSetLayoutHandle handle);
    ///@}
    // </editor-fold>

  private:
    std::unique_ptr<struct ResourceManagerPrivate> data_;
};
static_assert(std::movable<ResourceManager>);

} // namespace Cory
