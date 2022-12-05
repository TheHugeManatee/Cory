#pragma once

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

   #define the symbol THSVS_SIMPLER_VULKAN_SYNCHRONIZATION_IMPLEMENTATION in
   *one* C/C++ file before the #include of this file; the implementation
   will be generated in that file.

VERSION

    alpha.9

    Alpha.9 adds the thsvsGetAccessInfo function to translate access types into a thsvsVkAccessInfo.


VERSION HISTORY

    alpha.8

    Alpha.8 adds a host preinitialization state for linear images, as well as a number of new access sets for extensions released since the last update.

    alpha.7

    Alpha.7 incorporates a number of fixes from @gwihlidal, and fixes
    handling of pipeline stages in the presence of multiple access types or
    barriers in light of other recent changes.

    alpha.6

    Alpha.6 fixes a typo (VK_ACCESS_TYPE_MEMORY_READ|WRITE_BIT should have been VK_ACCESS_MEMORY_READ|WRITE_BIT), and sets the pipeline stage src and dst flag bits to VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT and VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT during initialization, not 0 as per alpha.5

    alpha.5

    Alpha.5 now correctly zeroes out the pipeline stage flags before trying to incrementally set bits on them... common theme here, whoops.

    alpha.4

    Alpha.4 now correctly zeroes out the access types before trying to incrementally set bits on them (!)

    alpha.3

    Alpha.3 changes the following:

    Uniform and vertex buffer access in one enum, matching D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER:
     - THSVS_ACCESS_ANY_SHADER_READ_UNIFORM_BUFFER_OR_VERTEX_BUFFER

    Color read *and* write access, matching D3D12_RESOURCE_STATE_RENDER_TARGET:
     - THSVS_ACCESS_COLOR_ATTACHMENT_READ_WRITE

    Also the "THSVS_ACCESS_*_SHADER_READ_SAMPLED_IMAGE" enums have been renamed to the form "THSVS_ACCESS_*_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER"

    alpha.2

    Alpha.2 adds four new resource states for "ANY SHADER ACCESS":
     - THSVS_ACCESS_ANY_SHADER_READ_UNIFORM_BUFFER
     - THSVS_ACCESS_ANY_SHADER_READ_SAMPLED_IMAGE
     - THSVS_ACCESS_ANY_SHADER_READ_OTHER
     - THSVS_ACCESS_ANY_SHADER_WRITE

    alpha.1

    Alpha.1 adds three new resource states:
     - THSVS_ACCESS_GENERAL (Any access on the device)
     - THSVS_ACCESS_DEPTH_ATTACHMENT_WRITE_STENCIL_READ_ONLY (Write access to only the depth aspect of a depth/stencil attachment)
     - THSVS_ACCESS_STENCIL_ATTACHMENT_WRITE_DEPTH_READ_ONLY (Write access to only the stencil aspect of a depth/stencil attachment)

    It also fixes a couple of typos, and adds clarification as to when extensions need to be enabled to use a feature.

    alpha.0

    This is the very first public release of this library; future revisions
    of this API may change the API in an incompatible manner as feedback is
    received.
    Once the version becomes stable, incompatible changes will only be made
    to major revisions of the API - minor revisions will only contain
    bug fixes or minor additions.

MEMORY ALLOCATION

    The thsvsCmdPipelineBarrier and thWaitEvents commands allocate temporary
    storage for the Vulkan barrier equivalents in order to pass them to the
    respective Vulkan commands.

    These use the `THSVS_TEMP_ALLOC(size)` and `THSVS_TEMP_FREE(x)` macros,
    which are by default set to alloca(size) and ((void)(x)), respectively.
    If you don't want to use stack space or would rather use your own
    allocation strategy, these can be overridden by defining these macros
    in before #include-ing the header file with
    THSVS_SIMPLER_VULKAN_SYNCHRONIZATION_IMPLEMENTATION defined.

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
    However, a number of optional error checks (THSVS_ERROR_CHECK_*) can be
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

    There's a potential pitfall in thsvsCmdPipelineBarrier and thsvsCmdWaitEvents
    where alloca is used for temporary allocations. See MEMORY ALLOCATION
    for more information.

    Testing of this library is so far extremely limited with no immediate
    plans to add to that - so there's bound to be some amount of bugs.
    Please raise these issues on the repo issue tracker, or provide a fix
    via a pull request yourself if you're so inclined.
*/

#include <Magnum/Vk/Vulkan.h>

#include <cstdint>



/*
AccessType defines all potential resource usages in the Vulkan API.
*/
enum AccessType {
    NONE,                                                      // No access. Useful primarily for initialization

    // Read access
    // Requires VK_NV_device_generated_commands to be enabled
    COMMAND_BUFFER_READ_NV,                                    // Command buffer read operation as defined by NV_device_generated_commands
    INDIRECT_BUFFER,                                           // Read as an indirect buffer for drawing or dispatch
    INDEX_BUFFER,                                              // Read as an index buffer for drawing
    VERTEX_BUFFER,                                             // Read as a vertex buffer for drawing
    VERTEX_SHADER_READ_UNIFORM_BUFFER,                         // Read as a uniform buffer in a vertex shader
    VERTEX_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER,  // Read as a sampled image/uniform texel buffer in a vertex shader
    VERTEX_SHADER_READ_OTHER,                                  // Read as any other resource in a vertex shader
    TESSELLATION_CONTROL_SHADER_READ_UNIFORM_BUFFER,           // Read as a uniform buffer in a tessellation control shader
    TESSELLATION_CONTROL_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER, // Read as a sampled image/uniform texel buffer  in a tessellation control shader
    TESSELLATION_CONTROL_SHADER_READ_OTHER,                    // Read as any other resource in a tessellation control shader
    TESSELLATION_EVALUATION_SHADER_READ_UNIFORM_BUFFER,        // Read as a uniform buffer in a tessellation evaluation shader
    TESSELLATION_EVALUATION_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER, // Read as a sampled image/uniform texel buffer in a tessellation evaluation shader
    TESSELLATION_EVALUATION_SHADER_READ_OTHER,                 // Read as any other resource in a tessellation evaluation shader
    GEOMETRY_SHADER_READ_UNIFORM_BUFFER,                       // Read as a uniform buffer in a geometry shader
    GEOMETRY_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER,// Read as a sampled image/uniform texel buffer  in a geometry shader
    GEOMETRY_SHADER_READ_OTHER,                                // Read as any other resource in a geometry shader
    TASK_SHADER_READ_UNIFORM_BUFFER_NV,                        // Read as a uniform buffer in a task shader
    TASK_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER_NV, // Read as a sampled image/uniform texel buffer in a task shader
    TASK_SHADER_READ_OTHER_NV,                                 // Read as any other resource in a task shader
    MESH_SHADER_READ_UNIFORM_BUFFER_NV,                        // Read as a uniform buffer in a mesh shader
    MESH_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER_NV, // Read as a sampled image/uniform texel buffer in a mesh shader
    MESH_SHADER_READ_OTHER_NV,                                 // Read as any other resource in a mesh shader
    TRANSFORM_FEEDBACK_COUNTER_READ_EXT,                       // Read as a transform feedback counter buffer
    FRAGMENT_DENSITY_MAP_READ_EXT,                             // Read as a fragment density map image
    SHADING_RATE_READ_NV,                                      // Read as a shading rate image
    FRAGMENT_SHADER_READ_UNIFORM_BUFFER,                       // Read as a uniform buffer in a fragment shader
    FRAGMENT_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER,// Read as a sampled image/uniform texel buffer  in a fragment shader
    FRAGMENT_SHADER_READ_COLOR_INPUT_ATTACHMENT,               // Read as an input attachment with a color format in a fragment shader
    FRAGMENT_SHADER_READ_DEPTH_STENCIL_INPUT_ATTACHMENT,       // Read as an input attachment with a depth/stencil format in a fragment shader
    FRAGMENT_SHADER_READ_OTHER,                                // Read as any other resource in a fragment shader
    COLOR_ATTACHMENT_READ,                                     // Read by standard blending/logic operations or subpass load operations
    COLOR_ATTACHMENT_ADVANCED_BLENDING_EXT,                    // Read by advanced blending, standard blending, logic operations, or subpass load operations
    DEPTH_STENCIL_ATTACHMENT_READ,                             // Read by depth/stencil tests or subpass load operations
    COMPUTE_SHADER_READ_UNIFORM_BUFFER,                        // Read as a uniform buffer in a compute shader
    COMPUTE_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER, // Read as a sampled image/uniform texel buffer in a compute shader
    COMPUTE_SHADER_READ_OTHER,                                 // Read as any other resource in a compute shader
    ANY_SHADER_READ_UNIFORM_BUFFER,                            // Read as a uniform buffer in any shader
    ANY_SHADER_READ_UNIFORM_BUFFER_OR_VERTEX_BUFFER,           // Read as a uniform buffer in any shader, or a vertex buffer
    ANY_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER,     // Read as a sampled image in any shader
    ANY_SHADER_READ_OTHER,                                     // Read as any other resource (excluding attachments) in any shader
    TRANSFER_READ,                                             // Read as the source of a transfer operation
    HOST_READ,                                                 // Read on the host

    // Requires VK_KHR_swapchain to be enabled
    PRESENT,                                                   // Read by the presentation engine (i.e. vkQueuePresentKHR)

    // Requires VK_EXT_conditional_rendering to be enabled
    CONDITIONAL_RENDERING_READ_EXT,                            // Read by conditional rendering

    // Requires VK_NV_ray_tracing to be enabled
    RAY_TRACING_SHADER_ACCELERATION_STRUCTURE_READ_NV,         // Read by a ray tracing shader as an acceleration structure
    ACCELERATION_STRUCTURE_BUILD_READ_NV,                      // Read as an acceleration structure during a build

    // Read accesses end
    END_OF_READ_ACCESS,

    // Write access
    // Requires VK_NV_device_generated_commands to be enabled
    COMMAND_BUFFER_WRITE_NV,                                   // Command buffer write operation
    VERTEX_SHADER_WRITE,                                       // Written as any resource in a vertex shader
    TESSELLATION_CONTROL_SHADER_WRITE,                         // Written as any resource in a tessellation control shader
    TESSELLATION_EVALUATION_SHADER_WRITE,                      // Written as any resource in a tessellation evaluation shader
    GEOMETRY_SHADER_WRITE,                                     // Written as any resource in a geometry shader

    // Requires VK_NV_mesh_shading to be enabled
    TASK_SHADER_WRITE_NV,                                      // Written as any resource in a task shader
    MESH_SHADER_WRITE_NV,                                      // Written as any resource in a mesh shader

    // Requires VK_EXT_transform_feedback to be enabled
    TRANSFORM_FEEDBACK_WRITE_EXT,                              // Written as a transform feedback buffer
    TRANSFORM_FEEDBACK_COUNTER_WRITE_EXT,                      // Written as a transform feedback counter buffer

    FRAGMENT_SHADER_WRITE,                                     // Written as any resource in a fragment shader
    COLOR_ATTACHMENT_WRITE,                                    // Written as a color attachment during rendering, or via a subpass store op
    DEPTH_STENCIL_ATTACHMENT_WRITE,                            // Written as a depth/stencil attachment during rendering, or via a subpass store op

    // Requires VK_KHR_maintenance2 to be enabled
    DEPTH_ATTACHMENT_WRITE_STENCIL_READ_ONLY,                  // Written as a depth aspect of a depth/stencil attachment during rendering, whilst the stencil aspect is read-only
    STENCIL_ATTACHMENT_WRITE_DEPTH_READ_ONLY,                  // Written as a stencil aspect of a depth/stencil attachment during rendering, whilst the depth aspect is read-only

    COMPUTE_SHADER_WRITE,                                      // Written as any resource in a compute shader
    ANY_SHADER_WRITE,                                          // Written as any resource in any shader
    TRANSFER_WRITE,                                            // Written as the destination of a transfer operation
    HOST_PREINITIALIZED,                                       // Data pre-filled by host before device access starts
    HOST_WRITE,                                                // Written on the host

    // Requires VK_NV_ray_tracing to be enabled
    ACCELERATION_STRUCTURE_BUILD_WRITE_NV,                     // Written as an acceleration structure during a build

    COLOR_ATTACHMENT_READ_WRITE,                               // Read or written as a color attachment during rendering
                                              // General access
    GENERAL,                                                   // Covers any access - useful for debug, generally avoid for performance reasons

    // Number of access types
    NUM_ACCESS_TYPES
};

/*
ImageLayout defines a handful of layout options for images.
Rather than a list of all possible image layouts, this reduced list is
correlated with the access types to map to the correct Vulkan layouts.
THSVS_IMAGE_LAYOUT_OPTIMAL is usually preferred.
*/
enum ImageLayout {
    OPTIMAL,                 // Choose the most optimal layout for each usage. Performs layout transitions as appropriate for the access.
    GENERAL,                 // Layout accessible by all Vulkan access types on a device - no layout transitions except for presentation

    // Requires VK_KHR_shared_presentable_image to be enabled. Can only be used for shared presentable images (i.e. single-buffered swap chains).
    GENERAL_AND_PRESENTATION // As GENERAL, but also allows presentation engines to access it - no layout transitions
};

/*
Global barriers define a set of accesses on multiple resources at once.
If a buffer or image doesn't require a queue ownership transfer, or an image
doesn't require a layout transition (e.g. you're using one of the GENERAL
layouts) then a global barrier should be preferred.
Simply define the previous and next access types of resources affected.
*/
struct ThsvsGlobalBarrier {
    uint32_t                prevAccessCount;
    const AccessType*  pPrevAccesses;
    uint32_t                nextAccessCount;
    const AccessType*  pNextAccesses;
};

/*
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
struct ThsvsBufferBarrier {
    uint32_t                prevAccessCount;
    const AccessType*  pPrevAccesses;
    uint32_t                nextAccessCount;
    const AccessType*  pNextAccesses;
    uint32_t                srcQueueFamilyIndex;
    uint32_t                dstQueueFamilyIndex;
    VkBuffer                buffer;
    VkDeviceSize            offset;
    VkDeviceSize            size;
};

/*
Image barriers should only be used when a queue family ownership transfer
or an image layout transition is required - prefer global barriers at all
other times.
In general it is better to use image barriers with THSVS_IMAGE_LAYOUT_OPTIMAL
than it is to use global barriers with images using either of the
THSVS_IMAGE_LAYOUT_GENERAL* layouts.

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
typedef struct ThsvsImageBarrier {
    uint32_t                prevAccessCount;
    const AccessType*  pPrevAccesses;
    uint32_t                nextAccessCount;
    const AccessType*  pNextAccesses;
    ImageLayout        prevLayout;
    ImageLayout        nextLayout;
    VkBool32                discardContents;
    uint32_t                srcQueueFamilyIndex;
    uint32_t                dstQueueFamilyIndex;
    VkImage                 image;
    VkImageSubresourceRange subresourceRange;
} ThsvsImageBarrier;

/*
Mapping function that translates a set of accesses into the corresponding
pipeline stages, VkAccessFlags, and image layout.
*/
void thsvsGetAccessInfo(
    uint32_t               accessCount,
    const AccessType* pAccesses,
    VkPipelineStageFlags*  pStageMask,
    VkAccessFlags*         pAccessMask,
    VkImageLayout*         pImageLayout,
    bool*                  pHasWriteAccess);

/*
Mapping function that translates a global barrier into a set of source and
destination pipeline stages, and a VkMemoryBarrier, that can be used with
Vulkan's synchronization methods.
*/
void thsvsGetVulkanMemoryBarrier(
    const ThsvsGlobalBarrier& thBarrier,
    VkPipelineStageFlags*     pSrcStages,
    VkPipelineStageFlags*     pDstStages,
    VkMemoryBarrier*          pVkBarrier);

/*
Mapping function that translates a buffer barrier into a set of source and
destination pipeline stages, and a VkBufferMemoryBarrier, that can be used
with Vulkan's synchronization methods.
*/
void thsvsGetVulkanBufferMemoryBarrier(
    const ThsvsBufferBarrier& thBarrier,
    VkPipelineStageFlags*     pSrcStages,
    VkPipelineStageFlags*     pDstStages,
    VkBufferMemoryBarrier*    pVkBarrier);

/*
Mapping function that translates an image barrier into a set of source and
destination pipeline stages, and a VkBufferMemoryBarrier, that can be used
with Vulkan's synchronization methods.
*/
void thsvsGetVulkanImageMemoryBarrier(
    const ThsvsImageBarrier& thBarrier,
    VkPipelineStageFlags*    pSrcStages,
    VkPipelineStageFlags*    pDstStages,
    VkImageMemoryBarrier*    pVkBarrier);

/*
Simplified wrapper around vkCmdPipelineBarrier.

The mapping functions defined above are used to translate the passed in
barrier definitions into a set of pipeline stages and native Vulkan memory
barriers to be passed to vkCmdPipelineBarrier.

commandBuffer is passed unmodified to vkCmdPipelineBarrier.
*/
void thsvsCmdPipelineBarrier(
    VkCommandBuffer           commandBuffer,
    const ThsvsGlobalBarrier* pGlobalBarrier,
    uint32_t                  bufferBarrierCount,
    const ThsvsBufferBarrier* pBufferBarriers,
    uint32_t                  imageBarrierCount,
    const ThsvsImageBarrier*  pImageBarriers);

/*
Wrapper around vkCmdSetEvent.

Sets an event when the accesses defined by pPrevAccesses are completed.

commandBuffer and event are passed unmodified to vkCmdSetEvent.
*/
void thsvsCmdSetEvent(
    VkCommandBuffer           commandBuffer,
    VkEvent                   event,
    uint32_t                  prevAccessCount,
    const AccessType*    pPrevAccesses);

/*
Wrapper around vkCmdResetEvent.

Resets an event when the accesses defined by pPrevAccesses are completed.

commandBuffer and event are passed unmodified to vkCmdResetEvent.
*/
void thsvsCmdResetEvent(
    VkCommandBuffer           commandBuffer,
    VkEvent                   event,
    uint32_t                  prevAccessCount,
    const AccessType*    pPrevAccesses);

/*
Simplified wrapper around vkCmdWaitEvents.

The mapping functions defined above are used to translate the passed in
barrier definitions into a set of pipeline stages and native Vulkan memory
barriers to be passed to vkCmdPipelineBarrier.

commandBuffer, eventCount, and pEvents are passed unmodified to
vkCmdWaitEvents.
*/
void thsvsCmdWaitEvents(
    VkCommandBuffer           commandBuffer,
    uint32_t                  eventCount,
    const VkEvent*            pEvents,
    const ThsvsGlobalBarrier* pGlobalBarrier,
    uint32_t                  bufferBarrierCount,
    const ThsvsBufferBarrier* pBufferBarriers,
    uint32_t                  imageBarrierCount,
    const ThsvsImageBarrier*  pImageBarriers);
