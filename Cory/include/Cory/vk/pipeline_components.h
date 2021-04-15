#pragma once

#include <vulkan/vulkan.h>

#include "utils.h"

#include <glm.h>

#include <vector>

namespace cory::vk {

class vertex_input_state_builder {
  public:
    vertex_input_state_builder() {}

    vertex_input_state_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    vertex_input_state_builder &flags(VkPipelineVertexInputStateCreateFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    vertex_input_state_builder &
    vertex_binding_description_count(uint32_t vertexBindingDescriptionCount) noexcept
    {
        info_.vertexBindingDescriptionCount = vertexBindingDescriptionCount;
        return *this;
    }

    vertex_input_state_builder &vertex_binding_descriptions(
        const VkVertexInputBindingDescription *pVertexBindingDescriptions) noexcept
    {
        info_.pVertexBindingDescriptions = pVertexBindingDescriptions;
        return *this;
    }

    vertex_input_state_builder &
    vertex_attribute_description_count(uint32_t vertexAttributeDescriptionCount) noexcept
    {
        info_.vertexAttributeDescriptionCount = vertexAttributeDescriptionCount;
        return *this;
    }

    vertex_input_state_builder &vertex_attribute_descriptions(
        const VkVertexInputAttributeDescription *pVertexAttributeDescriptions) noexcept
    {
        info_.pVertexAttributeDescriptions = pVertexAttributeDescriptions;
        return *this;
    }

    [[nodiscard]] auto get() const noexcept { return info_; }

  private:
    VkPipelineVertexInputStateCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };
};

class input_assembly_state_builder {
  public:
    input_assembly_state_builder() {}

    input_assembly_state_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    input_assembly_state_builder &flags(VkPipelineInputAssemblyStateCreateFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    input_assembly_state_builder &topology(VkPrimitiveTopology topology) noexcept
    {
        info_.topology = topology;
        return *this;
    }

    input_assembly_state_builder &primitive_restart_enable(VkBool32 primitiveRestartEnable) noexcept
    {
        info_.primitiveRestartEnable = primitiveRestartEnable;
        return *this;
    }

    [[nodiscard]] const auto &get() const noexcept { return info_; }

  private:
    VkPipelineInputAssemblyStateCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    };
};

class pipeline_viewport_state_builder {
  public:
    pipeline_viewport_state_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    pipeline_viewport_state_builder &flags(VkPipelineViewportStateCreateFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    pipeline_viewport_state_builder &viewport_count(uint32_t viewportCount) noexcept
    {
        info_.viewportCount = viewportCount;
        return *this;
    }

    pipeline_viewport_state_builder &viewports(std::vector<VkViewport> viewports) noexcept
    {
        viewports_ = std::move(viewports);
        return *this;
    }

    pipeline_viewport_state_builder &scissors(std::vector<VkRect2D> scissors) noexcept
    {
        scissors_ = std::move(scissors);
        return *this;
    }

    [[nodiscard]] const auto &get() noexcept
    {
        info_.viewportCount = static_cast<uint32_t>(viewports_.size());
        info_.pViewports = viewports_.data();
        info_.scissorCount = static_cast<uint32_t>(scissors_.size());
        info_.pScissors = scissors_.data();
        return info_;
    }

  private:
    VkPipelineViewportStateCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    };
    std::vector<VkViewport> viewports_;
    std::vector<VkRect2D> scissors_;
};

class rasterization_state_builder {
  public:
    rasterization_state_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    rasterization_state_builder &flags(VkPipelineRasterizationStateCreateFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    rasterization_state_builder &depth_clamp_enable(VkBool32 depthClampEnable) noexcept
    {
        info_.depthClampEnable = depthClampEnable;
        return *this;
    }

    rasterization_state_builder &
    rasterizer_discard_enable(VkBool32 rasterizerDiscardEnable) noexcept
    {
        info_.rasterizerDiscardEnable = rasterizerDiscardEnable;
        return *this;
    }

    rasterization_state_builder &polygon_mode(VkPolygonMode polygonMode) noexcept
    {
        info_.polygonMode = polygonMode;
        return *this;
    }

    rasterization_state_builder &cull_mode(VkCullModeFlags cullMode) noexcept
    {
        info_.cullMode = cullMode;
        return *this;
    }

    rasterization_state_builder &front_face(VkFrontFace frontFace) noexcept
    {
        info_.frontFace = frontFace;
        return *this;
    }

    rasterization_state_builder &depth_bias_enable(VkBool32 depthBiasEnable) noexcept
    {
        info_.depthBiasEnable = depthBiasEnable;
        return *this;
    }

    rasterization_state_builder &depth_bias_constant_factor(float depthBiasConstantFactor) noexcept
    {
        info_.depthBiasConstantFactor = depthBiasConstantFactor;
        return *this;
    }

    rasterization_state_builder &depth_bias_clamp(float depthBiasClamp) noexcept
    {
        info_.depthBiasClamp = depthBiasClamp;
        return *this;
    }

    rasterization_state_builder &depth_bias_slope_factor(float depthBiasSlopeFactor) noexcept
    {
        info_.depthBiasSlopeFactor = depthBiasSlopeFactor;
        return *this;
    }

    rasterization_state_builder &line_width(float lineWidth) noexcept
    {
        info_.lineWidth = lineWidth;
        return *this;
    }

    [[nodiscard]] const auto &get() const noexcept { return info_; }

  private:
    VkPipelineRasterizationStateCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    };
};

class multisampling_state_builder {
  public:
    multisampling_state_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    multisampling_state_builder &flags(VkPipelineMultisampleStateCreateFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    multisampling_state_builder &
    rasterization_samples(VkSampleCountFlagBits rasterizationSamples) noexcept
    {
        info_.rasterizationSamples = rasterizationSamples;
        return *this;
    }

    multisampling_state_builder &sample_shading_enable(VkBool32 sampleShadingEnable) noexcept
    {
        info_.sampleShadingEnable = sampleShadingEnable;
        return *this;
    }

    multisampling_state_builder &min_sample_shading(float minSampleShading) noexcept
    {
        info_.minSampleShading = minSampleShading;
        return *this;
    }

    multisampling_state_builder &sample_mask(const VkSampleMask *pSampleMask) noexcept
    {
        info_.pSampleMask = pSampleMask;
        return *this;
    }

    multisampling_state_builder &alpha_to_coverage_enable(VkBool32 alphaToCoverageEnable) noexcept
    {
        info_.alphaToCoverageEnable = alphaToCoverageEnable;
        return *this;
    }

    multisampling_state_builder &alpha_to_one_enable(VkBool32 alphaToOneEnable) noexcept
    {
        info_.alphaToOneEnable = alphaToOneEnable;
        return *this;
    }

    [[nodiscard]] const auto &get() const noexcept { return info_; }

  private:
    VkPipelineMultisampleStateCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    };
};

class depth_stencil_state_builder {
  public:
depth_stencil_state_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    depth_stencil_state_builder &flags(VkPipelineDepthStencilStateCreateFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    depth_stencil_state_builder &depth_test_enable(VkBool32 depthTestEnable) noexcept
    {
        info_.depthTestEnable = depthTestEnable;
        return *this;
    }

    depth_stencil_state_builder &depth_write_enable(VkBool32 depthWriteEnable) noexcept
    {
        info_.depthWriteEnable = depthWriteEnable;
        return *this;
    }

    depth_stencil_state_builder &depth_compare_op(VkCompareOp depthCompareOp) noexcept
    {
        info_.depthCompareOp = depthCompareOp;
        return *this;
    }

    depth_stencil_state_builder &depth_bounds_test_enable(VkBool32 depthBoundsTestEnable) noexcept
    {
        info_.depthBoundsTestEnable = depthBoundsTestEnable;
        return *this;
    }

    depth_stencil_state_builder &stencil_test_enable(VkBool32 stencilTestEnable) noexcept
    {
        info_.stencilTestEnable = stencilTestEnable;
        return *this;
    }

    depth_stencil_state_builder &front(VkStencilOpState front) noexcept
    {
        info_.front = front;
        return *this;
    }

    depth_stencil_state_builder &back(VkStencilOpState back) noexcept
    {
        info_.back = back;
        return *this;
    }

    depth_stencil_state_builder &min_depth_bounds(float minDepthBounds) noexcept
    {
        info_.minDepthBounds = minDepthBounds;
        return *this;
    }

    depth_stencil_state_builder &max_depth_bounds(float maxDepthBounds) noexcept
    {
        info_.maxDepthBounds = maxDepthBounds;
        return *this;
    }

    [[nodiscard]] const auto &get() const noexcept { return info_; }

  private:
    VkPipelineDepthStencilStateCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    };
};

class color_blend_state_builder {
  public:
    friend class color_blend_state;
    color_blend_state_builder() {}

    color_blend_state_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    color_blend_state_builder &flags(VkPipelineColorBlendStateCreateFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    color_blend_state_builder &logic_op_enable(VkBool32 logicOpEnable) noexcept
    {
        info_.logicOpEnable = logicOpEnable;
        return *this;
    }

    color_blend_state_builder &logic_op(VkLogicOp logicOp) noexcept
    {
        info_.logicOp = logicOp;
        return *this;
    }

    color_blend_state_builder &attachment_count(uint32_t attachmentCount) noexcept
    {
        info_.attachmentCount = attachmentCount;
        return *this;
    }

    color_blend_state_builder &
    attachments(std::vector<VkPipelineColorBlendAttachmentState> attachments) noexcept
    {
        attachments_ = std::move(attachments);
        return *this;
    }

    color_blend_state_builder &blend_constants(glm::vec4 blendConstants) noexcept
    {
        std::tie(info_.blendConstants[0],
                 info_.blendConstants[1],
                 info_.blendConstants[2],
                 info_.blendConstants[3]) =
            std::tie(blendConstants.x, blendConstants.y, blendConstants.z, blendConstants.w);
        return *this;
    }

    [[nodiscard]] const auto &get() noexcept
    {
        info_.attachmentCount = static_cast<uint32_t>(attachments_.size());
        info_.pAttachments = attachments_.data();
        return info_;
    }

  private:
    VkPipelineColorBlendStateCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    };
    std::vector<VkPipelineColorBlendAttachmentState> attachments_;
};


} // namespace cory::vk