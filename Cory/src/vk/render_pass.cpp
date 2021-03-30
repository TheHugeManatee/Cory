#include "Cory/vk/render_pass.h"

#include "Cory/Log.h"
#include "Cory/vk/utils.h"

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
render_pass_builder::add_color_attachment(const VkAttachmentDescription &colorAttachment,
                                          VkImageLayout layout /*= VK_IMAGE_LAYOUT_UNDEFINED*/)
{
    auto attachmentRef = add_attachment(colorAttachment, layout);
    color_attachment_refs_.push_back(attachmentRef);
    return attachmentRef;
}

VkAttachmentReference render_pass_builder::set_depth_attachment(VkFormat format,
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

VkAttachmentReference
render_pass_builder::set_depth_attachment(const VkAttachmentDescription &colorAttachment,
                                          VkImageLayout layout /*= VK_IMAGE_LAYOUT_UNDEFINED*/)
{
    CO_CORE_ASSERT(!depth_stencil_attachment_ref_.has_value(),
                   "DepthStencil attachment is already set");
    depth_stencil_attachment_ref_ = add_attachment(colorAttachment, layout);
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

uint32_t render_pass_builder::add_subpass(subpass_description_builder subpassDesc)
{
    auto subpassIdx = static_cast<uint32_t>(subpasses_.size());
    subpasses_.emplace_back(std::move(subpassDesc));
    return subpassIdx;
}

render_pass render_pass_builder::create()
{
    // init and advance the counter
    static uint32_t created_objects_counter{};
    if (name_.empty()) { name_ = fmt::format("render_pass #{}", created_objects_counter); }
    created_objects_counter++;

    // put all subpass descriptions into a contiguous block of memory so we can reference them as a
    // pointer in the CreateInfo
    std::vector<VkSubpassDescription> vk_subpasses(subpasses_.size());
    std::transform(subpasses_.begin(),
                   subpasses_.end(),
                   vk_subpasses.begin(),
                   [](subpass_description_builder &pass_builder) { return pass_builder.get(); });

    VkRenderPassCreateInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = static_cast<uint32_t>(attachments_.size()),
        .pAttachments = attachments_.data(),
        .subpassCount = static_cast<uint32_t>(vk_subpasses.size()),
        .pSubpasses = vk_subpasses.data(),
        .dependencyCount = static_cast<uint32_t>(subpass_dependencies_.size()),
        .pDependencies = subpass_dependencies_.data(),
    };

    VkRenderPass vk_render_pass;
    VK_CHECKED_CALL(vkCreateRenderPass(ctx_.device(), &renderPassInfo, nullptr, &vk_render_pass),
                    "Failed to create a render pass!");

    auto vk_pass_ptr = make_shared_resource(vk_render_pass, [dev = ctx_.device()](VkRenderPass p) {
        vkDestroyRenderPass(dev, p, nullptr);
    });
    return {ctx_, vk_pass_ptr, name_};
}

/**
 * adds an attachment with the given description. if \a layout is empty, will use the finalLayout
 * from the description
 */
VkAttachmentReference
render_pass_builder::add_attachment(const VkAttachmentDescription &desc,
                                    VkImageLayout layout /*= VK_IMAGE_LAYOUT_UNDEFINED*/)
{
    auto attachmentID = static_cast<uint32_t>(attachments_.size());
    attachments_.emplace_back(desc);
    VkAttachmentReference attachmentRef{};
    attachmentRef.attachment = attachmentID;
    if (layout == VK_IMAGE_LAYOUT_UNDEFINED) { attachmentRef.layout = desc.finalLayout; }
    else {
        attachmentRef.layout = layout;
    }
    return attachmentRef;
}

const std::vector<cory::vk::framebuffer> &render_pass::swapchain_framebuffers()
{
    CO_CORE_ASSERT(ctx_.swapchain().has_value(),
                   "Can't create framebuffers for swapchain: current graphics_context does not "
                   "have a swapchain!");
    CO_CORE_TRACE("Creating framebuffers for renderpass {}", name_);

    // create these framebuffers only once
    if (swapchain_framebuffers_.empty()) {
        // create the framebuffers for the swapchain images. This will connect the render-pass
        // to the images for rendering
        VkFramebufferCreateInfo fb_info = {};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.pNext = nullptr;

        fb_info.renderPass = vk_pass_ptr_.get();
        fb_info.attachmentCount = 1;
        fb_info.width = ctx_.swapchain()->extent().x;
        fb_info.height = ctx_.swapchain()->extent().y;
        fb_info.layers = 1;

        // grab how many images we have in the swapchain
        const uint32_t swapchain_imagecount = static_cast<uint32_t>(ctx_.swapchain()->size());

        swapchain_framebuffers_.reserve(swapchain_imagecount);

        // create framebuffers for each of the swapchain image views
        for (uint32_t i = 0; i < swapchain_imagecount; i++) {
            VkFramebuffer vk_framebuffer;
            const VkImageView &image_view = ctx_.swapchain()->views()[i].get();
            fb_info.pAttachments = &image_view;

            VK_CHECKED_CALL(vkCreateFramebuffer(ctx_.device(), &fb_info, nullptr, &vk_framebuffer),
                            "Could not create framebuffer for swapchain image");

            swapchain_framebuffers_.emplace_back(
                make_shared_resource(vk_framebuffer,
                                     [dev = ctx_.device()](VkFramebuffer f) {
                                         vkDestroyFramebuffer(dev, f, nullptr);
                                     }),
                ctx_.swapchain()->views()[i],
                *this);
        }
    }
    return swapchain_framebuffers_;
}

cory::vk::framebuffer render_pass::framebuffer(const cory::vk::image_view &view)
{
    // create the framebuffers for the swapchain images. This will connect the render-pass
    // to the images for rendering
    VkFramebufferCreateInfo fb_info = {};
    fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_info.pNext = nullptr;

    fb_info.renderPass = vk_pass_ptr_.get();
    fb_info.attachmentCount = 1;
    fb_info.width = view.size().x;
    fb_info.height = view.size().y;
    fb_info.layers = view.layers();

    VkFramebuffer vk_framebuffer;
    const VkImageView &image_view = view.get();
    fb_info.pAttachments = &image_view;

    VK_CHECKED_CALL(vkCreateFramebuffer(ctx_.device(), &fb_info, nullptr, &vk_framebuffer),
                    "Could not create framebuffer for swapchain image");

    auto vk_framebuffer_ptr =
        make_shared_resource(vk_framebuffer, [dev = ctx_.device()](VkFramebuffer f) {
            vkDestroyFramebuffer(dev, f, nullptr);
        });
    return cory::vk::framebuffer{vk_framebuffer_ptr, view, *this};
}

void render_pass::begin(command_buffer &cmd_buffer,
                        const cory::vk::framebuffer &fb,
                        std::vector<VkClearValue> clear_values,
                        VkSubpassContents subpass_contents /*= VK_SUBPASS_CONTENTS_INLINE*/)
{
    // start the main renderpass.
    // We will use the clear color from above, and the framebuffer of the index
    // the swapchain gave us
    VkRenderPassBeginInfo rpInfo = {};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.pNext = nullptr;

    rpInfo.renderPass = vk_pass_ptr_.get();
    rpInfo.renderArea.offset.x = 0;
    rpInfo.renderArea.offset.y = 0;
    rpInfo.renderArea.extent = {fb.size().x, fb.size().y};
    rpInfo.framebuffer = fb.get();

    // connect clear values
    rpInfo.clearValueCount = static_cast<uint32_t>(clear_values.size());
    rpInfo.pClearValues = clear_values.data();
    cmd_buffer.begin_render_pass(&rpInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void render_pass::end(command_buffer &cmd_buffer) { cmd_buffer.end_render_pass(); }

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
    auto depth_att0 = builder.set_depth_attachment(depth_stencil_format, samples);

    // add a second attachment with the generic attachment API
    auto color_att1 =
        builder.add_color_attachment(cory::vk::attachment_description_builder()
                                         .format(ctx.default_color_format())
                                         .load_op(VK_ATTACHMENT_LOAD_OP_CLEAR)
                                         .store_op(VK_ATTACHMENT_STORE_OP_STORE)
                                         .final_layout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR),
                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    builder.add_subpass(cory::vk::subpass_description_builder()
                            .name("geometry")
                            .color_attachments({color_att0, color_att1})
                            .depth_stencil_attachment(depth_att0));
    builder.add_previous_frame_dependency();

    cory::vk::render_pass render_pass = builder.create();

    SUBCASE("framebuffer creation")
    {
        auto im0 = ctx.build_image()
                       .extent({640, 480, 1})
                       .format(VK_FORMAT_R8G8B8A8_UINT)
                       .memory_usage(cory::vk::device_memory_usage::eGpuOnly)
                       .name("framebuffer test");
        auto im_view0 = ctx.build_image_view(im0).create();

        auto framebuffers = render_pass.framebuffer(im_view0);
        CHECK(framebuffers.get() != nullptr);
    }
}

TEST_SUITE_END();

#endif