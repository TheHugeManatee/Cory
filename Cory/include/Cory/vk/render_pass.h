#pragma once

#include "graphics_context.h"

#include <vulkan/vulkan.h>

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace cory::vk {

class graphics_context;
class subpass_description_builder;

using framebuffer = std::shared_ptr<VkFramebuffer_T>;

class render_pass {
  public:
    render_pass(graphics_context &ctx,
                std::shared_ptr<VkRenderPass_T> vk_pass_ptr,
                std::string_view name)
        : ctx_{ctx}
        , vk_pass_ptr_{vk_pass_ptr}
        , name_{name}
    {
    }

    const std::vector<cory::vk::framebuffer> &swapchain_framebuffers();
    cory::vk::framebuffer framebuffer(cory::vk::image_view &view);

    [[nodiscard]] auto get() { return vk_pass_ptr_.get(); }

  private:
    graphics_context &ctx_;
    std::string name_;
    std::shared_ptr<VkRenderPass_T> vk_pass_ptr_;
    std::vector<cory::vk::framebuffer> swapchain_framebuffers_;
};

class render_pass_builder {
  public:
    render_pass_builder(graphics_context &context)
        : ctx_{context}
    {
    }

    render_pass_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    render_pass_builder &flags(VkRenderPassCreateFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    render_pass_builder &name(std::string_view name) noexcept
    {
        name_ = name;
        return *this;
    }

    VkAttachmentReference add_color_attachment(VkFormat format, VkSampleCountFlagBits samples);
    VkAttachmentReference add_color_attachment(const VkAttachmentDescription &colorAttachment,
                                               VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED);

    VkAttachmentReference set_depth_attachment(VkFormat format, VkSampleCountFlagBits samples);
    VkAttachmentReference set_depth_attachment(const VkAttachmentDescription &colorAttachment,
                                               VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED);

    VkAttachmentReference
    add_resolve_attachment(VkFormat format,
                           VkImageLayout finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    VkAttachmentReference add_resolve_attachment(const VkAttachmentDescription &colorAttachment,
                                                 VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED);

    /**
     * Add a subpass dependency to depend on the VK_SUBPASS_EXTERNAL event of the
     * previous frame
     */
    render_pass_builder &add_previous_frame_dependency();

    render_pass_builder &add_subpass_dependency(VkSubpassDependency dependency);

    /**
     * Add a default configured graphics subpass that has all color, depth and
     * resolve attachments that the builder knows about.
     * @return the subpass index of the added pass
     */
    uint32_t add_default_subpass();

    // NOTE: the order of attachments directly corresponds to the
    // 'layout(location=0) out vec4 color' index in the fragment shader
    // - pInputAttachments: attachments that are read from a shader
    // - pResolveAttachments: attachments used for multisampling color attachments
    // - pDepthStencilAttachment: attachment for depth and stencil data
    // - pPreserveAttachments: attachments that are not currently used by the
    //   subpass but for which the data needs to be preserved.
    uint32_t add_subpass(subpass_description_builder subpass_builder);

    [[nodiscard]] render_pass create();

  private:
    VkAttachmentReference add_attachment(const VkAttachmentDescription &desc,
                                         VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED);

  private:
    graphics_context &ctx_;
    VkRenderPassCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,

    };
    std::string name_;

    std::vector<VkAttachmentDescription> attachments_;
    std::vector<VkAttachmentReference> color_attachment_refs_;
    std::vector<VkAttachmentReference> resolve_attachment_refs_;
    std::optional<VkAttachmentReference> depth_stencil_attachment_ref_;
    std::vector<VkSubpassDependency> subpass_dependencies_;

    std::vector<subpass_description_builder> subpasses_;
};

/**
 * Creates an attachment descriptor with a default initalized description. the default attachment
 * builder looks like this:
 *
 * @code
 * VkAttachmentDescription info_{.flags = {},
 *                               .format = VK_FORMAT_UNDEFINED,
 *                               .samples = VK_SAMPLE_COUNT_1_BIT,
 *                               .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
 *                               .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
 *                               .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
 *                               .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
 *                               .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
 *                               .finalLayout = VK_IMAGE_LAYOUT_UNDEFINED};
 * @endcode
 */
class attachment_description_builder {
  public:
    attachment_description_builder &flags(VkAttachmentDescriptionFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    attachment_description_builder &format(VkFormat format) noexcept
    {
        info_.format = format;
        return *this;
    }

    attachment_description_builder &samples(VkSampleCountFlagBits samples) noexcept
    {
        info_.samples = samples;
        return *this;
    }

    attachment_description_builder &load_op(VkAttachmentLoadOp loadOp) noexcept
    {
        info_.loadOp = loadOp;
        return *this;
    }

    attachment_description_builder &store_op(VkAttachmentStoreOp storeOp) noexcept
    {
        info_.storeOp = storeOp;
        return *this;
    }

    attachment_description_builder &stencil_load_op(VkAttachmentLoadOp stencilLoadOp) noexcept
    {
        info_.stencilLoadOp = stencilLoadOp;
        return *this;
    }

    attachment_description_builder &stencil_store_op(VkAttachmentStoreOp stencilStoreOp) noexcept
    {
        info_.stencilStoreOp = stencilStoreOp;
        return *this;
    }

    attachment_description_builder &initial_layout(VkImageLayout initialLayout) noexcept
    {
        info_.initialLayout = initialLayout;
        return *this;
    }

    attachment_description_builder &final_layout(VkImageLayout finalLayout) noexcept
    {
        info_.finalLayout = finalLayout;
        return *this;
    }

    attachment_description_builder &name(std::string_view name) noexcept
    {
        name_ = name;
        return *this;
    }

    [[nodiscard]] const VkAttachmentDescription &get() const noexcept { return info_; }

    [[nodiscard]] operator VkAttachmentDescription() const noexcept { return get(); }

  private:
    VkAttachmentDescription info_{.flags = {},
                                  .format = VK_FORMAT_UNDEFINED,
                                  .samples = VK_SAMPLE_COUNT_1_BIT,
                                  .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                  .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                  .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                  .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                  .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                  .finalLayout = VK_IMAGE_LAYOUT_UNDEFINED};
    std::string_view name_;
};

/**
 * Creates a subpass descriptor with a default initalized description. the default subpass binds to
 * the graphics bind point and does not have any attachments.
 */
class subpass_description_builder {
  public:
    subpass_description_builder &flags(VkSubpassDescriptionFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    subpass_description_builder &pipeline_bind_point(VkPipelineBindPoint pipelineBindPoint) noexcept
    {
        info_.pipelineBindPoint = pipelineBindPoint;
        return *this;
    }

    subpass_description_builder &
    input_attachments(std::vector<VkAttachmentReference> inputAttachments) noexcept
    {
        input_attachments_ = std::move(inputAttachments);
        return *this;
    }

    subpass_description_builder &
    color_attachments(std::vector<VkAttachmentReference> colorAttachments) noexcept
    {
        color_attachments_ = std::move(colorAttachments);
        return *this;
    }

    subpass_description_builder &
    resolve_attachments(std::vector<VkAttachmentReference> resolveAttachments) noexcept
    {
        resolve_attachments_ = std::move(resolveAttachments);
        return *this;
    }

    subpass_description_builder &
    depth_stencil_attachment(VkAttachmentReference depthStencilAttachment) noexcept
    {
        depth_stencil_attachment_ = depthStencilAttachment;
        return *this;
    }

    subpass_description_builder &
    preserve_attachments(std::vector<uint32_t> preserveAttachments) noexcept
    {
        preserve_attachments_ = std::move(preserveAttachments);
        return *this;
    }

    subpass_description_builder &name(std::string_view name) noexcept
    {
        name_ = name;
        return *this;
    }
    [[nodiscard]] const VkSubpassDescription &get()
    {
        info_.inputAttachmentCount = static_cast<uint32_t>(input_attachments_.size());
        info_.pInputAttachments = input_attachments_.data();

        info_.colorAttachmentCount = static_cast<uint32_t>(color_attachments_.size());
        info_.pColorAttachments = color_attachments_.data();

        info_.pDepthStencilAttachment =
            depth_stencil_attachment_.has_value() ? &*depth_stencil_attachment_ : nullptr;

        info_.preserveAttachmentCount = static_cast<uint32_t>(preserve_attachments_.size());
        info_.pResolveAttachments = resolve_attachments_.data();

        info_.preserveAttachmentCount = static_cast<uint32_t>(preserve_attachments_.size());
        info_.pPreserveAttachments = preserve_attachments_.data();

        return info_;
    }
    [[nodiscard]] operator VkSubpassDescription() { return get(); }

  private:
    VkSubpassDescription info_{
        .flags = {},
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    };
    std::vector<VkAttachmentReference> input_attachments_;
    std::vector<VkAttachmentReference> color_attachments_;
    std::vector<VkAttachmentReference> resolve_attachments_;
    std::optional<VkAttachmentReference> depth_stencil_attachment_{};
    std::vector<uint32_t> preserve_attachments_;
    std::string_view name_;
};

} // namespace cory::vk