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

#include <Cory/Renderer/Synchronization.hpp>

#include <Magnum/Vk/Device.h>
#include <array>
#include <gsl/narrow>

//// Optional Error Checking ////

#ifdef _DEBUG
/*
Checks for barriers defining multiple usages that have different layouts
*/
#define SYNC_ERROR_CHECK_MIXED_IMAGE_LAYOUT

/*
Checks if an image/buffer barrier is used when a global barrier would suffice
*/
//#define SYNC_ERROR_CHECK_COULD_USE_GLOBAL_BARRIER

/*
Checks if a write access is listed alongside any other access - if so it
points to a potential data hazard that you need to synchronize separately.
In some cases it may simply be over-synchronization however, but it's usually
worth checking.
*/
#define SYNC_ERROR_CHECK_POTENTIAL_HAZARD

/*
Checks if a variety of table lookups (like the access map) are within
a valid range.
*/
#define SYNC_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
#endif
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

#if defined(SYNC_ERROR_CHECK_ACCESS_TYPE_IN_RANGE) ||                                              \
    defined(SYNC_ERROR_CHECK_COULD_USE_GLOBAL_BARRIER) ||                                          \
    defined(SYNC_ERROR_CHECK_MIXED_IMAGE_LAYOUT) || defined(SYNC_ERROR_CHECK_POTENTIAL_HAZARD)
#include <assert.h>
#endif

#if !defined(SYNC_TEMP_ALLOC)
#define SYNC_TEMP_ALLOC(size) (alloca(size))
#endif

#if !defined(SYNC_TEMP_FREE)
#define SYNC_TEMP_FREE(x) ((void)(x))
#endif

namespace Cory::Sync {

struct AccessInfo {
    VkPipelineStageFlags stageMask;
    VkAccessFlags accessMask;
    VkImageLayout imageLayout;
};

constexpr std::array<AccessInfo, static_cast<uint32_t>(AccessType::NUM_ACCESS_TYPES)> AccessMap{
    {// _ACCESS_NONE
     {0, 0, VK_IMAGE_LAYOUT_UNDEFINED},

     // Read Access
     //    // _ACCESS_COMMAND_BUFFER_READ_NV
     //    {VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV,
     //     VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_NV,
     //     VK_IMAGE_LAYOUT_UNDEFINED},
     // _ACCESS_INDIRECT_BUFFER
     {VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
      VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
      VK_IMAGE_LAYOUT_UNDEFINED},

     // _ACCESS_INDEX_BUFFER
     {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_INDEX_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED},
     // _ACCESS_VERTEX_BUFFER
     {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
      VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
      VK_IMAGE_LAYOUT_UNDEFINED},
     // _ACCESS_VERTEX_SHADER_READ_UNIFORM_BUFFER
     {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED},
     // _ACCESS_VERTEX_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER
     {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
      VK_ACCESS_SHADER_READ_BIT,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
     // _ACCESS_VERTEX_SHADER_READ_OTHER
     {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL},

     // _ACCESS_TESSELLATION_CONTROL_SHADER_READ_UNIFORM_BUFFER
     {VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
      VK_ACCESS_UNIFORM_READ_BIT,
      VK_IMAGE_LAYOUT_UNDEFINED},
     // _ACCESS_TESSELLATION_CONTROL_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER
     {VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
      VK_ACCESS_SHADER_READ_BIT,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
     // _ACCESS_TESSELLATION_CONTROL_SHADER_READ_OTHER
     {VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
      VK_ACCESS_SHADER_READ_BIT,
      VK_IMAGE_LAYOUT_GENERAL},

     // _ACCESS_TESSELLATION_EVALUATION_SHADER_READ_UNIFORM_BUFFER
     {VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
      VK_ACCESS_UNIFORM_READ_BIT,
      VK_IMAGE_LAYOUT_UNDEFINED},
     // _ACCESS_TESSELLATION_EVALUATION_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER
     {VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
      VK_ACCESS_SHADER_READ_BIT,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
     // _ACCESS_TESSELLATION_EVALUATION_SHADER_READ_OTHER
     {VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
      VK_ACCESS_SHADER_READ_BIT,
      VK_IMAGE_LAYOUT_GENERAL},

     // _ACCESS_GEOMETRY_SHADER_READ_UNIFORM_BUFFER
     {VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED},
     // _ACCESS_GEOMETRY_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER
     {VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT,
      VK_ACCESS_SHADER_READ_BIT,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
     // _ACCESS_GEOMETRY_SHADER_READ_OTHER
     {VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL},

     //    // _ACCESS_TASK_SHADER_READ_UNIFORM_BUFFER_NV
     //    {VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV, VK_ACCESS_UNIFORM_READ_BIT,
     //    VK_IMAGE_LAYOUT_UNDEFINED},
     //    // _ACCESS_TASK_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER_NV
     //    {VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV,
     //     VK_ACCESS_SHADER_READ_BIT,
     //     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
     //    // _ACCESS_TASK_SHADER_READ_OTHER_NV
     //    {VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV, VK_ACCESS_SHADER_READ_BIT,
     //    VK_IMAGE_LAYOUT_GENERAL},
     //
     //    // _ACCESS_MESH_SHADER_READ_UNIFORM_BUFFER_NV
     //    {VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV, VK_ACCESS_UNIFORM_READ_BIT,
     //    VK_IMAGE_LAYOUT_UNDEFINED},
     //    // _ACCESS_MESH_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER_NV
     //    {VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV,
     //     VK_ACCESS_SHADER_READ_BIT,
     //     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
     //    // _ACCESS_MESH_SHADER_READ_OTHER_NV
     //    {VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV, VK_ACCESS_SHADER_READ_BIT,
     //    VK_IMAGE_LAYOUT_GENERAL},
     //
     //    // _ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_EXT
     //    {VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,
     //     VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT,
     //     VK_IMAGE_LAYOUT_UNDEFINED},
     //    // _ACCESS_FRAGMENT_DENSITY_MAP_READ_EXT
     //    {VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT,
     //     VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT,
     //     VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT},
     //    // _ACCESS_SHADING_RATE_READ_NV
     //    {VK_PIPELINE_STAGE_SHADING_RATE_IMAGE_BIT_NV,
     //     VK_ACCESS_SHADING_RATE_IMAGE_READ_BIT_NV,
     //     VK_IMAGE_LAYOUT_SHADING_RATE_OPTIMAL_NV},

     // _ACCESS_FRAGMENT_SHADER_READ_UNIFORM_BUFFER
     {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED},
     // _ACCESS_FRAGMENT_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER
     {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      VK_ACCESS_SHADER_READ_BIT,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
     // _ACCESS_FRAGMENT_SHADER_READ_COLOR_INPUT_ATTACHMENT
     {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
     // _ACCESS_FRAGMENT_SHADER_READ_DEPTH_STENCIL_INPUT_ATTACHMENT
     {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL},
     // _ACCESS_FRAGMENT_SHADER_READ_OTHER
     {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL},
     // _ACCESS_COLOR_ATTACHMENT_READ
     {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
     //    // _ACCESS_COLOR_ATTACHMENT_ADVANCED_BLENDING_EXT
     //    {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
     //     VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT,
     //     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
     // _ACCESS_DEPTH_STENCIL_ATTACHMENT_READ
     {VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL},

     // _ACCESS_COMPUTE_SHADER_READ_UNIFORM_BUFFER
     {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED},
     // _ACCESS_COMPUTE_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER
     {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_ACCESS_SHADER_READ_BIT,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
     // _ACCESS_COMPUTE_SHADER_READ_OTHER
     {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL},

     // _ACCESS_ANY_SHADER_READ_UNIFORM_BUFFER
     {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_UNIFORM_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED},
     // _ACCESS_ANY_SHADER_READ_UNIFORM_BUFFER_OR_VERTEX_BUFFER
     {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
      VK_IMAGE_LAYOUT_UNDEFINED},
     // _ACCESS_ANY_SHADER_READ_SAMPLED_IMAGE
     {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      VK_ACCESS_SHADER_READ_BIT,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
     // _ACCESS_ANY_SHADER_READ_OTHER
     {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL},

     // _ACCESS_TRANSFER_READ
     {VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_READ_BIT,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL},
     // _ACCESS_HOST_READ
     {VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT, VK_IMAGE_LAYOUT_GENERAL},
     // _ACCESS_PRESENT
     {0, 0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
     //    // _ACCESS_CONDITIONAL_RENDERING_READ_EXT
     //    {VK_PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT_EXT,
     //     VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT,
     //     VK_IMAGE_LAYOUT_UNDEFINED},
     //
     //    // _ACCESS_RAY_TRACING_SHADER_ACCELERATION_STRUCTURE_READ_NV
     //    {VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV,
     //     VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV,
     //     VK_IMAGE_LAYOUT_UNDEFINED},
     //    // _ACCESS_ACCELERATION_STRUCTURE_BUILD_READ_NV
     //    {VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV,
     //     VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV,
     //     VK_IMAGE_LAYOUT_UNDEFINED},
     // END_OF_READ_ACCESS
     {0, 0, VK_IMAGE_LAYOUT_UNDEFINED},

     // Write access
     //    // _ACCESS_COMMAND_BUFFER_WRITE_NV
     //    {VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV,
     //     VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV,
     //     VK_IMAGE_LAYOUT_UNDEFINED},
     // _ACCESS_VERTEX_SHADER_WRITE
     {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL},
     // _ACCESS_TESSELLATION_CONTROL_SHADER_WRITE
     {VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
      VK_ACCESS_SHADER_WRITE_BIT,
      VK_IMAGE_LAYOUT_GENERAL},
     // _ACCESS_TESSELLATION_EVALUATION_SHADER_WRITE
     {VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
      VK_ACCESS_SHADER_WRITE_BIT,
      VK_IMAGE_LAYOUT_GENERAL},
     // _ACCESS_GEOMETRY_SHADER_WRITE
     {VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL},
     //    // _ACCESS_TASK_SHADER_WRITE_NV
     //    {VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV, VK_ACCESS_SHADER_WRITE_BIT,
     //    VK_IMAGE_LAYOUT_GENERAL},
     //    // _ACCESS_MESH_SHADER_WRITE_NV
     //    {VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV, VK_ACCESS_SHADER_WRITE_BIT,
     //    VK_IMAGE_LAYOUT_GENERAL},
     //    // _ACCESS_TRANSFORM_FEEDBACK_WRITE_EXT
     //    {VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,
     //     VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT,
     //     VK_IMAGE_LAYOUT_UNDEFINED},
     //    // _ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_EXT
     //    {VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,
     //     VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,
     //     VK_IMAGE_LAYOUT_UNDEFINED},
     // _ACCESS_FRAGMENT_SHADER_WRITE
     {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL},
     // _ACCESS_COLOR_ATTACHMENT_WRITE
     {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
     // _ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE
     {VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
     // _ACCESS_DEPTH_ATTACHMENT_WRITE_STENCIL_READ_ONLY
     {VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
      VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR},
     // _ACCESS_STENCIL_ATTACHMENT_WRITE_DEPTH_READ_ONLY
     {VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
      VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR},

     // _ACCESS_COMPUTE_SHADER_WRITE
     {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL},

     // _ACCESS_ANY_SHADER_WRITE
     {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL},

     // _ACCESS_TRANSFER_WRITE
     {VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
     // _ACCESS_HOST_PREINITIALIZED
     {VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_IMAGE_LAYOUT_PREINITIALIZED},
     // _ACCESS_HOST_WRITE
     {VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL},
     //    // _ACCESS_ACCELERATION_STRUCTURE_BUILD_WRITE_NV
     //    {VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV,
     //     VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV,
     //     VK_IMAGE_LAYOUT_UNDEFINED},

     // _ACCESS_COLOR_ATTACHMENT_READ_WRITE
     {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
     // _ACCESS_GENERAL
     {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
      VK_IMAGE_LAYOUT_GENERAL}}};

// spot-check that we did not go out of sync when we deleted/commented away some of the entries
static_assert(AccessMap[static_cast<uint32_t>(AccessType::END_OF_READ_ACCESS)].stageMask == 0 &&
              AccessMap[static_cast<uint32_t>(AccessType::END_OF_READ_ACCESS)].accessMask == 0 &&
              AccessMap[static_cast<uint32_t>(AccessType::END_OF_READ_ACCESS)].imageLayout ==
                  VK_IMAGE_LAYOUT_UNDEFINED);
static_assert(AccessMap.back().stageMask == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT &&
              AccessMap.back().accessMask ==
                  (VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT) &&
              AccessMap.back().imageLayout == VK_IMAGE_LAYOUT_GENERAL);


VkImageLayout GetVkImageLayout(AccessType access)
{
    return AccessMap[static_cast<uint32_t>(access)].imageLayout;
}

void GetAccessInfo(std::span<const AccessType> accesses,
                   VkPipelineStageFlags *pStageMask,
                   VkAccessFlags *pAccessMask,
                   VkImageLayout *pImageLayout,
                   bool *pHasWriteAccess)
{
    *pStageMask = 0;
    *pAccessMask = 0;
    *pImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    *pHasWriteAccess = false;

    for (uint32_t i = 0; i < accesses.size(); ++i) {
        AccessType access = accesses[i];
        const AccessInfo *pAccessInfo = &AccessMap[static_cast<uint32_t>(access)];

#ifdef SYNC_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
        // Asserts that the previous access index is a valid range for the lookup
        assert(access < AccessType::NUM_ACCESS_TYPES);
#endif

#ifdef SYNC_ERROR_CHECK_POTENTIAL_HAZARD
        // Asserts that the access is a read, else it's a write and it should appear on its own.
        assert(access < AccessType::END_OF_READ_ACCESS || accesses.size() == 1);
#endif

        *pStageMask |= pAccessInfo->stageMask;

        if (access > AccessType::END_OF_READ_ACCESS) *pHasWriteAccess = true;

        *pAccessMask |= pAccessInfo->accessMask;

        VkImageLayout layout = pAccessInfo->imageLayout;

#ifdef SYNC_ERROR_CHECK_MIXED_IMAGE_LAYOUT
        assert(*pImageLayout == VK_IMAGE_LAYOUT_UNDEFINED || *pImageLayout == layout);
#endif

        *pImageLayout = layout;
    }
}

void GetVulkanMemoryBarrier(const GlobalBarrier &thBarrier,
                            VkPipelineStageFlags *pSrcStages,
                            VkPipelineStageFlags *pDstStages,
                            VkMemoryBarrier *pVkBarrier)
{
    *pSrcStages = 0;
    *pDstStages = 0;
    pVkBarrier->sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    pVkBarrier->pNext = nullptr;
    pVkBarrier->srcAccessMask = 0;
    pVkBarrier->dstAccessMask = 0;

    for (uint32_t i = 0; i < thBarrier.prevAccesses.size(); ++i) {
        AccessType prevAccess = thBarrier.prevAccesses[i];
        const AccessInfo *pPrevAccessInfo = &AccessMap[static_cast<uint32_t>(prevAccess)];

#ifdef SYNC_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
        // Asserts that the previous access index is a valid range for the lookup
        assert(prevAccess < AccessType::NUM_ACCESS_TYPES);
#endif

#ifdef SYNC_ERROR_CHECK_POTENTIAL_HAZARD
        // Asserts that the access is a read, else it's a write and it should appear on its own.
        assert(prevAccess < AccessType::END_OF_READ_ACCESS || thBarrier.prevAccesses.size() == 1);
#endif

        *pSrcStages |= pPrevAccessInfo->stageMask;

        // Add appropriate availability operations - for writes only.
        if (prevAccess > AccessType::END_OF_READ_ACCESS)
            pVkBarrier->srcAccessMask |= pPrevAccessInfo->accessMask;
    }

    for (uint32_t i = 0; i < thBarrier.nextAccesses.size(); ++i) {
        AccessType nextAccess = thBarrier.nextAccesses[i];
        const AccessInfo *pNextAccessInfo = &AccessMap[static_cast<uint32_t>(nextAccess)];

#ifdef SYNC_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
        // Asserts that the next access index is a valid range for the lookup
        assert(nextAccess < AccessType::NUM_ACCESS_TYPES);
#endif

#ifdef SYNC_ERROR_CHECK_POTENTIAL_HAZARD
        // Asserts that the access is a read, else it's a write and it should appear on its own.
        assert(nextAccess < AccessType::END_OF_READ_ACCESS || thBarrier.nextAccesses.size() == 1);
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

void GetVulkanBufferMemoryBarrier(const BufferBarrier &thBarrier,
                                  VkPipelineStageFlags *pSrcStages,
                                  VkPipelineStageFlags *pDstStages,
                                  VkBufferMemoryBarrier *pVkBarrier)
{
    *pSrcStages = 0;
    *pDstStages = 0;
    pVkBarrier->sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    pVkBarrier->pNext = nullptr;
    pVkBarrier->srcAccessMask = 0;
    pVkBarrier->dstAccessMask = 0;
    pVkBarrier->srcQueueFamilyIndex = thBarrier.srcQueueFamilyIndex;
    pVkBarrier->dstQueueFamilyIndex = thBarrier.dstQueueFamilyIndex;
    pVkBarrier->buffer = thBarrier.buffer;
    pVkBarrier->offset = thBarrier.offset;
    pVkBarrier->size = thBarrier.size;

#ifdef SYNC_ERROR_CHECK_COULD_USE_GLOBAL_BARRIER
    assert(pVkBarrier->srcQueueFamilyIndex != pVkBarrier->dstQueueFamilyIndex);
#endif

    for (uint32_t i = 0; i < thBarrier.prevAccesses.size(); ++i) {
        AccessType prevAccess = thBarrier.prevAccesses[i];
        const AccessInfo *pPrevAccessInfo = &AccessMap[static_cast<uint32_t>(prevAccess)];

#ifdef SYNC_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
        // Asserts that the previous access index is a valid range for the lookup
        assert(prevAccess < AccessType::NUM_ACCESS_TYPES);
#endif

#ifdef SYNC_ERROR_CHECK_POTENTIAL_HAZARD
        // Asserts that the access is a read, else it's a write and it should appear on its own.
        assert(prevAccess < AccessType::END_OF_READ_ACCESS || thBarrier.prevAccesses.size() == 1);
#endif

        *pSrcStages |= pPrevAccessInfo->stageMask;

        // Add appropriate availability operations - for writes only.
        if (prevAccess > AccessType::END_OF_READ_ACCESS)
            pVkBarrier->srcAccessMask |= pPrevAccessInfo->accessMask;
    }

    for (uint32_t i = 0; i < thBarrier.nextAccesses.size(); ++i) {
        AccessType nextAccess = thBarrier.nextAccesses[i];
        const AccessInfo *pNextAccessInfo = &AccessMap[static_cast<uint32_t>(nextAccess)];

#ifdef SYNC_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
        // Asserts that the next access index is a valid range for the lookup
        assert(nextAccess < AccessType::NUM_ACCESS_TYPES);
#endif

#ifdef SYNC_ERROR_CHECK_POTENTIAL_HAZARD
        // Asserts that the access is a read, else it's a write and it should appear on its own.
        assert(nextAccess < AccessType::END_OF_READ_ACCESS || thBarrier.nextAccesses.size() == 1);
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

void GetVulkanImageMemoryBarrier(const ImageBarrier &thBarrier,
                                 VkPipelineStageFlags *pSrcStages,
                                 VkPipelineStageFlags *pDstStages,
                                 VkImageMemoryBarrier *pVkBarrier)
{
    *pSrcStages = 0;
    *pDstStages = 0;
    pVkBarrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    pVkBarrier->pNext = nullptr;
    pVkBarrier->srcAccessMask = 0;
    pVkBarrier->dstAccessMask = 0;
    pVkBarrier->srcQueueFamilyIndex = thBarrier.srcQueueFamilyIndex;
    pVkBarrier->dstQueueFamilyIndex = thBarrier.dstQueueFamilyIndex;
    pVkBarrier->image = thBarrier.image;
    pVkBarrier->subresourceRange = thBarrier.subresourceRange;
    pVkBarrier->oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    pVkBarrier->newLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    for (uint32_t i = 0; i < thBarrier.prevAccesses.size(); ++i) {
        AccessType prevAccess = thBarrier.prevAccesses[i];
        const AccessInfo *pPrevAccessInfo = &AccessMap[static_cast<uint32_t>(prevAccess)];

#ifdef SYNC_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
        // Asserts that the previous access index is a valid range for the lookup
        assert(prevAccess < AccessType::NUM_ACCESS_TYPES);
#endif

#ifdef SYNC_ERROR_CHECK_POTENTIAL_HAZARD
        // Asserts that the access is a read, else it's a write and it should appear on its own.
        assert(prevAccess < AccessType::END_OF_READ_ACCESS || thBarrier.prevAccesses.size() == 1);
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
            case ImageLayout::General:
                if (prevAccess == AccessType::Present)
                    layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                else
                    layout = VK_IMAGE_LAYOUT_GENERAL;
                break;
            case ImageLayout::Optimal:
                layout = pPrevAccessInfo->imageLayout;
                break;
                //            case ImageLayout::GENERAL_AND_PRESENTATION:
                //                layout = VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR;
                //                break;
            }

#ifdef SYNC_ERROR_CHECK_MIXED_IMAGE_LAYOUT
            assert(pVkBarrier->oldLayout == VK_IMAGE_LAYOUT_UNDEFINED ||
                   pVkBarrier->oldLayout == layout);
#endif
            pVkBarrier->oldLayout = layout;
        }
    }

    for (uint32_t i = 0; i < thBarrier.nextAccesses.size(); ++i) {
        AccessType nextAccess = thBarrier.nextAccesses[i];
        const AccessInfo *pNextAccessInfo = &AccessMap[static_cast<uint32_t>(nextAccess)];

#ifdef SYNC_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
        // Asserts that the next access index is a valid range for the lookup
        assert(nextAccess < AccessType::NUM_ACCESS_TYPES);
#endif

#ifdef SYNC_ERROR_CHECK_POTENTIAL_HAZARD
        // Asserts that the access is a read, else it's a write and it should appear on its own.
        assert(nextAccess < AccessType::END_OF_READ_ACCESS || thBarrier.nextAccesses.size() == 1);
#endif

        *pDstStages |= pNextAccessInfo->stageMask;

        // Add visibility operations as necessary.
        // If the src access mask is zero, this is a WAR hazard (or for some reason a "RAR"),
        // so the dst access mask can be safely zeroed as these don't need visibility.
        if (pVkBarrier->srcAccessMask != 0)
            pVkBarrier->dstAccessMask |= pNextAccessInfo->accessMask;

        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        switch (thBarrier.nextLayout) {
        case ImageLayout::General:
            if (nextAccess == AccessType::Present)
                layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            else
                layout = VK_IMAGE_LAYOUT_GENERAL;
            break;
        case ImageLayout::Optimal:
            layout = pNextAccessInfo->imageLayout;
            break;
            // case ImageLayout::GENERAL_AND_PRESENTATION:
            //     layout = VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR;
            //     break;
        }

#ifdef SYNC_ERROR_CHECK_MIXED_IMAGE_LAYOUT
        assert(pVkBarrier->newLayout == VK_IMAGE_LAYOUT_UNDEFINED ||
               pVkBarrier->newLayout == layout);
#endif
        pVkBarrier->newLayout = layout;
    }

#ifdef SYNC_ERROR_CHECK_COULD_USE_GLOBAL_BARRIER
    assert(pVkBarrier->newLayout != pVkBarrier->oldLayout ||
           pVkBarrier->srcQueueFamilyIndex != pVkBarrier->dstQueueFamilyIndex);
#endif

    // Ensure that the stage masks are valid if no stages were determined
    if (*pSrcStages == 0) *pSrcStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    if (*pDstStages == 0) *pDstStages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
}

void CmdPipelineBarrier(Magnum::Vk::Device &device,
                        VkCommandBuffer commandBuffer,
                        const GlobalBarrier *pGlobalBarrier,
                        std::span<const BufferBarrier> bufferBarriers,
                        std::span<const ImageBarrier> imageBarriers)
{
    VkMemoryBarrier memoryBarrier;
    // Vulkan pipeline barrier command parameters
    //                     commandBuffer;
    VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    uint32_t memoryBarrierCount = (pGlobalBarrier != nullptr) ? 1 : 0;
    VkMemoryBarrier *pMemoryBarriers = (pGlobalBarrier != nullptr) ? &memoryBarrier : nullptr;
    uint32_t bufferMemoryBarrierCount = gsl::narrow<uint32_t>(bufferBarriers.size());
    VkBufferMemoryBarrier *pBufferMemoryBarriers = nullptr;
    uint32_t imageMemoryBarrierCount = gsl::narrow<uint32_t>(imageBarriers.size());
    VkImageMemoryBarrier *pImageMemoryBarriers = nullptr;

    // Global memory barrier
    if (pGlobalBarrier != nullptr) {
        VkPipelineStageFlags tempSrcStageMask = 0;
        VkPipelineStageFlags tempDstStageMask = 0;
        GetVulkanMemoryBarrier(
            *pGlobalBarrier, &tempSrcStageMask, &tempDstStageMask, pMemoryBarriers);
        srcStageMask |= tempSrcStageMask;
        dstStageMask |= tempDstStageMask;
    }

    // Buffer memory barriers
    if (bufferMemoryBarrierCount > 0) {
        pBufferMemoryBarriers = (VkBufferMemoryBarrier *)SYNC_TEMP_ALLOC(
            sizeof(VkBufferMemoryBarrier) * bufferMemoryBarrierCount);

        VkPipelineStageFlags tempSrcStageMask = 0;
        VkPipelineStageFlags tempDstStageMask = 0;
        for (uint32_t i = 0; i < bufferMemoryBarrierCount; ++i) {
            GetVulkanBufferMemoryBarrier(
                bufferBarriers[i], &tempSrcStageMask, &tempDstStageMask, &pBufferMemoryBarriers[i]);
            srcStageMask |= tempSrcStageMask;
            dstStageMask |= tempDstStageMask;
        }
    }

    // Image memory barriers
    if (imageMemoryBarrierCount > 0) {
        pImageMemoryBarriers = (VkImageMemoryBarrier *)SYNC_TEMP_ALLOC(
            sizeof(VkImageMemoryBarrier) * imageMemoryBarrierCount);

        VkPipelineStageFlags tempSrcStageMask = 0;
        VkPipelineStageFlags tempDstStageMask = 0;
        for (uint32_t i = 0; i < imageMemoryBarrierCount; ++i) {
            GetVulkanImageMemoryBarrier(
                imageBarriers[i], &tempSrcStageMask, &tempDstStageMask, &pImageMemoryBarriers[i]);
            srcStageMask |= tempSrcStageMask;
            dstStageMask |= tempDstStageMask;
        }
    }

    device->CmdPipelineBarrier(commandBuffer,
                               srcStageMask,
                               dstStageMask,
                               0,
                               memoryBarrierCount,
                               pMemoryBarriers,
                               bufferMemoryBarrierCount,
                               pBufferMemoryBarriers,
                               imageMemoryBarrierCount,
                               pImageMemoryBarriers);

    SYNC_TEMP_FREE(pBufferMemoryBarriers);
    SYNC_TEMP_FREE(pImageMemoryBarriers);
}

void CmdSetEvent(Magnum::Vk::Device &device,
                 VkCommandBuffer commandBuffer,
                 VkEvent event,
                 std::span<const AccessType> prevAccesses)
{
    VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    for (uint32_t i = 0; i < prevAccesses.size(); ++i) {
        AccessType prevAccess = prevAccesses[i];
        const AccessInfo *pPrevAccessInfo = &AccessMap[static_cast<uint32_t>(prevAccess)];

#ifdef SYNC_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
        // Asserts that the previous access index is a valid range for the lookup
        assert(prevAccess < AccessType::NUM_ACCESS_TYPES);
#endif

        stageMask |= pPrevAccessInfo->stageMask;
    }

    device->CmdSetEvent(commandBuffer, event, stageMask);
}

void CmdResetEvent(Magnum::Vk::Device &device,
                   VkCommandBuffer commandBuffer,
                   VkEvent event,
                   std::span<const AccessType> prevAccesses)
{
    VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    for (uint32_t i = 0; i < prevAccesses.size(); ++i) {
        AccessType prevAccess = prevAccesses[i];
        const AccessInfo *pPrevAccessInfo = &AccessMap[static_cast<uint32_t>(prevAccess)];

#ifdef SYNC_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
        // Asserts that the previous access index is a valid range for the lookup
        assert(prevAccess < AccessType::NUM_ACCESS_TYPES);
#endif

        stageMask |= pPrevAccessInfo->stageMask;
    }

    device->CmdResetEvent(commandBuffer, event, stageMask);
}

void CmdWaitEvents(Magnum::Vk::Device &device,
                   VkCommandBuffer commandBuffer,
                   std::span<const VkEvent> events,
                   const GlobalBarrier *pGlobalBarrier,
                   std::span<const BufferBarrier> bufferBarriers,
                   std::span<const ImageBarrier> imageBarriers)
{
    VkMemoryBarrier memoryBarrier;
    // Vulkan pipeline barrier command parameters
    //                     commandBuffer;
    //                     eventCount;
    //                     pEvents;
    VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    uint32_t memoryBarrierCount = (pGlobalBarrier != nullptr) ? 1 : 0;
    VkMemoryBarrier *pMemoryBarriers = (pGlobalBarrier != nullptr) ? &memoryBarrier : nullptr;
    uint32_t bufferMemoryBarrierCount = gsl::narrow<uint32_t>(bufferBarriers.size());
    VkBufferMemoryBarrier *pBufferMemoryBarriers = nullptr;
    uint32_t imageMemoryBarrierCount = gsl::narrow<uint32_t>(imageBarriers.size());
    VkImageMemoryBarrier *pImageMemoryBarriers = nullptr;

    // Global memory barrier
    if (pGlobalBarrier != nullptr) {
        VkPipelineStageFlags tempSrcStageMask = 0;
        VkPipelineStageFlags tempDstStageMask = 0;
        GetVulkanMemoryBarrier(
            *pGlobalBarrier, &tempSrcStageMask, &tempDstStageMask, pMemoryBarriers);
        srcStageMask |= tempSrcStageMask;
        dstStageMask |= tempDstStageMask;
    }

    // Buffer memory barriers
    if (bufferMemoryBarrierCount > 0) {
        pBufferMemoryBarriers = (VkBufferMemoryBarrier *)SYNC_TEMP_ALLOC(
            sizeof(VkBufferMemoryBarrier) * bufferMemoryBarrierCount);

        VkPipelineStageFlags tempSrcStageMask = 0;
        VkPipelineStageFlags tempDstStageMask = 0;
        for (uint32_t i = 0; i < bufferMemoryBarrierCount; ++i) {
            GetVulkanBufferMemoryBarrier(
                bufferBarriers[i], &tempSrcStageMask, &tempDstStageMask, &pBufferMemoryBarriers[i]);
            srcStageMask |= tempSrcStageMask;
            dstStageMask |= tempDstStageMask;
        }
    }

    // Image memory barriers
    if (imageMemoryBarrierCount > 0) {
        pImageMemoryBarriers = (VkImageMemoryBarrier *)SYNC_TEMP_ALLOC(
            sizeof(VkImageMemoryBarrier) * imageMemoryBarrierCount);

        VkPipelineStageFlags tempSrcStageMask = 0;
        VkPipelineStageFlags tempDstStageMask = 0;
        for (uint32_t i = 0; i < imageMemoryBarrierCount; ++i) {
            GetVulkanImageMemoryBarrier(
                imageBarriers[i], &tempSrcStageMask, &tempDstStageMask, &pImageMemoryBarriers[i]);
            srcStageMask |= tempSrcStageMask;
            dstStageMask |= tempDstStageMask;
        }
    }

    device->CmdWaitEvents(commandBuffer,
                          gsl::narrow<uint32_t>(events.size()),
                          events.data(),
                          srcStageMask,
                          dstStageMask,
                          memoryBarrierCount,
                          pMemoryBarriers,
                          bufferMemoryBarrierCount,
                          pBufferMemoryBarriers,
                          imageMemoryBarrierCount,
                          pImageMemoryBarriers);

    SYNC_TEMP_FREE(pBufferMemoryBarriers);
    SYNC_TEMP_FREE(pImageMemoryBarriers);
}
} // namespace Cory::Sync