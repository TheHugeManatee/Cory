#pragma once

#include <vector>
#include <memory>

namespace cory::vk {

    class shader_stage_builder {
  public:
    friend class shader_stage;
    shader_stage_builder(graphics_context &context)
        : ctx_{context}
    {
    }

    shader_stage_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    shader_stage_builder &flags(VkPipelineShaderStageCreateFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    shader_stage_builder &stage(VkShaderStageFlagBits stage) noexcept
    {
        info_.stage = stage;
        return *this;
    }

    shader_stage_builder &module(VkShaderModule module) noexcept
    {
        info_.module = module;
        return *this;
    }

    shader_stage_builder &name(const char *pName) noexcept
    {
        info_.pName = pName;
        return *this;
    }

    shader_stage_builder &
    specialization_info(const VkSpecializationInfo *pSpecializationInfo) noexcept
    {
        info_.pSpecializationInfo = pSpecializationInfo;
        return *this;
    }

    shader_stage_builder &name(std::string_view name) noexcept
    {
        name_ = name;
        return *this;
    }
    [[nodiscard]] shader_stage create() { return shader_stage(*this); }

  private:
    graphics_context &ctx_;
    VkPipelineShaderStageCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    };
    std::string_view name_;
};

class vertex_input_binding_description_builder {
  public:
    friend class vertex_input_binding_description;
    vertex_input_binding_description_builder(graphics_context &context)
        : ctx_{context}
    {
    }

    vertex_input_binding_description_builder &binding(uint32_t binding) noexcept
    {
        info_.binding = binding;
        return *this;
    }

    vertex_input_binding_description_builder &stride(uint32_t stride) noexcept
    {
        info_.stride = stride;
        return *this;
    }

    vertex_input_binding_description_builder &input_rate(VkVertexInputRate inputRate) noexcept
    {
        info_.inputRate = inputRate;
        return *this;
    }

    vertex_input_binding_description_builder &name(std::string_view name) noexcept
    {
        name_ = name;
        return *this;
    }
    [[nodiscard]] vertex_input_binding_description create()
    {
        return vertex_input_binding_description(*this);
    }

  private:
    graphics_context &ctx_;
    VkVertexInputBindingDescription info_{
        .sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION,
    };
    std::string_view name_;
};

class vertex_input_attribute_description_builder {
  public:
    friend class vertex_input_attribute_description;
    vertex_input_attribute_description_builder(graphics_context &context)
        : ctx_{context}
    {
    }

    vertex_input_attribute_description_builder &location(uint32_t location) noexcept
    {
        info_.location = location;
        return *this;
    }

    vertex_input_attribute_description_builder &binding(uint32_t binding) noexcept
    {
        info_.binding = binding;
        return *this;
    }

    vertex_input_attribute_description_builder &format(VkFormat format) noexcept
    {
        info_.format = format;
        return *this;
    }

    vertex_input_attribute_description_builder &offset(uint32_t offset) noexcept
    {
        info_.offset = offset;
        return *this;
    }

    vertex_input_attribute_description_builder &name(std::string_view name) noexcept
    {
        name_ = name;
        return *this;
    }
    [[nodiscard]] vertex_input_attribute_description create()
    {
        return vertex_input_attribute_description(*this);
    }

  private:
    graphics_context &ctx_;
    VkVertexInputAttributeDescription info_{
        .sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION,
    };
    std::string_view name_;
};

class viewport_state_builder {
  public:
    friend class viewport_state;
    viewport_state_builder(graphics_context &context)
        : ctx_{context}
    {
    }

    viewport_state_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    viewport_state_builder &flags(VkPipelineViewportStateCreateFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    viewport_state_builder &viewport_count(uint32_t viewportCount) noexcept
    {
        info_.viewportCount = viewportCount;
        return *this;
    }

    viewport_state_builder &viewports(const VkViewport *pViewports) noexcept
    {
        info_.pViewports = pViewports;
        return *this;
    }

    viewport_state_builder &scissor_count(uint32_t scissorCount) noexcept
    {
        info_.scissorCount = scissorCount;
        return *this;
    }

    viewport_state_builder &scissors(const VkRect2D *pScissors) noexcept
    {
        info_.pScissors = pScissors;
        return *this;
    }

    viewport_state_builder &name(std::string_view name) noexcept
    {
        name_ = name;
        return *this;
    }
    [[nodiscard]] viewport_state create() { return viewport_state(*this); }

  private:
    graphics_context &ctx_;
    VkPipelineViewportStateCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    };
    std::string_view name_;
};

class rasterization_state_builder {
  public:
    friend class rasterization_state;
    rasterization_state_builder(graphics_context &context)
        : ctx_{context}
    {
    }

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

    rasterization_state_builder &name(std::string_view name) noexcept
    {
        name_ = name;
        return *this;
    }
    [[nodiscard]] rasterization_state create() { return rasterization_state(*this); }

  private:
    graphics_context &ctx_;
    VkPipelineRasterizationStateCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    };
    std::string_view name_;
};

class multisample_state_builder {
  public:
    friend class multisample_state;
    multisample_state_builder(graphics_context &context)
        : ctx_{context}
    {
    }

    multisample_state_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    multisample_state_builder &flags(VkPipelineMultisampleStateCreateFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    multisample_state_builder &
    rasterization_samples(VkSampleCountFlagBits rasterizationSamples) noexcept
    {
        info_.rasterizationSamples = rasterizationSamples;
        return *this;
    }

    multisample_state_builder &sample_shading_enable(VkBool32 sampleShadingEnable) noexcept
    {
        info_.sampleShadingEnable = sampleShadingEnable;
        return *this;
    }

    multisample_state_builder &min_sample_shading(float minSampleShading) noexcept
    {
        info_.minSampleShading = minSampleShading;
        return *this;
    }

    multisample_state_builder &sample_mask(const VkSampleMask *pSampleMask) noexcept
    {
        info_.pSampleMask = pSampleMask;
        return *this;
    }

    multisample_state_builder &alpha_to_coverage_enable(VkBool32 alphaToCoverageEnable) noexcept
    {
        info_.alphaToCoverageEnable = alphaToCoverageEnable;
        return *this;
    }

    multisample_state_builder &alpha_to_one_enable(VkBool32 alphaToOneEnable) noexcept
    {
        info_.alphaToOneEnable = alphaToOneEnable;
        return *this;
    }

    multisample_state_builder &name(std::string_view name) noexcept
    {
        name_ = name;
        return *this;
    }
    [[nodiscard]] multisample_state create() { return multisample_state(*this); }

  private:
    graphics_context &ctx_;
    VkPipelineMultisampleStateCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    };
    std::string_view name_;
};

class depth_stencil_state_builder {
  public:
    friend class depth_stencil_state;
    depth_stencil_state_builder(graphics_context &context)
        : ctx_{context}
    {
    }

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

    depth_stencil_state_builder &name(std::string_view name) noexcept
    {
        name_ = name;
        return *this;
    }
    [[nodiscard]] depth_stencil_state create() { return depth_stencil_state(*this); }

  private:
    graphics_context &ctx_;
    VkPipelineDepthStencilStateCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    };
    std::string_view name_;
};

class color_blend_attachment_state_builder {
  public:
    friend class color_blend_attachment_state;
    color_blend_attachment_state_builder(graphics_context &context)
        : ctx_{context}
    {
    }

    color_blend_attachment_state_builder &blend_enable(VkBool32 blendEnable) noexcept
    {
        info_.blendEnable = blendEnable;
        return *this;
    }

    color_blend_attachment_state_builder &
    src_color_blend_factor(VkBlendFactor srcColorBlendFactor) noexcept
    {
        info_.srcColorBlendFactor = srcColorBlendFactor;
        return *this;
    }

    color_blend_attachment_state_builder &
    dst_color_blend_factor(VkBlendFactor dstColorBlendFactor) noexcept
    {
        info_.dstColorBlendFactor = dstColorBlendFactor;
        return *this;
    }

    color_blend_attachment_state_builder &color_blend_op(VkBlendOp colorBlendOp) noexcept
    {
        info_.colorBlendOp = colorBlendOp;
        return *this;
    }

    color_blend_attachment_state_builder &
    src_alpha_blend_factor(VkBlendFactor srcAlphaBlendFactor) noexcept
    {
        info_.srcAlphaBlendFactor = srcAlphaBlendFactor;
        return *this;
    }

    color_blend_attachment_state_builder &
    dst_alpha_blend_factor(VkBlendFactor dstAlphaBlendFactor) noexcept
    {
        info_.dstAlphaBlendFactor = dstAlphaBlendFactor;
        return *this;
    }

    color_blend_attachment_state_builder &alpha_blend_op(VkBlendOp alphaBlendOp) noexcept
    {
        info_.alphaBlendOp = alphaBlendOp;
        return *this;
    }

    color_blend_attachment_state_builder &
    color_write_mask(VkColorComponentFlags colorWriteMask) noexcept
    {
        info_.colorWriteMask = colorWriteMask;
        return *this;
    }

    color_blend_attachment_state_builder &name(std::string_view name) noexcept
    {
        name_ = name;
        return *this;
    }
    [[nodiscard]] color_blend_attachment_state create()
    {
        return color_blend_attachment_state(*this);
    }

  private:
    graphics_context &ctx_;
    VkPipelineColorBlendAttachmentState info_{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_ATTACHMENT_STATE,
    };
    std::string_view name_;
};

}