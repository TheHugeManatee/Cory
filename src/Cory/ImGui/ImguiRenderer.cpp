/*
  This file is part of KDGpu.

  SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "ImguiRenderer.hpp"

#include <Cory/Base/GlmUtils.hpp>
#include <Cory/Renderer/Shader.hpp>

#include <KDGpuExample/kdgpuexample.h>

#include <KDGpu/bind_group_layout_options.h>
#include <KDGpu/bind_group_options.h>
#include <KDGpu/buffer_options.h>
#include <KDGpu/device.h>
#include <KDGpu/render_pass.h>
#include <KDGpu/shader_object_options.h>
#include <KDGpu/texture_options.h>
#include <KDUtils/color.h>

#include <cmrc/cmrc.hpp>
#include <gsl/narrow>
#include <imgui.h>

#include <vector>

CMRC_DECLARE(KDGpuExample::ShaderResources);
CMRC_DECLARE(KDGpuExample::Resources);

using namespace KDGpu;

namespace {

const char *vertexShaderSource = R"(
struct PushConstants
{
    float2 scale;
    float2 translate;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> pushConstants;

struct VSInput
{
    float2 inPos   : POSITION;   // layout(location = 0)
    float2 inUV    : TEXCOORD0;  // layout(location = 1)
    float4 inColor : COLOR0;     // layout(location = 2)
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 outUV    : TEXCOORD0; // location = 0
    float4 outColor : COLOR0;    // location = 1
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.outUV = input.inUV;
    output.outColor = input.inColor;
    output.position =
        float4(input.inPos * pushConstants.scale + pushConstants.translate,
               0.0, 1.0);
    return output;
})";

const char *fragmentShaderSource = R"(
struct FSInput
{
    float2 inUV    : TEXCOORD0; // location = 0
    float4 inColor : COLOR0;    // location = 1
};

[[vk::binding(0, 0)]]
Sampler2D fontSampler;

float4 main(FSInput input) : SV_Target0
{
    float4 tex = fontSampler.Sample(input.inUV);
    return input.inColor * tex;
})";

struct VertexImGui {
    static VertexBufferLayout vertexBufferLayout()
    {
        return VertexBufferLayout{
            .binding = 0,
            .stride = sizeof(ImDrawVert),
            .inputRate = Gpu::VertexRate::Vertex,
        };
    }

    static std::vector<VertexAttribute> vertexAttributes()
    {
        // clang-format off
        static std::vector<VertexAttribute> attributes = {{
            .location = 0,
            .binding = 0,
            .format = Gpu::Format::R32G32_SFLOAT,
            .offset = offsetof(ImDrawVert, pos),
        }, {
            .location = 1,
            .binding = 0,
            .format = Gpu::Format::R32G32_SFLOAT,
            .offset = offsetof(ImDrawVert, uv),
        }, {
            .location = 2,
            .binding = 0,
            .format = Gpu::Format::R8G8B8A8_UNORM,
            .offset = offsetof(ImDrawVert, col),
        }};
        // clang-format on
        return attributes;
    }
};

std::vector<uint32_t> readShaderFileFromCmrc(cmrc::embedded_filesystem &fs,
                                             const std::string &filename)
{
    auto file = fs.open(filename);
    const std::size_t byteSize = file.size();
    std::vector<uint32_t> buffer(byteSize / 4);
    std::memcpy(buffer.data(), file.cbegin(), byteSize);
    return buffer;
}

} // namespace

namespace Cory {

ImGuiRenderer::ImGuiRenderer(Gpu::Device *device, Gpu::Queue *queue, ImGuiContext *imGuiContext)
    : m_device(device)
    , m_queue(queue)
    , m_imGuiContext(imGuiContext)
{
    ImGui::SetCurrentContext(m_imGuiContext);

    // Color scheme
    ImGuiStyle &style = ImGui::GetStyle();
    style.ChildRounding = 5.0f;
    style.FrameRounding = 2.0f;
    style.PopupRounding = 5.0f;
    style.WindowRounding = 5.0f;
    style.AntiAliasedFill = true;
    style.AntiAliasedLines = true;
    style.ItemSpacing = ImVec2(8.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);

    style.Colors[ImGuiCol_Text] = KDUtils::hexToRgba<ImVec4>("#e2e8f0", 1.0f);
    style.Colors[ImGuiCol_WindowBg] = KDUtils::hexToRgba<ImVec4>("#2a2726", 0.85f);
    style.Colors[ImGuiCol_TitleBg] = KDUtils::hexToRgba<ImVec4>("#1e293b", 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = KDUtils::hexToRgba<ImVec4>("#334155", 1.0f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = KDUtils::hexToRgba<ImVec4>("#1e293b", 1.0f);
}

ImGuiRenderer::~ImGuiRenderer() {}

void ImGuiRenderer::initialize(float scaleFactor,
                               Gpu::SampleCountFlagBits samples,
                               Gpu::Format colorFormat,
                               Gpu::Format depthFormat)
{
    (void)colorFormat;
    (void)depthFormat;
    m_samples = samples;

    const auto vertShaderCode = Shader::CompileToSpv(
        ShaderSource{vertexShaderSource, Gpu::ShaderStageFlagBits::VertexBit, "imgui.vert"});
    const auto fragShaderCode = Shader::CompileToSpv(
        ShaderSource{fragmentShaderSource, Gpu::ShaderStageFlagBits::FragmentBit, "imgui.frag"});

    m_bindGroupLayout = m_device->createBindGroupLayout(BindGroupLayoutOptions{
        .bindings =
            {
                {.binding = 0,
                 .count = 1,
                 .resourceType = ResourceBindingType::CombinedImageSampler,
                 .shaderStages = ShaderStageFlagBits::FragmentBit},
            },
    });

    const std::vector<PushConstantRange> pushConstantRanges{
        PushConstantRange{
            .offset = 0,
            .size = sizeof(PushConstantBlock),
            .shaderStages = ShaderStageFlagBits::VertexBit,
        },
    };

    m_pipelineLayout = m_device->createPipelineLayout(
        PipelineLayoutOptions{.bindGroupLayouts = {m_bindGroupLayout},
                              .pushConstantRanges = pushConstantRanges});

    m_vertexShaderObject = m_device->createShaderObject(ShaderObjectOptions{
        .label = "ImGui Vertex Shader",
        .stage = ShaderStageFlagBits::VertexBit,
        .nextStage = ShaderStageFlagBits::FragmentBit,
        .code = vertShaderCode,
        .entryPoint = "main",
        .bindGroupLayouts = {m_bindGroupLayout},
        .pushConstantRanges = pushConstantRanges,
    });

    m_fragmentShaderObject = m_device->createShaderObject(ShaderObjectOptions{
        .label = "ImGui Fragment Shader",
        .stage = ShaderStageFlagBits::FragmentBit,
        .nextStage = {},
        .code = fragShaderCode,
        .entryPoint = "main",
        .bindGroupLayouts = {m_bindGroupLayout},
        .pushConstantRanges = pushConstantRanges,
    });

    m_shaderStages = {
        ShaderStageFlags{ShaderStageFlagBits::VertexBit},
        ShaderStageFlags{ShaderStageFlagBits::FragmentBit},
    };
    m_shaderHandles = {m_vertexShaderObject.handle(), m_fragmentShaderObject.handle()};

    m_vertexLayouts = {VertexImGui::vertexBufferLayout()};
    m_vertexAttributes = VertexImGui::vertexAttributes();

    const auto samplerOptions =
        SamplerOptions{.magFilter = FilterMode::Linear, .minFilter = FilterMode::Linear};
    m_sampler = m_device->createSampler(samplerOptions);

    updateScale(scaleFactor);
}

void ImGuiRenderer::updateScale(const float scaleFactor)
{
    ImGuiStyle &style = ImGui::GetStyle();
    style.ScaleAllSizes(scaleFactor / m_oldScaleFactor);

    initializeFontData(scaleFactor);

    m_oldScaleFactor = scaleFactor;
}

void ImGuiRenderer::cleanup()
{
    m_meshes.clear();
    m_pipelineLayout = {};
    m_bindGroupLayout = {};
    m_bindGroup = {};
    m_sampler = {};
    m_textureView = {};
    m_texture = {};
    m_vertexShaderObject = {};
    m_fragmentShaderObject = {};
    m_shaderStages.clear();
    m_shaderHandles.clear();
    m_vertexLayouts.clear();
    m_vertexAttributes.clear();
}

bool ImGuiRenderer::updateGeometryBuffers(FrameContext &frameCtx)
{
    ImDrawData *imDrawData = ImGui::GetDrawData();

    if (!imDrawData) return false;

    // Note: Alignment is done inside buffer creation
    const size_t vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
    const size_t indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

    // Update buffers only if vertex or index count has been changed compared to current buffer size
    if ((vertexBufferSize == 0) || (indexBufferSize == 0)) return false;

    if (m_meshes.size() <= frameCtx.inFlightIndex) {
        m_meshes.resize(frameCtx.inFlightIndex + 1);
    }
    m_mesh = &m_meshes[frameCtx.inFlightIndex];

    // Vertex buffer
    if ((!m_mesh->vertices.isValid()) ||
        (m_mesh->vertexCount != static_cast<uint32_t>(imDrawData->TotalVtxCount))) {
        m_mesh->vertices = m_device->createBuffer(BufferOptions{
            .label = "Triangle Vertices",
            .size = vertexBufferSize,
            .usage = BufferUsageFlagBits::VertexBufferBit,
            .memoryUsage = MemoryUsage::CpuToGpu,
        });
        m_mesh->vertexCount = imDrawData->TotalVtxCount;
    }

    // Index buffer
    if ((!m_mesh->indexBuffer.isValid()) ||
        (m_mesh->indexCount < static_cast<uint32_t>(imDrawData->TotalIdxCount))) {
        m_mesh->indexBuffer = m_device->createBuffer(BufferOptions{
            .label = "Triangle Indices",
            .size = indexBufferSize,
            .usage = BufferUsageFlagBits::IndexBufferBit,
            .memoryUsage = MemoryUsage::CpuToGpu,
        });
        m_mesh->indexCount = imDrawData->TotalIdxCount;
    }

    // Upload data
    ImDrawVert *vtxDst = static_cast<ImDrawVert *>(m_mesh->vertices.map());
    ImDrawIdx *idxDst = static_cast<ImDrawIdx *>(m_mesh->indexBuffer.map());

    for (int n = 0; n < imDrawData->CmdListsCount; n++) {
        const ImDrawList *cmd_list = imDrawData->CmdLists[n];
        memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtxDst += cmd_list->VtxBuffer.Size;
        idxDst += cmd_list->IdxBuffer.Size;
    }

    // Flush
    m_mesh->vertices.unmap();
    m_mesh->indexBuffer.unmap();

    return m_mesh->vertexCount != 0;
}
void ImGuiRenderer::recordCommands(FrameContext &frameCtx, Gpu::RenderPassCommandRecorder *recorder)
{
    ImDrawData *imDrawData = ImGui::GetDrawData();

    if ((!imDrawData) || (imDrawData->CmdListsCount == 0)) return;

    int32_t vertexOffset = 0;
    uint32_t indexOffset = 0;

    recorder->bindShaders(m_shaderStages, m_shaderHandles);
    recorder->setPrimitiveTopology(PrimitiveTopology::TriangleList);
    recorder->setPrimitiveRestartEnabled(false);
    recorder->setCullMode(CullModeFlagBits::None);
    recorder->setFrontFace(FrontFace::CounterClockwise);
    recorder->setPolygonMode(PolygonMode::Fill);
    recorder->setRasterizerDiscardEnabled(false);
    recorder->setRasterizationSamples(m_samples);
    recorder->setDepthTestEnabled(false);
    recorder->setDepthWriteEnabled(false);
    recorder->setDepthCompareOp(CompareOperation::Always);
    recorder->setDepthBiasEnabled(false);
    recorder->setDepthBoundsTestEnabled(false);
    recorder->setDepthClampEnabled(false);
    recorder->setStencilTestEnabled(false);
    recorder->setAlphaToCoverageEnabled(false);
    recorder->setAlphaToOneEnabled(false);
    recorder->setLogicOpEnabled(false);
    recorder->setVertexInput(m_vertexLayouts, m_vertexAttributes);

    ColorComponentFlags colorMask{};
    colorMask.setFlag(ColorComponentFlagBits::RedBit, true);
    colorMask.setFlag(ColorComponentFlagBits::GreenBit, true);
    colorMask.setFlag(ColorComponentFlagBits::BlueBit, true);
    colorMask.setFlag(ColorComponentFlagBits::AlphaBit, true);
    const ColorBlendEquation blendEquation{
        .srcColorBlendFactor = BlendFactor::SrcAlpha,
        .dstColorBlendFactor = BlendFactor::OneMinusSrcAlpha,
        .colorBlendOp = BlendOperation::Add,
        .srcAlphaBlendFactor = BlendFactor::One,
        .dstAlphaBlendFactor = BlendFactor::OneMinusSrcAlpha,
        .alphaBlendOp = BlendOperation::Add,
    };
    const std::vector<bool> blendEnables{true};
    const std::vector<ColorBlendEquation> blendEquations{blendEquation};
    const std::vector<ColorComponentFlags> colorMasks{colorMask};
    recorder->setColorBlendEnabled(0, blendEnables);
    recorder->setColorBlendEquations(0, blendEquations);
    recorder->setColorWriteMasks(0, colorMasks);
    const std::vector<SampleMask> sampleMasks(1, 0xffffffffu);
    recorder->setSampleMask(m_samples, sampleMasks);

    // Bind the descriptor set
    recorder->setBindGroup(0, m_bindGroup, m_pipelineLayout);

    // Set the push constants
    const float displaySize[2] = {imDrawData->DisplaySize.x, imDrawData->DisplaySize.y};
    const float displayPos[2] = {imDrawData->DisplayPos.x, imDrawData->DisplayPos.y};
    m_pushConstantBlock.scale[0] = 2.0f / displaySize[0];
    m_pushConstantBlock.scale[1] = 2.0f / displaySize[1];
    m_pushConstantBlock.translate[0] = -1.0f - displayPos[0] * m_pushConstantBlock.scale[0];
    m_pushConstantBlock.translate[1] = -1.0f - displayPos[1] * m_pushConstantBlock.scale[1];

    recorder->pushConstant(
        PushConstantRange{
            .offset = 0,
            .size = sizeof(PushConstantBlock),
            .shaderStages = ShaderStageFlagBits::VertexBit,
        },
        &m_pushConstantBlock,
        m_pipelineLayout);

    // Set Viewport and scissor rect
    recorder->setViewportWithCount({Gpu::Viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(frameCtx.extent.x),
        .height = static_cast<float>(frameCtx.extent.y),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    }});

    recorder->setScissorWithCount({Gpu::Rect2D{
        .offset = {0, 0},
        .extent = {frameCtx.extent.x, frameCtx.extent.y},
    }});

    // Bind the vertex and index buffers
    recorder->setVertexBuffer(0, m_mesh->vertices);
    recorder->setIndexBuffer(m_mesh->indexBuffer, 0, IndexType::Uint16);

    for (int32_t cmdIdx = 0; cmdIdx < imDrawData->CmdListsCount; cmdIdx++) {
        const ImDrawList *cmd_list = imDrawData->CmdLists[cmdIdx];

        for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++) {
            const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[j];

            // Set the scissor rect
            recorder->setScissor(Gpu::Rect2D{
                .offset =
                    {
                        .x = std::max(static_cast<int32_t>(pcmd->ClipRect.x), 0),
                        .y = std::max(static_cast<int32_t>(pcmd->ClipRect.y), 0),
                    },
                .extent =
                    {
                        .width = static_cast<uint32_t>(pcmd->ClipRect.z - pcmd->ClipRect.x),
                        .height = static_cast<uint32_t>(pcmd->ClipRect.w - pcmd->ClipRect.y),
                    },
            });

            // And finally, draw a part of the UI
            recorder->drawIndexed(DrawIndexedCommand{
                .indexCount = pcmd->ElemCount,
                .firstIndex = indexOffset,
                .vertexOffset = vertexOffset,
            });

            indexOffset += pcmd->ElemCount;
        }
        vertexOffset += cmd_list->VtxBuffer.Size;
    }
}

void ImGuiRenderer::initializeFontData(const float scaleFactor)
{
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->Clear();

    // Clear previous font texture, view
    m_texture = {};
    m_textureView = {};

    // Create font texture, view
    unsigned char *fontData;
    int texWidth, texHeight;
    auto fs = cmrc::KDGpuExample::Resources::get_filesystem();
    auto ttfFile = fs.open("fonts/Roboto-Medium.ttf");
    auto ttfData = const_cast<void *>(static_cast<const void *>(ttfFile.begin()));
    ImFontConfig fontConfig{};
    fontConfig.FontDataOwnedByAtlas = false;
    const float fontPixelSize = 18.0f * scaleFactor;
    io.Fonts->AddFontFromMemoryTTF(
        ttfData, gsl::narrow<int>(ttfFile.size()), fontPixelSize, &fontConfig);
    io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
    DeviceSize uploadSize = texWidth * texHeight * 4 * sizeof(char);

    const auto textureOptions = TextureOptions{
        .type = TextureType::TextureType2D,
        .format = Format::R8G8B8A8_UNORM,
        .extent = {.width = static_cast<uint32_t>(texWidth),
                   .height = static_cast<uint32_t>(texHeight),
                   .depth = 1},
        .mipLevels = 1,
        .usage = TextureUsageFlagBits::SampledBit | TextureUsageFlagBits::TransferDstBit};
    m_texture = m_device->createTexture(textureOptions);

    // Upload the font texture data
    // clang-format off
    const std::vector<BufferTextureCopyRegion> regions = {{
        .textureSubResource = { .aspectMask = TextureAspectFlagBits::ColorBit },
        .textureExtent = { .width = static_cast<uint32_t>(texWidth), .height = static_cast<uint32_t>(texHeight), .depth = 1 }
    }};
    // clang-format on
    const WaitForTextureUploadOptions uploadOptions = {
        .destinationTexture = m_texture,
        .dstStages = PipelineStageFlagBit::FragmentShaderBit,
        .data = fontData,
        .byteSize = uploadSize,
        .oldLayout = TextureLayout::Undefined,
        .newLayout = TextureLayout::ShaderReadOnlyOptimal,
        .regions = regions};
    m_queue->waitForUploadTextureData(uploadOptions);

    m_textureView = m_texture.createView();

    // Update previous bind group if it exists
    if (m_bindGroup.isValid()) {
        const BindGroupEntry entry{.binding = 0,
                                   .resource = TextureViewSamplerBinding{
                                       .textureView = m_textureView, .sampler = m_sampler}};

        m_bindGroup.update(entry);
    }
    else {
        // Create a bind group for the font texture
        const BindGroupOptions bindGroupOptions = {.layout = m_bindGroupLayout,
                                                   .resources = {
                                                       {
                                                           .binding = 0,
                                                           .resource =
                                                               TextureViewSamplerBinding{
                                                                   .textureView = m_textureView,
                                                                   .sampler = m_sampler,
                                                               },
                                                       },
                                                   }};
        m_bindGroup = m_device->createBindGroup(bindGroupOptions);
    }
}

} // namespace Cory
