#include "Cory/vk/render_pass.h"

#include "Cory/Log.h"

#include "Cory/vk/graphics_context.h"

namespace cory::vk {

render_pass_builder &render_pass_builder::add_previous_frame_dependency()
{
    //****************** Subpass dependencies ******************
    // this sets up the render pass to wait for the
    // STAGE_COLOR_ATTACHMENT_OUTPUT stage to ensure the images are available
    // and the swap chain is not still reading the image
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    // not sure here..
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    dependency.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    subpass_dependencies_.emplace_back(std::move(dependency));

    return *this;
}

render_pass_builder &render_pass_builder::add_subpass_dependency(VkSubpassDependency dependency)
{
    subpass_dependencies_.emplace_back(std::move(dependency));
    return *this;
}

VkAttachmentReference render_pass_builder::add_color_attachment(VkFormat format,
                                                                VkSampleCountFlagBits samples)
{
    // care about color, but not stencil
    auto color_att_dec = attachment_description_builder()
                             .format(format)
                             .samples(samples)
                             .load_op(VK_ATTACHMENT_LOAD_OP_CLEAR)
                             .store_op(VK_ATTACHMENT_STORE_OP_STORE)
                             .final_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    auto attachmentRef = add_attachment(color_att_dec);
    color_attachment_refs_.push_back(attachmentRef);
    return attachmentRef;
}

VkAttachmentReference
render_pass_builder::add_color_attachment(VkAttachmentDescription colorAttachment)
{
    auto attachmentRef = add_attachment(colorAttachment);
    color_attachment_refs_.push_back(attachmentRef);
    return attachmentRef;
}

VkAttachmentReference render_pass_builder::add_depth_attachment(VkFormat format,
                                                                VkSampleCountFlagBits samples)
{
    CO_CORE_ASSERT(!depth_stencil_attachment_ref_.has_value(),
                   "DepthStencil attachment is already set");

    VkAttachmentDescription depth_att =
        attachment_description_builder()
            .format(format)
            .samples(samples)
            .load_op(VK_ATTACHMENT_LOAD_OP_CLEAR)
            .final_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    depth_stencil_attachment_ref_ = add_attachment(depth_att);
    return *depth_stencil_attachment_ref_;
}

VkAttachmentReference render_pass_builder::add_resolve_attachment(VkFormat format,
                                                                  VkImageLayout finalLayout)
{
    VkAttachmentDescription resolve_att_desc = attachment_description_builder()
                                                   .format(format)
                                                   .store_op(VK_ATTACHMENT_STORE_OP_STORE)
                                                   .final_layout(finalLayout);

    auto attachmentRef = add_attachment(resolve_att_desc);
    resolve_attachment_refs_.push_back(attachmentRef);
    return attachmentRef;
}

uint32_t render_pass_builder::add_default_subpass()
{
    auto subpass_builder = subpass_description_builder()
                               .color_attachments(color_attachment_refs_)
                               .resolve_attachments(resolve_attachment_refs_);

    if (depth_stencil_attachment_ref_)
        subpass_builder.depth_stencil_attachment(*depth_stencil_attachment_ref_);

    return add_subpass(subpass_builder);
}

uint32_t render_pass_builder::add_subpass(subpass_description_builder &subpassDesc)
{
    auto subpassIdx = static_cast<uint32_t>(subpasses_.size());
    subpasses_.emplace_back(std::move(subpassDesc));
    return subpassIdx;
}

render_pass render_pass_builder::create()
{
    std::vector<VkSubpassDescription> vk_subpasses(subpasses_.size());
    std::transform(subpasses_.begin(),
                   subpasses_.end(),
                   vk_subpasses.begin(),
                   [](subpass_description_builder &pass_builder) { return pass_builder.get(); });

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments_.size());
    renderPassInfo.pAttachments = attachments_.data();
    renderPassInfo.subpassCount = static_cast<uint32_t>(vk_subpasses.size());
    renderPassInfo.pSubpasses = vk_subpasses.data();
    renderPassInfo.dependencyCount = static_cast<uint32_t>(subpass_dependencies_.size());
    renderPassInfo.pDependencies = subpass_dependencies_.data();

    VkRenderPass vk_render_pass;
    vkCreateRenderPass(ctx_.device(), &renderPassInfo, nullptr, &vk_render_pass);

    return make_shared_resource(vk_render_pass, [dev = ctx_.device()](VkRenderPass p) {
        vkDestroyRenderPass(dev, p, nullptr);
    });
}

/**
 * adds an attachment with the given description. if \a layout is empty, will use the finalLayout
 * from the description
 */
VkAttachmentReference
render_pass_builder::add_attachment(VkAttachmentDescription desc,
                                    VkImageLayout layout /*= VK_IMAGE_LAYOUT_UNDEFINED*/)
{
    auto attachmentID = static_cast<uint32_t>(attachments_.size());
    attachments_.push_back(desc);
    VkAttachmentReference attachmentRef{};
    attachmentRef.attachment = attachmentID;
    if (layout == VK_IMAGE_LAYOUT_UNDEFINED) { attachmentRef.layout = desc.finalLayout; }
    else {
        attachmentRef.layout = layout;
    }
    return attachmentRef;
}

} // namespace cory::vk

#ifndef DOCTEST_CONFIG_DISABLE

#include <doctest/doctest.h>

#include "Cory/vk/test_utils.h"

TEST_SUITE_BEGIN("cory::vk::render_pass");

TEST_CASE("attachment_description_builder interface")
{
    auto att_desc = cory::vk::attachment_description_builder()
                        .name("test_attachment_desc")
                        .store_op(VK_ATTACHMENT_STORE_OP_STORE)
                        .stencil_store_op(VK_ATTACHMENT_STORE_OP_STORE)
                        .get();

    CHECK(att_desc.loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE);
    CHECK(att_desc.storeOp == VK_ATTACHMENT_STORE_OP_STORE);
    CHECK(att_desc.stencilStoreOp == VK_ATTACHMENT_STORE_OP_STORE);
}

TEST_CASE("subpass_description_builder interface")
{
    VkAttachmentReference color_att0{};
    VkAttachmentReference depth_stencil_att0{};

    VkSubpassDescription subpass_desc = cory::vk::subpass_description_builder()
                                            .pipeline_bind_point(VK_PIPELINE_BIND_POINT_GRAPHICS)
                                            .color_attachments({color_att0})
                                            .depth_stencil_attachment(depth_stencil_att0);
}

TEST_CASE("render pass creation")
{

    cory::vk::graphics_context ctx = cory::vk::test_context();

    VkFormat swapchain_format = ctx.default_color_format();
    VkFormat depth_stencil_format = ctx.default_depth_stencil_format();
    VkSampleCountFlagBits samples = ctx.max_msaa_samples();

    cory::vk::render_pass_builder builder(ctx);
    auto color_att0 =
        builder.add_color_attachment(cory::vk::attachment_description_builder()
                                         .format(swapchain_format)
                                         .samples(samples)
                                         .load_op(VK_ATTACHMENT_LOAD_OP_CLEAR)
                                         .store_op(VK_ATTACHMENT_STORE_OP_STORE)
                                         .final_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    auto depth_att0 = builder.add_depth_attachment(depth_stencil_format, samples);

    builder.add_subpass(cory::vk::subpass_description_builder()
                            .name("geometry")
                            .color_attachments({color_att0})
                            .depth_stencil_attachment(depth_att0));
    builder.add_previous_frame_dependency();

    cory::vk::render_pass render_pass = builder.create();
}

TEST_SUITE_END();

#endif