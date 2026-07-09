/*
  This file is part of KDGpu.

  SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <Cory/Renderer/FrameContext.hpp>

#include <KDGpu/bind_group.h>
#include <KDGpu/bind_group_layout.h>
#include <KDGpu/buffer.h>
#include <KDGpu/gpu_core.h>
#include <KDGpu/graphics_pipeline_options.h>
#include <KDGpu/pipeline_layout.h>
#include <KDGpu/sampler.h>
#include <KDGpu/shader_object.h>
#include <KDGpu/texture.h>
#include <KDGpu/texture_view.h>

#include <glm/vec2.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace KDGpu {
class Device;
class Queue;
class RenderPassCommandRecorder;
class RenderPass;
} // namespace KDGpu

struct ImGuiContext;

namespace Cory {

using ImGuiTextureId = uint64_t;
inline constexpr ImGuiTextureId InvalidImGuiTextureId = 0;

/*
 * @brief ImGui renderer abstraction to adapt to KDGpu.
 *
 * This class is based off of KDGpuExample::ImGuiRenderer, but has some adaptations for handling
 * multiple meshes with an overlapping rendering pipeline.
 */
class ImGuiRenderer {
  public:
    ImGuiRenderer(Gpu::Device *device, Gpu::Queue *queue, ImGuiContext *imGuiContext);
    ~ImGuiRenderer();

    ImGuiRenderer(const ImGuiRenderer &other) noexcept = delete;
    ImGuiRenderer &operator=(const ImGuiRenderer &other) noexcept = delete;

    ImGuiRenderer(ImGuiRenderer &&other) noexcept = default;
    ImGuiRenderer &operator=(ImGuiRenderer &&other) noexcept = default;

    void initialize(float scaleFactor,
                    Gpu::SampleCountFlagBits samples,
                    Gpu::Format colorFormat,
                    Gpu::Format depthFormat);
    void updateScale(float scaleFactor);
    void cleanup();

    bool updateGeometryBuffers(FrameContext &frameCtx);
    void recordCommands(FrameContext &frameCtx, Gpu::RenderPassCommandRecorder *recorder);

    [[nodiscard]] ImGuiTextureId registerTexture(std::string_view label,
                                                 glm::u32vec2 size,
                                                 std::span<const std::byte> pixelsRgba8);
    void unregisterTexture(ImGuiTextureId textureId);

  private:
    void initializeFontData(float scaleFactor);

    struct MeshData {
        Gpu::Buffer vertices;
        Gpu::Buffer indexBuffer;
        bool isIndexed{false};
        uint32_t vertexCount{0};
        uint32_t indexCount{0};
        Gpu::IndexType indexType{Gpu::IndexType::Uint32};
    };

    // TODO: Handle multiple frames in flight
    std::vector<MeshData> m_meshes;
    MeshData *m_mesh{nullptr};

    Gpu::BindGroupLayout m_bindGroupLayout;
    Gpu::BindGroup m_bindGroup;
    Texture m_texture;
    TextureView m_textureView;
    Gpu::Sampler m_sampler;
    Gpu::Sampler m_imageSampler;

    struct RegisteredTexture {
        std::string label;
        Texture texture;
        TextureView textureView;
        Gpu::BindGroup bindGroup;
    };
    void rebuildRegisteredTextureBindGroups();
    [[nodiscard]] Gpu::BindGroup createRegisteredTextureBindGroup(const RegisteredTexture &texture);

    std::unordered_map<ImGuiTextureId, RegisteredTexture> m_registeredTextures;
    ImGuiTextureId m_nextTextureId{1};
    static constexpr ImGuiTextureId FontTextureId = std::numeric_limits<ImGuiTextureId>::max();

    struct PushConstantBlock {
        float scale[2];
        float translate[2];
    };
    PushConstantBlock m_pushConstantBlock;

    Gpu::Device *m_device{nullptr};
    Gpu::Queue *m_queue{nullptr};
    ImGuiContext *m_imGuiContext{nullptr};

    Gpu::ShaderObject m_vertexShaderObject;
    Gpu::ShaderObject m_fragmentShaderObject;
    Gpu::PipelineLayout m_pipelineLayout;
    std::vector<Gpu::ShaderStageFlags> m_shaderStages;
    std::vector<Gpu::Handle<Gpu::ShaderObject_t>> m_shaderHandles;
    std::vector<Gpu::VertexBufferLayout> m_vertexLayouts;
    std::vector<Gpu::VertexAttribute> m_vertexAttributes;
    Gpu::SampleCountFlagBits m_samples{Gpu::SampleCountFlagBits::Samples1Bit};

    float m_oldScaleFactor = 1.0f;
};

} // namespace Cory
