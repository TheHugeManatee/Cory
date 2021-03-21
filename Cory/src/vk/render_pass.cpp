#include "Cory/vk/render_pass.h"

#include "Cory/Log.h"

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

    auto attachmentRef = add_attachment(color_att_dec, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    color_attachment_refs_.push_back(attachmentRef);
    return attachmentRef;
}

VkAttachmentReference
render_pass_builder::add_color_attachment(VkAttachmentDescription colorAttachment)
{
    auto attachmentRef = add_attachment(colorAttachment, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    color_attachment_refs_.push_back(attachmentRef);
    return attachmentRef;
}

VkAttachmentReference render_pass_builder::add_depth_attachment(VkFormat format,
                                                                VkSampleCountFlagBits samples)
{
    CO_CORE_ASSERT(!depth_stencil_attachment_ref_.has_value(),
                   "DepthStencil attachment is already set");

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = format;
    depthAttachment.samples = samples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    depth_stencil_attachment_ref_ =
        add_attachment(depthAttachment, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    return *depth_stencil_attachment_ref_;
}

VkAttachmentReference render_pass_builder::add_resolve_attachment(VkFormat format,
                                                                  VkImageLayout finalLayout)
{
    auto resolve_att_desc = attachment_description_builder()
                                .format(format)
                                .store_op(VK_ATTACHMENT_STORE_OP_STORE)
                                .final_layout(finalLayout);

    auto attachmentRef = add_attachment(resolve_att_desc, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    resolve_attachment_refs_.push_back(attachmentRef);
    return attachmentRef;
}

uint32_t render_pass_builder::add_default_subpass()
{
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<uint32_t>(color_attachment_refs_.size());
    subpass.pColorAttachments = color_attachment_refs_.data();
    if (depth_stencil_attachment_ref_)
        subpass.pDepthStencilAttachment = &*depth_stencil_attachment_ref_;
    else
        subpass.pDepthStencilAttachment = nullptr;
    subpass.pResolveAttachments = resolve_attachment_refs_.data();

    return add_subpass(subpass);
}

uint32_t render_pass_builder::add_subpass(VkSubpassDescription subpassDesc)
{
    auto subpassIdx = static_cast<uint32_t>(subpasses_.size());
    subpasses_.emplace_back(std::move(subpassDesc));
    return subpassIdx;
}

render_pass render_pass_builder::create()
{
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.attachmentCount = static_cast<uint32_t>(render_pass_builder.size());
    renderPassInfo.pAttachments = attachments_.data();
    renderPassInfo.subpassCount = static_cast<uint32_t>(subpasses_.size());
    renderPassInfo.pSubpasses = subpasses_.data();
    renderPassInfo.dependencyCount = static_cast<uint32_t>(subpass_dependencies_.size());
    renderPassInfo.pDependencies = subpass_dependencies_.data();

    return ctx.device->createRenderPass(renderPassInfo);
}

VkAttachmentReference render_pass_builder::add_attachment(VkAttachmentDescription desc,
                                                          VkImageLayout layout)
{
    auto attachmentID = static_cast<uint32_t>(attachments_.size());
    attachments_.push_back(desc);
    VkAttachmentReference attachmentRef{};
    attachmentRef.attachment = attachmentID;
    attachmentRef.layout = layout;
    return attachmentRef;
}

} // namespace cory::vk

#ifndef DOCTEST_CONFIG_DISABLE

#include <doctest/doctest.h>

#include "Cory/vk/test_utils.h"

TEST_SUITE_BEGIN("cory::Vkrender_pass");

TEST_CASE("attachment_description_builder interface")
{
    auto att_desc = cory::attachment_description_builder()
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
    VkAttachmentReference color_att0;
    VkAttachmentReference depth_stencil_att0;

    VkSubpassDescription subpass_desc = cory::vk::subpass_description_builder()
                                            .pipeline_bind_point(VK_PIPELINE_BIND_POINT_GRAPHICS)
                                            .color_attachments({color_att0})
                                            .depth_stencil_attachment(depth_stencil_att0);
}

TEST_CASE("render pass creation")
{

    cory::vk::graphics_context &ctx = cory::vk::test_context();

    cory::vk::render_pass_builder builder(ctx);
    auto color_att0 =
        builder.add_color_attachment(cory::vk::attachment_description_builder()
                                         .format(swapchain_format)
                                         .samples(ctx.max_msaa_samples())
                                         .load_op(VK_ATTACHMENT_LOAD_OP_CLEAR)
                                         .store_op(VK_ATTACHMENT_STORE_OP_STORE)
                                         .final_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    auto depth_att0 =
        builder.add_depth_attachment(ctx.default_depth_format(), ctx.max_msaa_samples());

    cory::vk::render_pass render_pass = builder
                                            .add_subpass(cory::vk::subpass_description_builder()
                                                             .name("geometry")
                                                             .color_attachments({color_att0})
                                                             .depth_stencil_attachment(depth_att0))
                                            .add_previous_frame_dependency()
                                            .create();
}

TEST_SUITE_END();

#endif