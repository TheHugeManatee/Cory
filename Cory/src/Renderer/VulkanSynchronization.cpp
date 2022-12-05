#include <Cory/Renderer/VulkanSynchronization.hpp>

#include <stdlib.h>

//// Optional Error Checking ////
/*
Checks for barriers defining multiple usages that have different layouts
*/
// #define THSVS_ERROR_CHECK_MIXED_IMAGE_LAYOUT

/*
Checks if an image/buffer barrier is used when a global barrier would suffice
*/
// #define THSVS_ERROR_CHECK_COULD_USE_GLOBAL_BARRIER

/*
Checks if a write access is listed alongside any other access - if so it
points to a potential data hazard that you need to synchronize separately.
In some cases it may simply be over-synchronization however, but it's usually
worth checking.
*/
// #define THSVS_ERROR_CHECK_POTENTIAL_HAZARD

/*
Checks if a variety of table lookups (like the access map) are within
a valid range.
*/
// #define THSVS_ERROR_CHECK_ACCESS_TYPE_IN_RANGE

//// Temporary Memory Allocation ////
/*
Override these if you can't afford the stack space or just want to use a
custom temporary allocator.
These are currently used exclusively to allocate Vulkan memory barriers in
the API, one for each Buffer or Image barrier passed into the pipeline and
event functions.
May consider other allocation strategies in future.
*/

// Alloca inclusion code below copied from
// https://github.com/nothings/stb/blob/master/stb_vorbis.c

// find definition of alloca if it's not in stdlib.h:
#if defined(_MSC_VER) || defined(__MINGW32__)
#include <malloc.h>
#endif
#if defined(__linux__) || defined(__linux) || defined(__EMSCRIPTEN__)
#include <alloca.h>
#endif

#if defined(THSVS_ERROR_CHECK_ACCESS_TYPE_IN_RANGE) ||                                             \
    defined(THSVS_ERROR_CHECK_COULD_USE_GLOBAL_BARRIER) ||                                         \
    defined(THSVS_ERROR_CHECK_MIXED_IMAGE_LAYOUT) || defined(THSVS_ERROR_CHECK_POTENTIAL_HAZARD)
#include <assert.h>
#endif

#if !defined(THSVS_TEMP_ALLOC)
#define THSVS_TEMP_ALLOC(size) (alloca(size))
#endif

#if !defined(THSVS_TEMP_FREE)
#define THSVS_TEMP_FREE(x) ((void)(x))
#endif

typedef struct ThsvsVkAccessInfo {
    VkPipelineStageFlags stageMask;
    VkAccessFlags accessMask;
    VkImageLayout imageLayout;
} ThsvsVkAccessInfo;

const ThsvsVkAccessInfo ThsvsAccessMap[static_cast<uint32_t>(AccessType::NUM_ACCESS_TYPES)] = {
    // THSVS_ACCESS_NONE
    {0, 0, VK_IMAGE_LAYOUT_UNDEFINED},

    // Read Access
    // THSVS_ACCESS_COMMAND_BUFFER_READ_NV
    {VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV,
     VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_NV,
     VK_IMAGE_LAYOUT_UNDEFINED},
    // THSVS_ACCESS_INDIRECT_BUFFER
    {VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
     VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
     VK_IMAGE_LAYOUT_UNDEFINED},

    // THSVS_ACCESS_INDEX_BUFFER
    {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_INDEX_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED},
    // THSVS_ACCESS_VERTEX_BUFFER
    {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
     VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
     VK_IMAGE_LAYOUT_UNDEFINED},
    // THSVS_ACCESS_VERTEX_SHADER_READ_UNIFORM_BUFFER
    {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED},
    // THSVS_ACCESS_VERTEX_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER
    {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
     VK_ACCESS_SHADER_READ_BIT,
     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    // THSVS_ACCESS_VERTEX_SHADER_READ_OTHER
    {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL},

    // THSVS_ACCESS_TESSELLATION_CONTROL_SHADER_READ_UNIFORM_BUFFER
    {VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
     VK_ACCESS_UNIFORM_READ_BIT,
     VK_IMAGE_LAYOUT_UNDEFINED},
    // THSVS_ACCESS_TESSELLATION_CONTROL_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER
    {VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
     VK_ACCESS_SHADER_READ_BIT,
     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    // THSVS_ACCESS_TESSELLATION_CONTROL_SHADER_READ_OTHER
    {VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
     VK_ACCESS_SHADER_READ_BIT,
     VK_IMAGE_LAYOUT_GENERAL},

    // THSVS_ACCESS_TESSELLATION_EVALUATION_SHADER_READ_UNIFORM_BUFFER
    {VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
     VK_ACCESS_UNIFORM_READ_BIT,
     VK_IMAGE_LAYOUT_UNDEFINED},
    // THSVS_ACCESS_TESSELLATION_EVALUATION_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER
    {VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
     VK_ACCESS_SHADER_READ_BIT,
     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    // THSVS_ACCESS_TESSELLATION_EVALUATION_SHADER_READ_OTHER
    {VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
     VK_ACCESS_SHADER_READ_BIT,
     VK_IMAGE_LAYOUT_GENERAL},

    // THSVS_ACCESS_GEOMETRY_SHADER_READ_UNIFORM_BUFFER
    {VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED},
    // THSVS_ACCESS_GEOMETRY_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER
    {VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT,
     VK_ACCESS_SHADER_READ_BIT,
     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    // THSVS_ACCESS_GEOMETRY_SHADER_READ_OTHER
    {VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL},

    // THSVS_ACCESS_TASK_SHADER_READ_UNIFORM_BUFFER_NV
    {VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV, VK_ACCESS_UNIFORM_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED},
    // THSVS_ACCESS_TASK_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER_NV
    {VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV,
     VK_ACCESS_SHADER_READ_BIT,
     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    // THSVS_ACCESS_TASK_SHADER_READ_OTHER_NV
    {VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL},

    // THSVS_ACCESS_MESH_SHADER_READ_UNIFORM_BUFFER_NV
    {VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV, VK_ACCESS_UNIFORM_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED},
    // THSVS_ACCESS_MESH_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER_NV
    {VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV,
     VK_ACCESS_SHADER_READ_BIT,
     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    // THSVS_ACCESS_MESH_SHADER_READ_OTHER_NV
    {VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL},

    // THSVS_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_EXT
    {VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,
     VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT,
     VK_IMAGE_LAYOUT_UNDEFINED},
    // THSVS_ACCESS_FRAGMENT_DENSITY_MAP_READ_EXT
    {VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT,
     VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT,
     VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT},
    // THSVS_ACCESS_SHADING_RATE_READ_NV
    {VK_PIPELINE_STAGE_SHADING_RATE_IMAGE_BIT_NV,
     VK_ACCESS_SHADING_RATE_IMAGE_READ_BIT_NV,
     VK_IMAGE_LAYOUT_SHADING_RATE_OPTIMAL_NV},

    // THSVS_ACCESS_FRAGMENT_SHADER_READ_UNIFORM_BUFFER
    {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED},
    // THSVS_ACCESS_FRAGMENT_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER
    {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
     VK_ACCESS_SHADER_READ_BIT,
     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    // THSVS_ACCESS_FRAGMENT_SHADER_READ_COLOR_INPUT_ATTACHMENT
    {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
     VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    // THSVS_ACCESS_FRAGMENT_SHADER_READ_DEPTH_STENCIL_INPUT_ATTACHMENT
    {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
     VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
     VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL},
    // THSVS_ACCESS_FRAGMENT_SHADER_READ_OTHER
    {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL},
    // THSVS_ACCESS_COLOR_ATTACHMENT_READ
    {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
     VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    // THSVS_ACCESS_COLOR_ATTACHMENT_ADVANCED_BLENDING_EXT
    {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
     VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT,
     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    // THSVS_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ
    {VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
     VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
     VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL},

    // THSVS_ACCESS_COMPUTE_SHADER_READ_UNIFORM_BUFFER
    {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED},
    // THSVS_ACCESS_COMPUTE_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER
    {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
     VK_ACCESS_SHADER_READ_BIT,
     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    // THSVS_ACCESS_COMPUTE_SHADER_READ_OTHER
    {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL},

    // THSVS_ACCESS_ANY_SHADER_READ_UNIFORM_BUFFER
    {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_UNIFORM_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED},
    // THSVS_ACCESS_ANY_SHADER_READ_UNIFORM_BUFFER_OR_VERTEX_BUFFER
    {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
     VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
     VK_IMAGE_LAYOUT_UNDEFINED},
    // THSVS_ACCESS_ANY_SHADER_READ_SAMPLED_IMAGE
    {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
     VK_ACCESS_SHADER_READ_BIT,
     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    // THSVS_ACCESS_ANY_SHADER_READ_OTHER
    {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL},

    // THSVS_ACCESS_TRANSFER_READ
    {VK_PIPELINE_STAGE_TRANSFER_BIT,
     VK_ACCESS_TRANSFER_READ_BIT,
     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL},
    // THSVS_ACCESS_HOST_READ
    {VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT, VK_IMAGE_LAYOUT_GENERAL},
    // THSVS_ACCESS_PRESENT
    {0, 0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
    // THSVS_ACCESS_CONDITIONAL_RENDERING_READ_EXT
    {VK_PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT_EXT,
     VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT,
     VK_IMAGE_LAYOUT_UNDEFINED},

    // THSVS_ACCESS_RAY_TRACING_SHADER_ACCELERATION_STRUCTURE_READ_NV
    {VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV,
     VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV,
     VK_IMAGE_LAYOUT_UNDEFINED},
    // THSVS_ACCESS_ACCELERATION_STRUCTURE_BUILD_READ_NV
    {VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV,
     VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV,
     VK_IMAGE_LAYOUT_UNDEFINED},
    // THSVS_END_OF_READ_ACCESS
    {0, 0, VK_IMAGE_LAYOUT_UNDEFINED},

    // Write access
    // THSVS_ACCESS_COMMAND_BUFFER_WRITE_NV
    {VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV,
     VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV,
     VK_IMAGE_LAYOUT_UNDEFINED},
    // THSVS_ACCESS_VERTEX_SHADER_WRITE
    {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL},
    // THSVS_ACCESS_TESSELLATION_CONTROL_SHADER_WRITE
    {VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
     VK_ACCESS_SHADER_WRITE_BIT,
     VK_IMAGE_LAYOUT_GENERAL},
    // THSVS_ACCESS_TESSELLATION_EVALUATION_SHADER_WRITE
    {VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
     VK_ACCESS_SHADER_WRITE_BIT,
     VK_IMAGE_LAYOUT_GENERAL},
    // THSVS_ACCESS_GEOMETRY_SHADER_WRITE
    {VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL},
    // THSVS_ACCESS_TASK_SHADER_WRITE_NV
    {VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL},
    // THSVS_ACCESS_MESH_SHADER_WRITE_NV
    {VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL},
    // THSVS_ACCESS_TRANSFORM_FEEDBACK_WRITE_EXT
    {VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,
     VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT,
     VK_IMAGE_LAYOUT_UNDEFINED},
    // THSVS_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_EXT
    {VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,
     VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,
     VK_IMAGE_LAYOUT_UNDEFINED},
    // THSVS_ACCESS_FRAGMENT_SHADER_WRITE
    {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL},
    // THSVS_ACCESS_COLOR_ATTACHMENT_WRITE
    {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    // THSVS_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE
    {VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
     VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
     VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
    // THSVS_ACCESS_DEPTH_ATTACHMENT_WRITE_STENCIL_READ_ONLY
    {VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
     VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
     VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR},
    // THSVS_ACCESS_STENCIL_ATTACHMENT_WRITE_DEPTH_READ_ONLY
    {VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
     VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
     VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR},

    // THSVS_ACCESS_COMPUTE_SHADER_WRITE
    {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL},

    // THSVS_ACCESS_ANY_SHADER_WRITE
    {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL},

    // THSVS_ACCESS_TRANSFER_WRITE
    {VK_PIPELINE_STAGE_TRANSFER_BIT,
     VK_ACCESS_TRANSFER_WRITE_BIT,
     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
    // THSVS_ACCESS_HOST_PREINITIALIZED
    {VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_IMAGE_LAYOUT_PREINITIALIZED},
    // THSVS_ACCESS_HOST_WRITE
    {VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL},
    // THSVS_ACCESS_ACCELERATION_STRUCTURE_BUILD_WRITE_NV
    {VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV,
     VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV,
     VK_IMAGE_LAYOUT_UNDEFINED},

    // THSVS_ACCESS_COLOR_ATTACHMENT_READ_WRITE
    {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
     VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    // THSVS_ACCESS_GENERAL
    {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
     VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
     VK_IMAGE_LAYOUT_GENERAL}};

void thsvsGetAccessInfo(uint32_t accessCount,
                        const AccessType *pAccesses,
                        VkPipelineStageFlags *pStageMask,
                        VkAccessFlags *pAccessMask,
                        VkImageLayout *pImageLayout,
                        bool *pHasWriteAccess)
{
    *pStageMask = 0;
    *pAccessMask = 0;
    *pImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    *pHasWriteAccess = false;

    for (uint32_t i = 0; i < accessCount; ++i) {
        AccessType access = pAccesses[i];
        const ThsvsVkAccessInfo *pAccessInfo = &ThsvsAccessMap[access];

#ifdef THSVS_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
        // Asserts that the previous access index is a valid range for the lookup
        assert(access < THSVS_NUM_ACCESS_TYPES);
#endif

#ifdef THSVS_ERROR_CHECK_POTENTIAL_HAZARD
        // Asserts that the access is a read, else it's a write and it should appear on its own.
        assert(access < THSVS_END_OF_READ_ACCESS || accessCount == 1);
#endif

        *pStageMask |= pAccessInfo->stageMask;

        if (access > AccessType::END_OF_READ_ACCESS) *pHasWriteAccess = true;

        *pAccessMask |= pAccessInfo->accessMask;

        VkImageLayout layout = pAccessInfo->imageLayout;

#ifdef THSVS_ERROR_CHECK_MIXED_IMAGE_LAYOUT
        assert(*pImageLayout == VK_IMAGE_LAYOUT_UNDEFINED || *pImageLayout == layout);
#endif

        *pImageLayout = layout;
    }
}

void thsvsGetVulkanMemoryBarrier(const ThsvsGlobalBarrier &thBarrier,
                                 VkPipelineStageFlags *pSrcStages,
                                 VkPipelineStageFlags *pDstStages,
                                 VkMemoryBarrier *pVkBarrier)
{
    *pSrcStages = 0;
    *pDstStages = 0;
    pVkBarrier->sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    pVkBarrier->pNext = NULL;
    pVkBarrier->srcAccessMask = 0;
    pVkBarrier->dstAccessMask = 0;

    for (uint32_t i = 0; i < thBarrier.prevAccessCount; ++i) {
        AccessType prevAccess = thBarrier.pPrevAccesses[i];
        const ThsvsVkAccessInfo *pPrevAccessInfo = &ThsvsAccessMap[prevAccess];

#ifdef THSVS_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
        // Asserts that the previous access index is a valid range for the lookup
        assert(prevAccess < THSVS_NUM_ACCESS_TYPES);
#endif

#ifdef THSVS_ERROR_CHECK_POTENTIAL_HAZARD
        // Asserts that the access is a read, else it's a write and it should appear on its own.
        assert(prevAccess < THSVS_END_OF_READ_ACCESS || thBarrier.prevAccessCount == 1);
#endif

        *pSrcStages |= pPrevAccessInfo->stageMask;

        // Add appropriate availability operations - for writes only.
        if (prevAccess > AccessType::END_OF_READ_ACCESS)
            pVkBarrier->srcAccessMask |= pPrevAccessInfo->accessMask;
    }

    for (uint32_t i = 0; i < thBarrier.nextAccessCount; ++i) {
        AccessType nextAccess = thBarrier.pNextAccesses[i];
        const ThsvsVkAccessInfo *pNextAccessInfo = &ThsvsAccessMap[nextAccess];

#ifdef THSVS_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
        // Asserts that the next access index is a valid range for the lookup
        assert(nextAccess < THSVS_NUM_ACCESS_TYPES);
#endif

#ifdef THSVS_ERROR_CHECK_POTENTIAL_HAZARD
        // Asserts that the access is a read, else it's a write and it should appear on its own.
        assert(nextAccess < THSVS_END_OF_READ_ACCESS || thBarrier.nextAccessCount == 1);
#endif
        *pDstStages |= pNextAccessInfo->stageMask;

        // Add visibility operations as necessary.
        // If the src access mask is zero, this is a WAR hazard (or for some reason a "RAR"),
        // so the dst access mask can be safely zeroed as these don't need visibility.
        if (pVkBarrier->srcAccessMask != 0)
            pVkBarrier->dstAccessMask |= pNextAccessInfo->accessMask;
    }

    // Ensure that the stage masks are valid if no stages were determined
    if (*pSrcStages == 0) *pSrcStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    if (*pDstStages == 0) *pDstStages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
}

void thsvsGetVulkanBufferMemoryBarrier(const ThsvsBufferBarrier &thBarrier,
                                       VkPipelineStageFlags *pSrcStages,
                                       VkPipelineStageFlags *pDstStages,
                                       VkBufferMemoryBarrier *pVkBarrier)
{
    *pSrcStages = 0;
    *pDstStages = 0;
    pVkBarrier->sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    pVkBarrier->pNext = NULL;
    pVkBarrier->srcAccessMask = 0;
    pVkBarrier->dstAccessMask = 0;
    pVkBarrier->srcQueueFamilyIndex = thBarrier.srcQueueFamilyIndex;
    pVkBarrier->dstQueueFamilyIndex = thBarrier.dstQueueFamilyIndex;
    pVkBarrier->buffer = thBarrier.buffer;
    pVkBarrier->offset = thBarrier.offset;
    pVkBarrier->size = thBarrier.size;

#ifdef THSVS_ERROR_CHECK_COULD_USE_GLOBAL_BARRIER
    assert(pVkBarrier->srcQueueFamilyIndex != pVkBarrier->dstQueueFamilyIndex);
#endif

    for (uint32_t i = 0; i < thBarrier.prevAccessCount; ++i) {
        AccessType prevAccess = thBarrier.pPrevAccesses[i];
        const ThsvsVkAccessInfo *pPrevAccessInfo = &ThsvsAccessMap[prevAccess];

#ifdef THSVS_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
        // Asserts that the previous access index is a valid range for the lookup
        assert(prevAccess < THSVS_NUM_ACCESS_TYPES);
#endif

#ifdef THSVS_ERROR_CHECK_POTENTIAL_HAZARD
        // Asserts that the access is a read, else it's a write and it should appear on its own.
        assert(prevAccess < THSVS_END_OF_READ_ACCESS || thBarrier.prevAccessCount == 1);
#endif

        *pSrcStages |= pPrevAccessInfo->stageMask;

        // Add appropriate availability operations - for writes only.
        if (prevAccess > AccessType::END_OF_READ_ACCESS)
            pVkBarrier->srcAccessMask |= pPrevAccessInfo->accessMask;
    }

    for (uint32_t i = 0; i < thBarrier.nextAccessCount; ++i) {
        AccessType nextAccess = thBarrier.pNextAccesses[i];
        const ThsvsVkAccessInfo *pNextAccessInfo = &ThsvsAccessMap[nextAccess];

#ifdef THSVS_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
        // Asserts that the next access index is a valid range for the lookup
        assert(nextAccess < THSVS_NUM_ACCESS_TYPES);
#endif

#ifdef THSVS_ERROR_CHECK_POTENTIAL_HAZARD
        // Asserts that the access is a read, else it's a write and it should appear on its own.
        assert(nextAccess < THSVS_END_OF_READ_ACCESS || thBarrier.nextAccessCount == 1);
#endif

        *pDstStages |= pNextAccessInfo->stageMask;

        // Add visibility operations as necessary.
        // If the src access mask is zero, this is a WAR hazard (or for some reason a "RAR"),
        // so the dst access mask can be safely zeroed as these don't need visibility.
        if (pVkBarrier->srcAccessMask != 0)
            pVkBarrier->dstAccessMask |= pNextAccessInfo->accessMask;
    }

    // Ensure that the stage masks are valid if no stages were determined
    if (*pSrcStages == 0) *pSrcStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    if (*pDstStages == 0) *pDstStages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
}

void thsvsGetVulkanImageMemoryBarrier(const ThsvsImageBarrier &thBarrier,
                                      VkPipelineStageFlags *pSrcStages,
                                      VkPipelineStageFlags *pDstStages,
                                      VkImageMemoryBarrier *pVkBarrier)
{
    *pSrcStages = 0;
    *pDstStages = 0;
    pVkBarrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    pVkBarrier->pNext = NULL;
    pVkBarrier->srcAccessMask = 0;
    pVkBarrier->dstAccessMask = 0;
    pVkBarrier->srcQueueFamilyIndex = thBarrier.srcQueueFamilyIndex;
    pVkBarrier->dstQueueFamilyIndex = thBarrier.dstQueueFamilyIndex;
    pVkBarrier->image = thBarrier.image;
    pVkBarrier->subresourceRange = thBarrier.subresourceRange;
    pVkBarrier->oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    pVkBarrier->newLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    for (uint32_t i = 0; i < thBarrier.prevAccessCount; ++i) {
        AccessType prevAccess = thBarrier.pPrevAccesses[i];
        const ThsvsVkAccessInfo *pPrevAccessInfo = &ThsvsAccessMap[prevAccess];

#ifdef THSVS_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
        // Asserts that the previous access index is a valid range for the lookup
        assert(prevAccess < THSVS_NUM_ACCESS_TYPES);
#endif

#ifdef THSVS_ERROR_CHECK_POTENTIAL_HAZARD
        // Asserts that the access is a read, else it's a write and it should appear on its own.
        assert(prevAccess < THSVS_END_OF_READ_ACCESS || thBarrier.prevAccessCount == 1);
#endif

        *pSrcStages |= pPrevAccessInfo->stageMask;

        // Add appropriate availability operations - for writes only.
        if (prevAccess > AccessType::END_OF_READ_ACCESS)
            pVkBarrier->srcAccessMask |= pPrevAccessInfo->accessMask;

        if (thBarrier.discardContents == VK_TRUE) {
            pVkBarrier->oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        }
        else {
            VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

            switch (thBarrier.prevLayout) {
            case ImageLayout::GENERAL:
                if (prevAccess == AccessType::PRESENT)
                    layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                else
                    layout = VK_IMAGE_LAYOUT_GENERAL;
                break;
            case ImageLayout::OPTIMAL:
                layout = pPrevAccessInfo->imageLayout;
                break;
            case ImageLayout::GENERAL_AND_PRESENTATION:
                layout = VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR;
                break;
            }

#ifdef THSVS_ERROR_CHECK_MIXED_IMAGE_LAYOUT
            assert(pVkBarrier->oldLayout == VK_IMAGE_LAYOUT_UNDEFINED ||
                   pVkBarrier->oldLayout == layout);
#endif
            pVkBarrier->oldLayout = layout;
        }
    }

    for (uint32_t i = 0; i < thBarrier.nextAccessCount; ++i) {
        AccessType nextAccess = thBarrier.pNextAccesses[i];
        const ThsvsVkAccessInfo *pNextAccessInfo = &ThsvsAccessMap[nextAccess];

#ifdef THSVS_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
        // Asserts that the next access index is a valid range for the lookup
        assert(nextAccess < THSVS_NUM_ACCESS_TYPES);
#endif

#ifdef THSVS_ERROR_CHECK_POTENTIAL_HAZARD
        // Asserts that the access is a read, else it's a write and it should appear on its own.
        assert(nextAccess < THSVS_END_OF_READ_ACCESS || thBarrier.nextAccessCount == 1);
#endif

        *pDstStages |= pNextAccessInfo->stageMask;

        // Add visibility operations as necessary.
        // If the src access mask is zero, this is a WAR hazard (or for some reason a "RAR"),
        // so the dst access mask can be safely zeroed as these don't need visibility.
        if (pVkBarrier->srcAccessMask != 0)
            pVkBarrier->dstAccessMask |= pNextAccessInfo->accessMask;

        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        switch (thBarrier.nextLayout) {
        case ImageLayout::GENERAL:
            if (nextAccess == AccessType::PRESENT)
                layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            else
                layout = VK_IMAGE_LAYOUT_GENERAL;
            break;
        case ImageLayout::OPTIMAL:
            layout = pNextAccessInfo->imageLayout;
            break;
        case ImageLayout::GENERAL_AND_PRESENTATION:
            layout = VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR;
            break;
        }

#ifdef THSVS_ERROR_CHECK_MIXED_IMAGE_LAYOUT
        assert(pVkBarrier->newLayout == VK_IMAGE_LAYOUT_UNDEFINED ||
               pVkBarrier->newLayout == layout);
#endif
        pVkBarrier->newLayout = layout;
    }

#ifdef THSVS_ERROR_CHECK_COULD_USE_GLOBAL_BARRIER
    assert(pVkBarrier->newLayout != pVkBarrier->oldLayout ||
           pVkBarrier->srcQueueFamilyIndex != pVkBarrier->dstQueueFamilyIndex);
#endif

    // Ensure that the stage masks are valid if no stages were determined
    if (*pSrcStages == 0) *pSrcStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    if (*pDstStages == 0) *pDstStages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
}

void thsvsCmdPipelineBarrier(VkCommandBuffer commandBuffer,
                             const ThsvsGlobalBarrier *pGlobalBarrier,
                             uint32_t bufferBarrierCount,
                             const ThsvsBufferBarrier *pBufferBarriers,
                             uint32_t imageBarrierCount,
                             const ThsvsImageBarrier *pImageBarriers)
{
    VkMemoryBarrier memoryBarrier;
    // Vulkan pipeline barrier command parameters
    //                     commandBuffer;
    VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    uint32_t memoryBarrierCount = (pGlobalBarrier != NULL) ? 1 : 0;
    VkMemoryBarrier *pMemoryBarriers = (pGlobalBarrier != NULL) ? &memoryBarrier : NULL;
    uint32_t bufferMemoryBarrierCount = bufferBarrierCount;
    VkBufferMemoryBarrier *pBufferMemoryBarriers = NULL;
    uint32_t imageMemoryBarrierCount = imageBarrierCount;
    VkImageMemoryBarrier *pImageMemoryBarriers = NULL;

    // Global memory barrier
    if (pGlobalBarrier != NULL) {
        VkPipelineStageFlags tempSrcStageMask = 0;
        VkPipelineStageFlags tempDstStageMask = 0;
        thsvsGetVulkanMemoryBarrier(
            *pGlobalBarrier, &tempSrcStageMask, &tempDstStageMask, pMemoryBarriers);
        srcStageMask |= tempSrcStageMask;
        dstStageMask |= tempDstStageMask;
    }

    // Buffer memory barriers
    if (bufferBarrierCount > 0) {
        pBufferMemoryBarriers = (VkBufferMemoryBarrier *)THSVS_TEMP_ALLOC(
            sizeof(VkBufferMemoryBarrier) * bufferMemoryBarrierCount);

        VkPipelineStageFlags tempSrcStageMask = 0;
        VkPipelineStageFlags tempDstStageMask = 0;
        for (uint32_t i = 0; i < bufferBarrierCount; ++i) {
            thsvsGetVulkanBufferMemoryBarrier(pBufferBarriers[i],
                                              &tempSrcStageMask,
                                              &tempDstStageMask,
                                              &pBufferMemoryBarriers[i]);
            srcStageMask |= tempSrcStageMask;
            dstStageMask |= tempDstStageMask;
        }
    }

    // Image memory barriers
    if (imageBarrierCount > 0) {
        pImageMemoryBarriers = (VkImageMemoryBarrier *)THSVS_TEMP_ALLOC(
            sizeof(VkImageMemoryBarrier) * imageMemoryBarrierCount);

        VkPipelineStageFlags tempSrcStageMask = 0;
        VkPipelineStageFlags tempDstStageMask = 0;
        for (uint32_t i = 0; i < imageBarrierCount; ++i) {
            thsvsGetVulkanImageMemoryBarrier(
                pImageBarriers[i], &tempSrcStageMask, &tempDstStageMask, &pImageMemoryBarriers[i]);
            srcStageMask |= tempSrcStageMask;
            dstStageMask |= tempDstStageMask;
        }
    }

    vkCmdPipelineBarrier(commandBuffer,
                         srcStageMask,
                         dstStageMask,
                         0,
                         memoryBarrierCount,
                         pMemoryBarriers,
                         bufferMemoryBarrierCount,
                         pBufferMemoryBarriers,
                         imageMemoryBarrierCount,
                         pImageMemoryBarriers);

    THSVS_TEMP_FREE(pBufferMemoryBarriers);
    THSVS_TEMP_FREE(pImageMemoryBarriers);
}

void thsvsCmdSetEvent(VkCommandBuffer commandBuffer,
                      VkEvent event,
                      uint32_t prevAccessCount,
                      const AccessType *pPrevAccesses)
{
    VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    for (uint32_t i = 0; i < prevAccessCount; ++i) {
        AccessType prevAccess = pPrevAccesses[i];
        const ThsvsVkAccessInfo *pPrevAccessInfo = &ThsvsAccessMap[prevAccess];

#ifdef THSVS_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
        // Asserts that the previous access index is a valid range for the lookup
        assert(prevAccess < THSVS_NUM_ACCESS_TYPES);
#endif

        stageMask |= pPrevAccessInfo->stageMask;
    }

    vkCmdSetEvent(commandBuffer, event, stageMask);
}

void thsvsCmdResetEvent(VkCommandBuffer commandBuffer,
                        VkEvent event,
                        uint32_t prevAccessCount,
                        const AccessType *pPrevAccesses)
{
    VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    for (uint32_t i = 0; i < prevAccessCount; ++i) {
        AccessType prevAccess = pPrevAccesses[i];
        const ThsvsVkAccessInfo *pPrevAccessInfo = &ThsvsAccessMap[prevAccess];

#ifdef THSVS_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
        // Asserts that the previous access index is a valid range for the lookup
        assert(prevAccess < THSVS_NUM_ACCESS_TYPES);
#endif

        stageMask |= pPrevAccessInfo->stageMask;
    }

    vkCmdResetEvent(commandBuffer, event, stageMask);
}

void thsvsCmdWaitEvents(VkCommandBuffer commandBuffer,
                        uint32_t eventCount,
                        const VkEvent *pEvents,
                        const ThsvsGlobalBarrier *pGlobalBarrier,
                        uint32_t bufferBarrierCount,
                        const ThsvsBufferBarrier *pBufferBarriers,
                        uint32_t imageBarrierCount,
                        const ThsvsImageBarrier *pImageBarriers)
{
    VkMemoryBarrier memoryBarrier;
    // Vulkan pipeline barrier command parameters
    //                     commandBuffer;
    //                     eventCount;
    //                     pEvents;
    VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    uint32_t memoryBarrierCount = (pGlobalBarrier != NULL) ? 1 : 0;
    VkMemoryBarrier *pMemoryBarriers = (pGlobalBarrier != NULL) ? &memoryBarrier : NULL;
    uint32_t bufferMemoryBarrierCount = bufferBarrierCount;
    VkBufferMemoryBarrier *pBufferMemoryBarriers = NULL;
    uint32_t imageMemoryBarrierCount = imageBarrierCount;
    VkImageMemoryBarrier *pImageMemoryBarriers = NULL;

    // Global memory barrier
    if (pGlobalBarrier != NULL) {
        VkPipelineStageFlags tempSrcStageMask = 0;
        VkPipelineStageFlags tempDstStageMask = 0;
        thsvsGetVulkanMemoryBarrier(
            *pGlobalBarrier, &tempSrcStageMask, &tempDstStageMask, pMemoryBarriers);
        srcStageMask |= tempSrcStageMask;
        dstStageMask |= tempDstStageMask;
    }

    // Buffer memory barriers
    if (bufferBarrierCount > 0) {
        pBufferMemoryBarriers = (VkBufferMemoryBarrier *)THSVS_TEMP_ALLOC(
            sizeof(VkBufferMemoryBarrier) * bufferMemoryBarrierCount);

        VkPipelineStageFlags tempSrcStageMask = 0;
        VkPipelineStageFlags tempDstStageMask = 0;
        for (uint32_t i = 0; i < bufferBarrierCount; ++i) {
            thsvsGetVulkanBufferMemoryBarrier(pBufferBarriers[i],
                                              &tempSrcStageMask,
                                              &tempDstStageMask,
                                              &pBufferMemoryBarriers[i]);
            srcStageMask |= tempSrcStageMask;
            dstStageMask |= tempDstStageMask;
        }
    }

    // Image memory barriers
    if (imageBarrierCount > 0) {
        pImageMemoryBarriers = (VkImageMemoryBarrier *)THSVS_TEMP_ALLOC(
            sizeof(VkImageMemoryBarrier) * imageMemoryBarrierCount);

        VkPipelineStageFlags tempSrcStageMask = 0;
        VkPipelineStageFlags tempDstStageMask = 0;
        for (uint32_t i = 0; i < imageBarrierCount; ++i) {
            thsvsGetVulkanImageMemoryBarrier(
                pImageBarriers[i], &tempSrcStageMask, &tempDstStageMask, &pImageMemoryBarriers[i]);
            srcStageMask |= tempSrcStageMask;
            dstStageMask |= tempDstStageMask;
        }
    }

    vkCmdWaitEvents(commandBuffer,
                    eventCount,
                    pEvents,
                    srcStageMask,
                    dstStageMask,
                    memoryBarrierCount,
                    pMemoryBarriers,
                    bufferMemoryBarrierCount,
                    pBufferMemoryBarriers,
                    imageMemoryBarrierCount,
                    pImageMemoryBarriers);

    THSVS_TEMP_FREE(pBufferMemoryBarriers);
    THSVS_TEMP_FREE(pImageMemoryBarriers);
}
