#pragma once

// This is a C++ port of the "simple_vulkan_synchronization" library by Tobias Hector:
// https://github.com/Tobski/simple_vulkan_synchronization
//
// Changes (c) 2022 Jakob Weiss:
//   - Remove the THSVS prefix from functions and structures in favor of a namespace
//   - remove (comment out) many enum values for which my vulkan header did not have the right enum
//     values
//   - C++ify the interface, replacing count+pointer with std::span where applicable
//   - Rename enum values to avoid SCREAMING_SNAKE_CASE in favor of CamelCase + scoped enums

// The library is originally under the MIT license:
// Copyright (c) 2017-2019 Tobias Hector

// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is furnished to do
// so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

//// Simpler Vulkan Synchronization ////
/*
In an effort to make Vulkan synchronization more accessible, I created this
stb-inspired single-header library in order to somewhat simplify the core
synchronization mechanisms in Vulkan - pipeline barriers and events.

Rather than the complex maze of enums and bit flags in Vulkan - many
combinations of which are invalid or nonsensical - this library collapses
this to a much shorter list of 40 distinct usage types, and a couple of
options for handling image layouts.

Use of other synchronization mechanisms such as semaphores, fences and render
passes are not addressed in this API at present.

USAGE

   #define the symbol _SIMPLER_VULKAN_SYNCHRONIZATION_IMPLEMENTATION in
   *one* C/C++ file before the #include of this file; the implementation
   will be generated in that file.

VERSION

    alpha.9

    Alpha.9 adds the GetAccessInfo function to translate access types into a VkAccessInfo.


VERSION HISTORY

    alpha.8

    Alpha.8 adds a host preinitialization state for linear images, as well as a number of new access
sets for extensions released since the last update.

    alpha.7

    Alpha.7 incorporates a number of fixes from @gwihlidal, and fixes
    handling of pipeline stages in the presence of multiple access types or
    barriers in light of other recent changes.

    alpha.6

    Alpha.6 fixes a typo (VK_ACCESS_TYPE_MEMORY_READ|WRITE_BIT should have been
VK_ACCESS_MEMORY_READ|WRITE_BIT), and sets the pipeline stage src and dst flag bits to
VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT and VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT during initialization,
not 0 as per alpha.5

    alpha.5

    Alpha.5 now correctly zeroes out the pipeline stage flags before trying to incrementally set
bits on them... common theme here, whoops.

    alpha.4

    Alpha.4 now correctly zeroes out the access types before trying to incrementally set bits on
them (!)

    alpha.3

    Alpha.3 changes the following:

    Uniform and vertex buffer access in one enum, matching
D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER:
     - _ACCESS_ANY_SHADER_READ_UNIFORM_BUFFER_OR_VERTEX_BUFFER

    Color read *and* write access, matching D3D12_RESOURCE_STATE_RENDER_TARGET:
     - _ACCESS_COLOR_ATTACHMENT_READ_WRITE

    Also the "_ACCESS_*_SHADER_READ_SAMPLED_IMAGE" enums have been renamed to the form
"_ACCESS_*_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER"

    alpha.2

    Alpha.2 adds four new resource states for "ANY SHADER ACCESS":
     - _ACCESS_ANY_SHADER_READ_UNIFORM_BUFFER
     - _ACCESS_ANY_SHADER_READ_SAMPLED_IMAGE
     - _ACCESS_ANY_SHADER_READ_OTHER
     - _ACCESS_ANY_SHADER_WRITE

    alpha.1

    Alpha.1 adds three new resource states:
     - _ACCESS_GENERAL (Any access on the device)
     - _ACCESS_DEPTH_ATTACHMENT_WRITE_STENCIL_READ_ONLY (Write access to only the depth aspect
of a depth/stencil attachment)
     - _ACCESS_STENCIL_ATTACHMENT_WRITE_DEPTH_READ_ONLY (Write access to only the stencil
aspect of a depth/stencil attachment)

    It also fixes a couple of typos, and adds clarification as to when extensions need to be enabled
to use a feature.

    alpha.0

    This is the very first public release of this library; future revisions
    of this API may change the API in an incompatible manner as feedback is
    received.
    Once the version becomes stable, incompatible changes will only be made
    to major revisions of the API - minor revisions will only contain
    bug fixes or minor additions.

MEMORY ALLOCATION

    The CmdPipelineBarrier and thWaitEvents commands allocate temporary
    storage for the Vulkan barrier equivalents in order to pass them to the
    respective Vulkan commands.

    These use the `SYNC_TEMP_ALLOC(size)` and `SYNC_TEMP_FREE(x)` macros,
    which are by default set to alloca(size) and ((void)(x)), respectively.
    If you don't want to use stack space or would rather use your own
    allocation strategy, these can be overridden by defining these macros
    in before #include-ing the header file with
    _SIMPLER_VULKAN_SYNCHRONIZATION_IMPLEMENTATION defined.

    I'd rather avoid the need for these allocations in what are likely to be
    high-traffic commands, but currently just want to ship something - may
    revisit this at a future date based on feedback.

EXPRESSIVENESS COMPARED TO RAW VULKAN

    Despite the fact that this API is fairly simple, it expresses 99% of
    what you'd actually ever want to do in practice.
    Adding the missing expressiveness would result in increased complexity
    which didn't seem worth the trade off - however I would consider adding
    something for them in future if it becomes an issue.

    Here's a list of known things you can't express:

    * Execution only dependencies cannot be expressed.
      These are occasionally useful in conjunction with semaphores, or when
      trying to be clever with scheduling - but their usage is both limited
      and fairly tricky to get right anyway.
    * Depth/Stencil Input Attachments can be read in a shader using either
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL or
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL - this library
      *always* uses VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL.
      It's possible (though highly unlikely) when aliasing images that this
      results in unnecessary transitions.

ERROR CHECKS

    By default, as with the Vulkan API, this library does NOT check for
    errors.
    However, a number of optional error checks (_ERROR_CHECK_*) can be
    enabled by uncommenting the relevant #defines.
    Currently, error checks simply assert at the point a failure is detected
    and do not output an error message.
    I certainly do not claim they capture *all* possible errors, but they
    capture what should be some of the more common ones.
    Use of the Vulkan Validation Layers in tandem with this library is
    strongly recommended:
        https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers

ISSUES

    This header was clean of warnings using -Wall as of time of publishing
    on both gcc 4.8.4 and clang 3.5, using the c99 standard.

    There's a potential pitfall in CmdPipelineBarrier and CmdWaitEvents
    where alloca is used for temporary allocations. See MEMORY ALLOCATION
    for more information.

    Testing of this library is so far extremely limited with no immediate
    plans to add to that - so there's bound to be some amount of bugs.
    Please raise these issues on the repo issue tracker, or provide a fix
    via a pull request yourself if you're so inclined.
*/

#include <Magnum/Vk/Vk.h>
#include <Magnum/Vk/Vulkan.h>

#include <cstdint>
#include <span>

namespace Cory::Sync {

/**
AccessType defines all potential resource usages in the Vulkan API.
*/
enum class AccessType : uint32_t {
    // No access. Useful primarily for initialization
    NONE,

    // Read access
    //    // Requires VK_NV_device_generated_commands to be enabled
    //    // Command buffer read operation as defined by NV_device_generated_commands
    //    COMMAND_BUFFER_READ_NV,
    // Read as an indirect buffer for drawing or dispatch
    IndirectBuffer,
    // Read as an index buffer for drawing
    IndexBuffer,
    // Read as a vertex buffer for drawing
    VertexBuffer,
    // Read as a uniform buffer in a vertex shader
    VertexShaderReadUniformBuffer,
    // Read as a sampled image/uniform texel buffer in a vertex shader
    VertexShaderReadSampledImageOrUniformTexelBuffer,
    // Read as any other resource in a vertex shader
    VertexShaderReadOther,
    // Read as a uniform buffer in a tessellation control shader
    TessellationControlShaderReadUniformBuffer,
    // Read as a sampled image/uniform texel buffer  in a tessellation control shader
    TessellationControlShaderReadSampledImageOrUniformTexelBuffer,
    // Read as any other resource in a tessellation control shader
    TessellationControlShaderReadOther,
    // Read as a uniform buffer in a tessellation evaluation shader
    TessellationEvaluationShaderReadUniformBuffer,
    // Read as a sampled image/uniform texel buffer in a tessellation evaluation shader
    TessellationEvaluationShaderReadSampledImageOrUniformTexelBuffer,
    // Read as any other resource in a tessellation evaluation shader
    TessellationEvaluationShaderReadOther,
    // Read as a uniform buffer in a geometry shader
    GeometryShaderReadUniformBuffer,
    // Read as a sampled image/uniform texel buffer  in a geometry shader
    GeometryShaderReadSampledImageOrUniformTexelBuffer,
    // Read as any other resource in a geometry shader
    GeometryShaderReadOther,
    //    // Read as a uniform buffer in a task shader
    //    TASK_SHADER_READ_UNIFORM_BUFFER_NV,
    //    // Read as a sampled image/uniform texel buffer in a task shader
    //    TASK_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER_NV,
    //    // Read as any other resource in a task shader
    //    TASK_SHADER_READ_OTHER_NV,
    //    // Read as a uniform buffer in a mesh shader
    //    MESH_SHADER_READ_UNIFORM_BUFFER_NV,
    //    // Read as a sampled image/uniform texel buffer in a mesh shader
    //    MESH_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER_NV,
    //    // Read as any other resource in a mesh shader
    //    MESH_SHADER_READ_OTHER_NV,
    //    // Read as a transform feedback counter buffer
    //    TRANSFORM_FEEDBACK_COUNTER_READ_EXT,
    //    // Read as a fragment density map image
    //    FRAGMENT_DENSITY_MAP_READ_EXT,
    //    // Read as a shading rate image
    //    SHADING_RATE_READ_NV,
    //    // Read as a uniform buffer in a fragment shader
    FragmentShaderReadUniformBuffer,
    // Read as a sampled image/uniform texel buffer  in a fragment shader
    FragmentShaderReadSampledImageOrUniformTexelBuffer,
    // Read as an input attachment with a color format in a fragment shader
    FragmentShaderReadColorInputAttachment,
    // Read as an input attachment with a depth/stencil format in a fragment shader
    FragmentShaderReadDepthStencilInputAttachment,
    // Read as any other resource in a fragment shader
    FragmentShaderReadOther,
    // Read by standard blending/logic operations or subpass load operations
    ColorAttachmentRead,
    //    // Read by advanced blending, standard blending, logic operations, or subpass load
    //    operations
    //    COLOR_ATTACHMENT_ADVANCED_BLENDING_EXT,
    // Read by depth/stencil tests or subpass load operations
    DepthStencilAttachmentRead,
    // Read as a uniform buffer in a compute shader
    ComputeShaderReadUniformBuffer,
    // Read as a sampled image/uniform texel buffer in a compute shader
    ComputeShaderReadSampledImageOrUniformTexelBuffer,
    // Read as any other resource in a compute shader
    ComputeShaderReadOther,
    // Read as a uniform buffer in any shader
    AnyShaderReadUniformBuffer,
    // Read as a uniform buffer in any shader, or a vertex buffer
    AnyShaderReadUniformBufferOrVertexBuffer,
    // Read as a sampled image in any shader
    AnyShaderReadSampledImageOrUniformTexelBuffer,
    // Read as any other resource (excluding attachments) in any shader
    AnyShaderReadOther,
    // Read as the source of a transfer operation
    TransferRead,
    // Read on the host
    HostRead,

    // Requires VK_KHR_swapchain to be enabled
    // Read by the presentation engine (i.e. vkQueuePresentKHR)
    Present,

    //    // Requires VK_EXT_conditional_rendering to be enabled
    //    // Read by conditional rendering
    //    CONDITIONAL_RENDERING_READ_EXT,
    //
    //    // Requires VK_NV_ray_tracing to be enabled
    //    // Read by a ray tracing shader as an acceleration structure
    //    RAY_TRACING_SHADER_ACCELERATION_STRUCTURE_READ_NV,
    //    // Read as an acceleration structure during a build
    //    ACCELERATION_STRUCTURE_BUILD_READ_NV,

    // Read accesses end
    END_OF_READ_ACCESS,

    // Write access
    //    // Requires VK_NV_device_generated_commands to be enabled
    //    // Command buffer write operation
    //    COMMAND_BUFFER_WRITE_NV,
    // Written as any resource in a vertex shader
    VertexShaderWrite,
    // Written as any resource in a tessellation control shader
    TessellationControlShaderWrite,
    // Written as any resource in a tessellation evaluation shader
    TessellationEvaluationShaderWrite,
    // Written as any resource in a geometry shader
    GeometryShaderWrite,

    //    // Requires VK_NV_mesh_shading to be enabled
    //    // Written as any resource in a task shader
    //    TASK_SHADER_WRITE_NV,
    //    // Written as any resource in a mesh shader
    //    MESH_SHADER_WRITE_NV,
    //
    //    // Requires VK_EXT_transform_feedback to be enabled
    //    // Written as a transform feedback buffer
    //    TRANSFORM_FEEDBACK_WRITE_EXT,
    //    // Written as a transform feedback counter buffer
    //    TRANSFORM_FEEDBACK_COUNTER_WRITE_EXT,

    // Written as any resource in a fragment shader
    FragmentShaderWrite,
    // Written as a color attachment during rendering, or via a subpass store op
    ColorAttachmentWrite,
    // Written as a depth/stencil attachment during rendering, or via a subpass store op
    DepthStencilAttachmentWrite,

    // Requires VK_KHR_maintenance2 to be enabled
    // Written as a depth aspect of a depth/stencil attachment during rendering, whilst the stencil
    // aspect is read-only
    DepthAttachmentWriteStencilReadOnly,
    // Written as a stencil aspect of a depth/stencil attachment during rendering, whilst the depth
    // aspect is read-only
    StencilAttachmentWriteDepthReadOnly,

    // Written as any resource in a compute shader
    ComputeShaderWrite,
    // Written as any resource in any shader
    AnyShaderWrite,
    // Written as the destination of a transfer operation
    TransferWrite,
    // Data pre-filled by host before device access starts
    HostPreinitialized,
    // Written on the host
    HostWrite,

    //    // Requires VK_NV_ray_tracing to be enabled
    //    // Written as an acceleration structure during a build
    //    ACCELERATION_STRUCTURE_BUILD_WRITE_NV,

    // Read or written as a color attachment during rendering
    ColorAttachmentReadWrite,

    // General access
    // Covers any access - useful for debug, generally avoid for performance reasons
    General,

    // Number of access types
    NUM_ACCESS_TYPES
};

/**
ImageLayout defines a handful of layout options for images.
Rather than a list of all possible image layouts, this reduced list is
correlated with the access types to map to the correct Vulkan layouts.
_IMAGE_LAYOUT_OPTIMAL is usually preferred.
*/
enum class ImageLayout {
    // Choose the most optimal layout for each usage. Performs layout transitions as appropriate for
    // the access.
    Optimal,
    // Layout accessible by all Vulkan access types on a device - no layout transitions except for
    // presentation
    General,

    // Requires VK_KHR_shared_presentable_image to be enabled. Can only be used for shared
    // presentable images (i.e. single-buffered swap chains).
    // As General, but also allows presentation engines to access it - no layout transitions
    // GENERAL_AND_PRESENTATION
};

/**
Global barriers define a set of accesses on multiple resources at once.
If a buffer or image doesn't require a queue ownership transfer, or an image
doesn't require a layout transition (e.g. you're using one of the General
layouts) then a global barrier should be preferred.
Simply define the previous and next access types of resources affected.
*/
struct GlobalBarrier {
    std::span<AccessType> prevAccesses;
    std::span<AccessType> nextAccesses;
};

/**
Buffer barriers should only be used when a queue family ownership transfer
is required - prefer global barriers at all other times.

Access types are defined in the same way as for a global memory barrier, but
they only affect the buffer range identified by buffer, offset and size,
rather than all resources.
srcQueueFamilyIndex and dstQueueFamilyIndex will be passed unmodified into a
VkBufferMemoryBarrier.

A buffer barrier defining a queue ownership transfer needs to be executed
twice - once by a queue in the source queue family, and then once again by a
queue in the destination queue family, with a semaphore guaranteeing
execution order between them.
*/
struct BufferBarrier {
    std::span<AccessType> prevAccesses;
    std::span<AccessType> nextAccesses;
    uint32_t srcQueueFamilyIndex;
    uint32_t dstQueueFamilyIndex;
    VkBuffer buffer;
    VkDeviceSize offset;
    VkDeviceSize size;
};

/**
Image barriers should only be used when a queue family ownership transfer
or an image layout transition is required - prefer global barriers at all
other times.
In general it is better to use image barriers with _IMAGE_LAYOUT_OPTIMAL
than it is to use global barriers with images using either of the
_IMAGE_LAYOUT_GENERAL* layouts.

Access types are defined in the same way as for a global memory barrier, but
they only affect the image subresource range identified by image and
subresourceRange, rather than all resources.
srcQueueFamilyIndex, dstQueueFamilyIndex, image, and subresourceRange will
be passed unmodified into a VkImageMemoryBarrier.

An image barrier defining a queue ownership transfer needs to be executed
twice - once by a queue in the source queue family, and then once again by a
queue in the destination queue family, with a semaphore guaranteeing
execution order between them.

If discardContents is set to true, the contents of the image become
undefined after the barrier is executed, which can result in a performance
boost over attempting to preserve the contents.
This is particularly useful for transient images where the contents are
going to be immediately overwritten. A good example of when to use this is
when an application re-uses a presented image after vkAcquireNextImageKHR.
*/
struct ImageBarrier {
    std::span<AccessType> prevAccesses;
    std::span<AccessType> nextAccesses;
    ImageLayout prevLayout;
    ImageLayout nextLayout;
    VkBool32 discardContents;
    uint32_t srcQueueFamilyIndex;
    uint32_t dstQueueFamilyIndex;
    VkImage image;
    VkImageSubresourceRange subresourceRange;
};

/**
Mapping function that translates a set of accesses into the corresponding
pipeline stages, VkAccessFlags, and image layout.
*/
void GetAccessInfo(const std::span<AccessType> &accesses,
                   VkPipelineStageFlags *pStageMask,
                   VkAccessFlags *pAccessMask,
                   VkImageLayout *pImageLayout,
                   bool *pHasWriteAccess);

/**
Mapping function that translates a global barrier into a set of source and
destination pipeline stages, and a VkMemoryBarrier, that can be used with
Vulkan's synchronization methods.
*/
void GetVulkanMemoryBarrier(const GlobalBarrier &thBarrier,
                            VkPipelineStageFlags *pSrcStages,
                            VkPipelineStageFlags *pDstStages,
                            VkMemoryBarrier *pVkBarrier);

/**
Mapping function that translates a buffer barrier into a set of source and
destination pipeline stages, and a VkBufferMemoryBarrier, that can be used
with Vulkan's synchronization methods.
*/
void GetVulkanBufferMemoryBarrier(const BufferBarrier &thBarrier,
                                  VkPipelineStageFlags *pSrcStages,
                                  VkPipelineStageFlags *pDstStages,
                                  VkBufferMemoryBarrier *pVkBarrier);

/**
Mapping function that translates an image barrier into a set of source and
destination pipeline stages, and a VkBufferMemoryBarrier, that can be used
with Vulkan's synchronization methods.
*/
void GetVulkanImageMemoryBarrier(const ImageBarrier &thBarrier,
                                 VkPipelineStageFlags *pSrcStages,
                                 VkPipelineStageFlags *pDstStages,
                                 VkImageMemoryBarrier *pVkBarrier);

/**
Simplified wrapper around vkCmdPipelineBarrier.

The mapping functions defined above are used to translate the passed in
barrier definitions into a set of pipeline stages and native Vulkan memory
barriers to be passed to vkCmdPipelineBarrier.

commandBuffer is passed unmodified to vkCmdPipelineBarrier.
*/
void CmdPipelineBarrier(Magnum::Vk::Device &device,
                        VkCommandBuffer commandBuffer,
                        const GlobalBarrier *pGlobalBarrier,
                        std::span<BufferBarrier> bufferBarriers,
                        std::span<ImageBarrier> imageBarriers);

/**
Wrapper around vkCmdSetEvent.

Sets an event when the accesses defined by pPrevAccesses are completed.

commandBuffer and event are passed unmodified to vkCmdSetEvent.
*/
void CmdSetEvent(Magnum::Vk::Device &device,
                 VkCommandBuffer commandBuffer,
                 VkEvent event,
                 std::span<AccessType> prevAccesses);

/**
Wrapper around vkCmdResetEvent.

Resets an event when the accesses defined by pPrevAccesses are completed.

commandBuffer and event are passed unmodified to vkCmdResetEvent.
*/
void CmdResetEvent(Magnum::Vk::Device &device,
                   VkCommandBuffer commandBuffer,
                   VkEvent event,
                   std::span<AccessType> prevAccesses);

/**
Simplified wrapper around vkCmdWaitEvents.

The mapping functions defined above are used to translate the passed in
barrier definitions into a set of pipeline stages and native Vulkan memory
barriers to be passed to vkCmdPipelineBarrier.

commandBuffer, eventCount, and pEvents are passed unmodified to
vkCmdWaitEvents.
*/
void CmdWaitEvents(Magnum::Vk::Device &device,
                   VkCommandBuffer commandBuffer,
                   std::span<VkEvent> events,
                   const GlobalBarrier *pGlobalBarrier,
                   std::span<BufferBarrier> bufferBarriers,
                   std::span<ImageBarrier> imageBarriers);
} // namespace Cory::Sync