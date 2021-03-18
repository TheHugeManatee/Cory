#pragma once

#include "command_pool.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace cory {
namespace vk {

class graphics_context;

/**
 * encapsulates a command buffer that has finished recording and is ready to execute.
 */
class executable_command_buffer {
  public:
    executable_command_buffer(std::shared_ptr<VkCommandBuffer_T> cmd_buffer_ptr, command_pool pool)
        : cmd_buffer_ptr_{cmd_buffer_ptr}
        , pool_(std::move(pool))
    {
    }

    VkCommandBuffer get() { return cmd_buffer_ptr_.get(); }

  private:
    command_pool pool_;
    std::shared_ptr<VkCommandBuffer_T> cmd_buffer_ptr_;
};

class command_buffer {
  public:
    command_buffer(command_pool &pool)
        : pool_{pool} {};

    VkCommandBuffer get() { return cmd_buffer_ptr_.get(); }

    /**
     * finishes the command buffer with \a vkEndCommandBuffer and creates a new object encapsulating
     * the executable state. after this call the \a command_buffer will be in a moved-from state and
     * cannot be used further.
     */
    executable_command_buffer end() noexcept
    {
        vkEndCommandBuffer(cmd_buffer_ptr_.get());
        return {std::move(cmd_buffer_ptr_), std::move(pool_)};
    }

    command_buffer &begin_conditional_rendering_ext(
        const VkConditionalRenderingBeginInfoEXT *pConditionalRenderingBegin)
    {
        vkCmdBeginConditionalRenderingEXT(cmd_buffer_ptr_.get(), pConditionalRenderingBegin);
        return *this;
    }

    command_buffer &begin_debug_utils_label_ext(const VkDebugUtilsLabelEXT *pLabelInfo)
    {
        vkCmdBeginDebugUtilsLabelEXT(cmd_buffer_ptr_.get(), pLabelInfo);
        return *this;
    }

    command_buffer &begin_query(VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags)
    {
        vkCmdBeginQuery(cmd_buffer_ptr_.get(), queryPool, query, flags);
        return *this;
    }

    command_buffer &begin_query_indexed_ext(VkQueryPool queryPool,
                                            uint32_t query,
                                            VkQueryControlFlags flags,
                                            uint32_t index)
    {
        vkCmdBeginQueryIndexedEXT(cmd_buffer_ptr_.get(), queryPool, query, flags, index);
        return *this;
    }

    command_buffer &begin_render_pass(const VkRenderPassBeginInfo *pRenderPassBegin,
                                      VkSubpassContents contents)
    {
        vkCmdBeginRenderPass(cmd_buffer_ptr_.get(), pRenderPassBegin, contents);
        return *this;
    }

    command_buffer &begin_render_pass_2(const VkRenderPassBeginInfo *pRenderPassBegin,
                                        const VkSubpassBeginInfo *pSubpassBeginInfo)
    {
        vkCmdBeginRenderPass2(cmd_buffer_ptr_.get(), pRenderPassBegin, pSubpassBeginInfo);
        return *this;
    }

    command_buffer &begin_render_pass_2khr(const VkRenderPassBeginInfo *pRenderPassBegin,
                                           const VkSubpassBeginInfo *pSubpassBeginInfo)
    {
        vkCmdBeginRenderPass2KHR(cmd_buffer_ptr_.get(), pRenderPassBegin, pSubpassBeginInfo);
        return *this;
    }

    command_buffer &begin_transform_feedback_ext(uint32_t firstCounterBuffer,
                                                 uint32_t counterBufferCount,
                                                 const VkBuffer *pCounterBuffers,
                                                 const VkDeviceSize *pCounterBufferOffsets)
    {
        vkCmdBeginTransformFeedbackEXT(cmd_buffer_ptr_.get(),
                                       firstCounterBuffer,
                                       counterBufferCount,
                                       pCounterBuffers,
                                       pCounterBufferOffsets);
        return *this;
    }

    command_buffer &bind_descriptor_sets(VkPipelineBindPoint pipelineBindPoint,
                                         VkPipelineLayout layout,
                                         uint32_t firstSet,
                                         uint32_t descriptorSetCount,
                                         const VkDescriptorSet *pDescriptorSets,
                                         uint32_t dynamicOffsetCount,
                                         const uint32_t *pDynamicOffsets)
    {
        vkCmdBindDescriptorSets(cmd_buffer_ptr_.get(),
                                pipelineBindPoint,
                                layout,
                                firstSet,
                                descriptorSetCount,
                                pDescriptorSets,
                                dynamicOffsetCount,
                                pDynamicOffsets);
        return *this;
    }

    command_buffer &bind_index_buffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType)
    {
        vkCmdBindIndexBuffer(cmd_buffer_ptr_.get(), buffer, offset, indexType);
        return *this;
    }

    command_buffer &bind_pipeline(VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline)
    {
        vkCmdBindPipeline(cmd_buffer_ptr_.get(), pipelineBindPoint, pipeline);
        return *this;
    }

    command_buffer &bind_pipeline_shader_group_nv(VkPipelineBindPoint pipelineBindPoint,
                                                  VkPipeline pipeline,
                                                  uint32_t groupIndex)
    {
        vkCmdBindPipelineShaderGroupNV(
            cmd_buffer_ptr_.get(), pipelineBindPoint, pipeline, groupIndex);
        return *this;
    }

    command_buffer &bind_shading_rate_image_nv(VkImageView imageView, VkImageLayout imageLayout)
    {
        vkCmdBindShadingRateImageNV(cmd_buffer_ptr_.get(), imageView, imageLayout);
        return *this;
    }

    command_buffer &bind_transform_feedback_buffers_ext(uint32_t firstBinding,
                                                        uint32_t bindingCount,
                                                        const VkBuffer *pBuffers,
                                                        const VkDeviceSize *pOffsets,
                                                        const VkDeviceSize *pSizes)
    {
        vkCmdBindTransformFeedbackBuffersEXT(
            cmd_buffer_ptr_.get(), firstBinding, bindingCount, pBuffers, pOffsets, pSizes);
        return *this;
    }

    command_buffer &bind_vertex_buffers(uint32_t firstBinding,
                                        uint32_t bindingCount,
                                        const VkBuffer *pBuffers,
                                        const VkDeviceSize *pOffsets)
    {
        vkCmdBindVertexBuffers(
            cmd_buffer_ptr_.get(), firstBinding, bindingCount, pBuffers, pOffsets);
        return *this;
    }

    command_buffer &bind_vertex_buffers_2ext(uint32_t firstBinding,
                                             uint32_t bindingCount,
                                             const VkBuffer *pBuffers,
                                             const VkDeviceSize *pOffsets,
                                             const VkDeviceSize *pSizes,
                                             const VkDeviceSize *pStrides)
    {
        vkCmdBindVertexBuffers2EXT(cmd_buffer_ptr_.get(),
                                   firstBinding,
                                   bindingCount,
                                   pBuffers,
                                   pOffsets,
                                   pSizes,
                                   pStrides);
        return *this;
    }

    command_buffer &blit_image(VkImage srcImage,
                               VkImageLayout srcImageLayout,
                               VkImage dstImage,
                               VkImageLayout dstImageLayout,
                               uint32_t regionCount,
                               const VkImageBlit *pRegions,
                               VkFilter filter)
    {
        vkCmdBlitImage(cmd_buffer_ptr_.get(),
                       srcImage,
                       srcImageLayout,
                       dstImage,
                       dstImageLayout,
                       regionCount,
                       pRegions,
                       filter);
        return *this;
    }

    command_buffer &blit_image_2khr(const VkBlitImageInfo2KHR *pBlitImageInfo)
    {
        vkCmdBlitImage2KHR(cmd_buffer_ptr_.get(), pBlitImageInfo);
        return *this;
    }

    command_buffer &build_acceleration_structure_nv(const VkAccelerationStructureInfoNV *pInfo,
                                                    VkBuffer instanceData,
                                                    VkDeviceSize instanceOffset,
                                                    VkBool32 update,
                                                    VkAccelerationStructureKHR dst,
                                                    VkAccelerationStructureKHR src,
                                                    VkBuffer scratch,
                                                    VkDeviceSize scratchOffset)
    {
        vkCmdBuildAccelerationStructureNV(cmd_buffer_ptr_.get(),
                                          pInfo,
                                          instanceData,
                                          instanceOffset,
                                          update,
                                          dst,
                                          src,
                                          scratch,
                                          scratchOffset);
        return *this;
    }

    command_buffer &clear_attachments(uint32_t attachmentCount,
                                      const VkClearAttachment *pAttachments,
                                      uint32_t rectCount,
                                      const VkClearRect *pRects)
    {
        vkCmdClearAttachments(
            cmd_buffer_ptr_.get(), attachmentCount, pAttachments, rectCount, pRects);
        return *this;
    }

    command_buffer &clear_color_image(VkImage image,
                                      VkImageLayout imageLayout,
                                      const VkClearColorValue *pColor,
                                      uint32_t rangeCount,
                                      const VkImageSubresourceRange *pRanges)
    {
        vkCmdClearColorImage(
            cmd_buffer_ptr_.get(), image, imageLayout, pColor, rangeCount, pRanges);
        return *this;
    }

    command_buffer &clear_depth_stencil_image(VkImage image,
                                              VkImageLayout imageLayout,
                                              const VkClearDepthStencilValue *pDepthStencil,
                                              uint32_t rangeCount,
                                              const VkImageSubresourceRange *pRanges)
    {
        vkCmdClearDepthStencilImage(
            cmd_buffer_ptr_.get(), image, imageLayout, pDepthStencil, rangeCount, pRanges);
        return *this;
    }

    command_buffer &copy_acceleration_structure_nv(VkAccelerationStructureKHR dst,
                                                   VkAccelerationStructureKHR src,
                                                   VkCopyAccelerationStructureModeKHR mode)
    {
        vkCmdCopyAccelerationStructureNV(cmd_buffer_ptr_.get(), dst, src, mode);
        return *this;
    }

    command_buffer &copy_buffer(VkBuffer srcBuffer,
                                VkBuffer dstBuffer,
                                uint32_t regionCount,
                                const VkBufferCopy *pRegions)
    {
        vkCmdCopyBuffer(cmd_buffer_ptr_.get(), srcBuffer, dstBuffer, regionCount, pRegions);
        return *this;
    }

    command_buffer &copy_buffer_2khr(const VkCopyBufferInfo2KHR *pCopyBufferInfo)
    {
        vkCmdCopyBuffer2KHR(cmd_buffer_ptr_.get(), pCopyBufferInfo);
        return *this;
    }

    command_buffer &copy_buffer_to_image(VkBuffer srcBuffer,
                                         VkImage dstImage,
                                         VkImageLayout dstImageLayout,
                                         uint32_t regionCount,
                                         const VkBufferImageCopy *pRegions)
    {
        vkCmdCopyBufferToImage(
            cmd_buffer_ptr_.get(), srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
        return *this;
    }

    command_buffer &
    copy_buffer_to_image_2khr(const VkCopyBufferToImageInfo2KHR *pCopyBufferToImageInfo)
    {
        vkCmdCopyBufferToImage2KHR(cmd_buffer_ptr_.get(), pCopyBufferToImageInfo);
        return *this;
    }

    command_buffer &copy_image(VkImage srcImage,
                               VkImageLayout srcImageLayout,
                               VkImage dstImage,
                               VkImageLayout dstImageLayout,
                               uint32_t regionCount,
                               const VkImageCopy *pRegions)
    {
        vkCmdCopyImage(cmd_buffer_ptr_.get(),
                       srcImage,
                       srcImageLayout,
                       dstImage,
                       dstImageLayout,
                       regionCount,
                       pRegions);
        return *this;
    }

    command_buffer &copy_image_2khr(const VkCopyImageInfo2KHR *pCopyImageInfo)
    {
        vkCmdCopyImage2KHR(cmd_buffer_ptr_.get(), pCopyImageInfo);
        return *this;
    }

    command_buffer &copy_image_to_buffer(VkImage srcImage,
                                         VkImageLayout srcImageLayout,
                                         VkBuffer dstBuffer,
                                         uint32_t regionCount,
                                         const VkBufferImageCopy *pRegions)
    {
        vkCmdCopyImageToBuffer(
            cmd_buffer_ptr_.get(), srcImage, srcImageLayout, dstBuffer, regionCount, pRegions);
        return *this;
    }

    command_buffer &
    copy_image_to_buffer_2khr(const VkCopyImageToBufferInfo2KHR *pCopyImageToBufferInfo)
    {
        vkCmdCopyImageToBuffer2KHR(cmd_buffer_ptr_.get(), pCopyImageToBufferInfo);
        return *this;
    }

    command_buffer &copy_query_pool_results(VkQueryPool queryPool,
                                            uint32_t firstQuery,
                                            uint32_t queryCount,
                                            VkBuffer dstBuffer,
                                            VkDeviceSize dstOffset,
                                            VkDeviceSize stride,
                                            VkQueryResultFlags flags)
    {
        vkCmdCopyQueryPoolResults(cmd_buffer_ptr_.get(),
                                  queryPool,
                                  firstQuery,
                                  queryCount,
                                  dstBuffer,
                                  dstOffset,
                                  stride,
                                  flags);
        return *this;
    }

    command_buffer &debug_marker_begin_ext(const VkDebugMarkerMarkerInfoEXT *pMarkerInfo)
    {
        vkCmdDebugMarkerBeginEXT(cmd_buffer_ptr_.get(), pMarkerInfo);
        return *this;
    }

    command_buffer &debug_marker_end_ext()
    {
        vkCmdDebugMarkerEndEXT(cmd_buffer_ptr_.get());
        return *this;
    }

    command_buffer &debug_marker_insert_ext(const VkDebugMarkerMarkerInfoEXT *pMarkerInfo)
    {
        vkCmdDebugMarkerInsertEXT(cmd_buffer_ptr_.get(), pMarkerInfo);
        return *this;
    }

    command_buffer &dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
        vkCmdDispatch(cmd_buffer_ptr_.get(), groupCountX, groupCountY, groupCountZ);
        return *this;
    }

    command_buffer &dispatch_base(uint32_t baseGroupX,
                                  uint32_t baseGroupY,
                                  uint32_t baseGroupZ,
                                  uint32_t groupCountX,
                                  uint32_t groupCountY,
                                  uint32_t groupCountZ)
    {
        vkCmdDispatchBase(cmd_buffer_ptr_.get(),
                          baseGroupX,
                          baseGroupY,
                          baseGroupZ,
                          groupCountX,
                          groupCountY,
                          groupCountZ);
        return *this;
    }

    command_buffer &dispatch_base_khr(uint32_t baseGroupX,
                                      uint32_t baseGroupY,
                                      uint32_t baseGroupZ,
                                      uint32_t groupCountX,
                                      uint32_t groupCountY,
                                      uint32_t groupCountZ)
    {
        vkCmdDispatchBaseKHR(cmd_buffer_ptr_.get(),
                             baseGroupX,
                             baseGroupY,
                             baseGroupZ,
                             groupCountX,
                             groupCountY,
                             groupCountZ);
        return *this;
    }

    command_buffer &dispatch_indirect(VkBuffer buffer, VkDeviceSize offset)
    {
        vkCmdDispatchIndirect(cmd_buffer_ptr_.get(), buffer, offset);
        return *this;
    }

    command_buffer &
    draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
    {
        vkCmdDraw(cmd_buffer_ptr_.get(), vertexCount, instanceCount, firstVertex, firstInstance);
        return *this;
    }

    command_buffer &draw_indexed(uint32_t indexCount,
                                 uint32_t instanceCount,
                                 uint32_t firstIndex,
                                 int32_t vertexOffset,
                                 uint32_t firstInstance)
    {
        vkCmdDrawIndexed(cmd_buffer_ptr_.get(),
                         indexCount,
                         instanceCount,
                         firstIndex,
                         vertexOffset,
                         firstInstance);
        return *this;
    }

    command_buffer &
    draw_indexed_indirect(VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
    {
        vkCmdDrawIndexedIndirect(cmd_buffer_ptr_.get(), buffer, offset, drawCount, stride);
        return *this;
    }

    command_buffer &draw_indexed_indirect_count(VkBuffer buffer,
                                                VkDeviceSize offset,
                                                VkBuffer countBuffer,
                                                VkDeviceSize countBufferOffset,
                                                uint32_t maxDrawCount,
                                                uint32_t stride)
    {
        vkCmdDrawIndexedIndirectCount(cmd_buffer_ptr_.get(),
                                      buffer,
                                      offset,
                                      countBuffer,
                                      countBufferOffset,
                                      maxDrawCount,
                                      stride);
        return *this;
    }

    command_buffer &draw_indexed_indirect_count_amd(VkBuffer buffer,
                                                    VkDeviceSize offset,
                                                    VkBuffer countBuffer,
                                                    VkDeviceSize countBufferOffset,
                                                    uint32_t maxDrawCount,
                                                    uint32_t stride)
    {
        vkCmdDrawIndexedIndirectCountAMD(cmd_buffer_ptr_.get(),
                                         buffer,
                                         offset,
                                         countBuffer,
                                         countBufferOffset,
                                         maxDrawCount,
                                         stride);
        return *this;
    }

    command_buffer &draw_indexed_indirect_count_khr(VkBuffer buffer,
                                                    VkDeviceSize offset,
                                                    VkBuffer countBuffer,
                                                    VkDeviceSize countBufferOffset,
                                                    uint32_t maxDrawCount,
                                                    uint32_t stride)
    {
        vkCmdDrawIndexedIndirectCountKHR(cmd_buffer_ptr_.get(),
                                         buffer,
                                         offset,
                                         countBuffer,
                                         countBufferOffset,
                                         maxDrawCount,
                                         stride);
        return *this;
    }

    command_buffer &
    draw_indirect(VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
    {
        vkCmdDrawIndirect(cmd_buffer_ptr_.get(), buffer, offset, drawCount, stride);
        return *this;
    }

    command_buffer &draw_indirect_byte_count_ext(uint32_t instanceCount,
                                                 uint32_t firstInstance,
                                                 VkBuffer counterBuffer,
                                                 VkDeviceSize counterBufferOffset,
                                                 uint32_t counterOffset,
                                                 uint32_t vertexStride)
    {
        vkCmdDrawIndirectByteCountEXT(cmd_buffer_ptr_.get(),
                                      instanceCount,
                                      firstInstance,
                                      counterBuffer,
                                      counterBufferOffset,
                                      counterOffset,
                                      vertexStride);
        return *this;
    }

    command_buffer &draw_indirect_count(VkBuffer buffer,
                                        VkDeviceSize offset,
                                        VkBuffer countBuffer,
                                        VkDeviceSize countBufferOffset,
                                        uint32_t maxDrawCount,
                                        uint32_t stride)
    {
        vkCmdDrawIndirectCount(cmd_buffer_ptr_.get(),
                               buffer,
                               offset,
                               countBuffer,
                               countBufferOffset,
                               maxDrawCount,
                               stride);
        return *this;
    }

    command_buffer &draw_indirect_count_amd(VkBuffer buffer,
                                            VkDeviceSize offset,
                                            VkBuffer countBuffer,
                                            VkDeviceSize countBufferOffset,
                                            uint32_t maxDrawCount,
                                            uint32_t stride)
    {
        vkCmdDrawIndirectCountAMD(cmd_buffer_ptr_.get(),
                                  buffer,
                                  offset,
                                  countBuffer,
                                  countBufferOffset,
                                  maxDrawCount,
                                  stride);
        return *this;
    }

    command_buffer &draw_indirect_count_khr(VkBuffer buffer,
                                            VkDeviceSize offset,
                                            VkBuffer countBuffer,
                                            VkDeviceSize countBufferOffset,
                                            uint32_t maxDrawCount,
                                            uint32_t stride)
    {
        vkCmdDrawIndirectCountKHR(cmd_buffer_ptr_.get(),
                                  buffer,
                                  offset,
                                  countBuffer,
                                  countBufferOffset,
                                  maxDrawCount,
                                  stride);
        return *this;
    }

    command_buffer &draw_mesh_tasks_indirect_count_nv(VkBuffer buffer,
                                                      VkDeviceSize offset,
                                                      VkBuffer countBuffer,
                                                      VkDeviceSize countBufferOffset,
                                                      uint32_t maxDrawCount,
                                                      uint32_t stride)
    {
        vkCmdDrawMeshTasksIndirectCountNV(cmd_buffer_ptr_.get(),
                                          buffer,
                                          offset,
                                          countBuffer,
                                          countBufferOffset,
                                          maxDrawCount,
                                          stride);
        return *this;
    }

    command_buffer &draw_mesh_tasks_indirect_nv(VkBuffer buffer,
                                                VkDeviceSize offset,
                                                uint32_t drawCount,
                                                uint32_t stride)
    {
        vkCmdDrawMeshTasksIndirectNV(cmd_buffer_ptr_.get(), buffer, offset, drawCount, stride);
        return *this;
    }

    command_buffer &draw_mesh_tasks_nv(uint32_t taskCount, uint32_t firstTask)
    {
        vkCmdDrawMeshTasksNV(cmd_buffer_ptr_.get(), taskCount, firstTask);
        return *this;
    }

    command_buffer &end_conditional_rendering_ext()
    {
        vkCmdEndConditionalRenderingEXT(cmd_buffer_ptr_.get());
        return *this;
    }

    command_buffer &end_debug_utils_label_ext()
    {
        vkCmdEndDebugUtilsLabelEXT(cmd_buffer_ptr_.get());
        return *this;
    }

    command_buffer &end_query(VkQueryPool queryPool, uint32_t query)
    {
        vkCmdEndQuery(cmd_buffer_ptr_.get(), queryPool, query);
        return *this;
    }

    command_buffer &end_query_indexed_ext(VkQueryPool queryPool, uint32_t query, uint32_t index)
    {
        vkCmdEndQueryIndexedEXT(cmd_buffer_ptr_.get(), queryPool, query, index);
        return *this;
    }

    command_buffer &end_render_pass()
    {
        vkCmdEndRenderPass(cmd_buffer_ptr_.get());
        return *this;
    }

    command_buffer &end_render_pass_2(const VkSubpassEndInfo *pSubpassEndInfo)
    {
        vkCmdEndRenderPass2(cmd_buffer_ptr_.get(), pSubpassEndInfo);
        return *this;
    }

    command_buffer &end_render_pass_2khr(const VkSubpassEndInfo *pSubpassEndInfo)
    {
        vkCmdEndRenderPass2KHR(cmd_buffer_ptr_.get(), pSubpassEndInfo);
        return *this;
    }

    command_buffer &end_transform_feedback_ext(uint32_t firstCounterBuffer,
                                               uint32_t counterBufferCount,
                                               const VkBuffer *pCounterBuffers,
                                               const VkDeviceSize *pCounterBufferOffsets)
    {
        vkCmdEndTransformFeedbackEXT(cmd_buffer_ptr_.get(),
                                     firstCounterBuffer,
                                     counterBufferCount,
                                     pCounterBuffers,
                                     pCounterBufferOffsets);
        return *this;
    }

    command_buffer &execute_commands(uint32_t commandBufferCount,
                                     const VkCommandBuffer *pCommandBuffers)
    {
        vkCmdExecuteCommands(cmd_buffer_ptr_.get(), commandBufferCount, pCommandBuffers);
        return *this;
    }

    command_buffer &
    execute_generated_commands_nv(VkBool32 isPreprocessed,
                                  const VkGeneratedCommandsInfoNV *pGeneratedCommandsInfo)
    {
        vkCmdExecuteGeneratedCommandsNV(
            cmd_buffer_ptr_.get(), isPreprocessed, pGeneratedCommandsInfo);
        return *this;
    }

    command_buffer &
    fill_buffer(VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data)
    {
        vkCmdFillBuffer(cmd_buffer_ptr_.get(), dstBuffer, dstOffset, size, data);
        return *this;
    }

    command_buffer &insert_debug_utils_label_ext(const VkDebugUtilsLabelEXT *pLabelInfo)
    {
        vkCmdInsertDebugUtilsLabelEXT(cmd_buffer_ptr_.get(), pLabelInfo);
        return *this;
    }

    command_buffer &next_subpass(VkSubpassContents contents)
    {
        vkCmdNextSubpass(cmd_buffer_ptr_.get(), contents);
        return *this;
    }

    command_buffer &next_subpass_2(const VkSubpassBeginInfo *pSubpassBeginInfo,
                                   const VkSubpassEndInfo *pSubpassEndInfo)
    {
        vkCmdNextSubpass2(cmd_buffer_ptr_.get(), pSubpassBeginInfo, pSubpassEndInfo);
        return *this;
    }

    command_buffer &next_subpass_2khr(const VkSubpassBeginInfo *pSubpassBeginInfo,
                                      const VkSubpassEndInfo *pSubpassEndInfo)
    {
        vkCmdNextSubpass2KHR(cmd_buffer_ptr_.get(), pSubpassBeginInfo, pSubpassEndInfo);
        return *this;
    }

    command_buffer &pipeline_barrier(VkPipelineStageFlags srcStageMask,
                                     VkPipelineStageFlags dstStageMask,
                                     VkDependencyFlags dependencyFlags,
                                     uint32_t memoryBarrierCount,
                                     const VkMemoryBarrier *pMemoryBarriers,
                                     uint32_t bufferMemoryBarrierCount,
                                     const VkBufferMemoryBarrier *pBufferMemoryBarriers,
                                     uint32_t imageMemoryBarrierCount,
                                     const VkImageMemoryBarrier *pImageMemoryBarriers)
    {
        vkCmdPipelineBarrier(cmd_buffer_ptr_.get(),
                             srcStageMask,
                             dstStageMask,
                             dependencyFlags,
                             memoryBarrierCount,
                             pMemoryBarriers,
                             bufferMemoryBarrierCount,
                             pBufferMemoryBarriers,
                             imageMemoryBarrierCount,
                             pImageMemoryBarriers);
        return *this;
    }

    command_buffer &
    preprocess_generated_commands_nv(const VkGeneratedCommandsInfoNV *pGeneratedCommandsInfo)
    {
        vkCmdPreprocessGeneratedCommandsNV(cmd_buffer_ptr_.get(), pGeneratedCommandsInfo);
        return *this;
    }

    command_buffer &push_constants(VkPipelineLayout layout,
                                   VkShaderStageFlags stageFlags,
                                   uint32_t offset,
                                   uint32_t size,
                                   const void *pValues)
    {
        vkCmdPushConstants(cmd_buffer_ptr_.get(), layout, stageFlags, offset, size, pValues);
        return *this;
    }

    command_buffer &push_descriptor_set_khr(VkPipelineBindPoint pipelineBindPoint,
                                            VkPipelineLayout layout,
                                            uint32_t set,
                                            uint32_t descriptorWriteCount,
                                            const VkWriteDescriptorSet *pDescriptorWrites)
    {
        vkCmdPushDescriptorSetKHR(cmd_buffer_ptr_.get(),
                                  pipelineBindPoint,
                                  layout,
                                  set,
                                  descriptorWriteCount,
                                  pDescriptorWrites);
        return *this;
    }

    command_buffer &
    push_descriptor_set_with_template_khr(VkDescriptorUpdateTemplate descriptorUpdateTemplate,
                                          VkPipelineLayout layout,
                                          uint32_t set,
                                          const void *pData)
    {
        vkCmdPushDescriptorSetWithTemplateKHR(
            cmd_buffer_ptr_.get(), descriptorUpdateTemplate, layout, set, pData);
        return *this;
    }

    command_buffer &reset_event(VkEvent event, VkPipelineStageFlags stageMask)
    {
        vkCmdResetEvent(cmd_buffer_ptr_.get(), event, stageMask);
        return *this;
    }

    command_buffer &
    reset_query_pool(VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount)
    {
        vkCmdResetQueryPool(cmd_buffer_ptr_.get(), queryPool, firstQuery, queryCount);
        return *this;
    }

    command_buffer &resolve_image(VkImage srcImage,
                                  VkImageLayout srcImageLayout,
                                  VkImage dstImage,
                                  VkImageLayout dstImageLayout,
                                  uint32_t regionCount,
                                  const VkImageResolve *pRegions)
    {
        vkCmdResolveImage(cmd_buffer_ptr_.get(),
                          srcImage,
                          srcImageLayout,
                          dstImage,
                          dstImageLayout,
                          regionCount,
                          pRegions);
        return *this;
    }

    command_buffer &resolve_image_2khr(const VkResolveImageInfo2KHR *pResolveImageInfo)
    {
        vkCmdResolveImage2KHR(cmd_buffer_ptr_.get(), pResolveImageInfo);
        return *this;
    }

    command_buffer &set_blend_constants(const float blendConstants[4])
    {
        vkCmdSetBlendConstants(cmd_buffer_ptr_.get(), blendConstants);
        return *this;
    }

    command_buffer &set_checkpoint_nv(const void *pCheckpointMarker)
    {
        vkCmdSetCheckpointNV(cmd_buffer_ptr_.get(), pCheckpointMarker);
        return *this;
    }

    command_buffer &
    set_coarse_sample_order_nv(VkCoarseSampleOrderTypeNV sampleOrderType,
                               uint32_t customSampleOrderCount,
                               const VkCoarseSampleOrderCustomNV *pCustomSampleOrders)
    {
        vkCmdSetCoarseSampleOrderNV(
            cmd_buffer_ptr_.get(), sampleOrderType, customSampleOrderCount, pCustomSampleOrders);
        return *this;
    }

    command_buffer &set_cull_mode_ext(VkCullModeFlags cullMode)
    {
        vkCmdSetCullModeEXT(cmd_buffer_ptr_.get(), cullMode);
        return *this;
    }

    command_buffer &
    set_depth_bias(float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor)
    {
        vkCmdSetDepthBias(
            cmd_buffer_ptr_.get(), depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
        return *this;
    }

    command_buffer &set_depth_bounds(float minDepthBounds, float maxDepthBounds)
    {
        vkCmdSetDepthBounds(cmd_buffer_ptr_.get(), minDepthBounds, maxDepthBounds);
        return *this;
    }

    command_buffer &set_depth_bounds_test_enable_ext(VkBool32 depthBoundsTestEnable)
    {
        vkCmdSetDepthBoundsTestEnableEXT(cmd_buffer_ptr_.get(), depthBoundsTestEnable);
        return *this;
    }

    command_buffer &set_depth_compare_op_ext(VkCompareOp depthCompareOp)
    {
        vkCmdSetDepthCompareOpEXT(cmd_buffer_ptr_.get(), depthCompareOp);
        return *this;
    }

    command_buffer &set_depth_test_enable_ext(VkBool32 depthTestEnable)
    {
        vkCmdSetDepthTestEnableEXT(cmd_buffer_ptr_.get(), depthTestEnable);
        return *this;
    }

    command_buffer &set_depth_write_enable_ext(VkBool32 depthWriteEnable)
    {
        vkCmdSetDepthWriteEnableEXT(cmd_buffer_ptr_.get(), depthWriteEnable);
        return *this;
    }

    command_buffer &set_device_mask(uint32_t deviceMask)
    {
        vkCmdSetDeviceMask(cmd_buffer_ptr_.get(), deviceMask);
        return *this;
    }

    command_buffer &set_device_mask_khr(uint32_t deviceMask)
    {
        vkCmdSetDeviceMaskKHR(cmd_buffer_ptr_.get(), deviceMask);
        return *this;
    }

    command_buffer &set_discard_rectangle_ext(uint32_t firstDiscardRectangle,
                                              uint32_t discardRectangleCount,
                                              const VkRect2D *pDiscardRectangles)
    {
        vkCmdSetDiscardRectangleEXT(cmd_buffer_ptr_.get(),
                                    firstDiscardRectangle,
                                    discardRectangleCount,
                                    pDiscardRectangles);
        return *this;
    }

    command_buffer &set_event(VkEvent event, VkPipelineStageFlags stageMask)
    {
        vkCmdSetEvent(cmd_buffer_ptr_.get(), event, stageMask);
        return *this;
    }

    command_buffer &set_exclusive_scissor_nv(uint32_t firstExclusiveScissor,
                                             uint32_t exclusiveScissorCount,
                                             const VkRect2D *pExclusiveScissors)
    {
        vkCmdSetExclusiveScissorNV(cmd_buffer_ptr_.get(),
                                   firstExclusiveScissor,
                                   exclusiveScissorCount,
                                   pExclusiveScissors);
        return *this;
    }

    command_buffer &set_front_face_ext(VkFrontFace frontFace)
    {
        vkCmdSetFrontFaceEXT(cmd_buffer_ptr_.get(), frontFace);
        return *this;
    }

    command_buffer &set_line_stipple_ext(uint32_t lineStippleFactor, uint16_t lineStipplePattern)
    {
        vkCmdSetLineStippleEXT(cmd_buffer_ptr_.get(), lineStippleFactor, lineStipplePattern);
        return *this;
    }

    command_buffer &set_line_width(float lineWidth)
    {
        vkCmdSetLineWidth(cmd_buffer_ptr_.get(), lineWidth);
        return *this;
    }

    command_buffer &set_performance_marker_intel(const VkPerformanceMarkerInfoINTEL *pMarkerInfo)
    {
        vkCmdSetPerformanceMarkerINTEL(cmd_buffer_ptr_.get(), pMarkerInfo);
        return *this;
    }

    command_buffer &
    set_performance_override_intel(const VkPerformanceOverrideInfoINTEL *pOverrideInfo)
    {
        vkCmdSetPerformanceOverrideINTEL(cmd_buffer_ptr_.get(), pOverrideInfo);
        return *this;
    }

    command_buffer &
    set_performance_stream_marker_intel(const VkPerformanceStreamMarkerInfoINTEL *pMarkerInfo)
    {
        vkCmdSetPerformanceStreamMarkerINTEL(cmd_buffer_ptr_.get(), pMarkerInfo);
        return *this;
    }

    command_buffer &set_primitive_topology_ext(VkPrimitiveTopology primitiveTopology)
    {
        vkCmdSetPrimitiveTopologyEXT(cmd_buffer_ptr_.get(), primitiveTopology);
        return *this;
    }

    command_buffer &set_sample_locations_ext(const VkSampleLocationsInfoEXT *pSampleLocationsInfo)
    {
        vkCmdSetSampleLocationsEXT(cmd_buffer_ptr_.get(), pSampleLocationsInfo);
        return *this;
    }

    command_buffer &
    set_scissor(uint32_t firstScissor, uint32_t scissorCount, const VkRect2D *pScissors)
    {
        vkCmdSetScissor(cmd_buffer_ptr_.get(), firstScissor, scissorCount, pScissors);
        return *this;
    }

    command_buffer &set_scissor_with_count_ext(uint32_t scissorCount, const VkRect2D *pScissors)
    {
        vkCmdSetScissorWithCountEXT(cmd_buffer_ptr_.get(), scissorCount, pScissors);
        return *this;
    }

    command_buffer &set_stencil_compare_mask(VkStencilFaceFlags faceMask, uint32_t compareMask)
    {
        vkCmdSetStencilCompareMask(cmd_buffer_ptr_.get(), faceMask, compareMask);
        return *this;
    }

    command_buffer &set_stencil_op_ext(VkStencilFaceFlags faceMask,
                                       VkStencilOp failOp,
                                       VkStencilOp passOp,
                                       VkStencilOp depthFailOp,
                                       VkCompareOp compareOp)
    {
        vkCmdSetStencilOpEXT(
            cmd_buffer_ptr_.get(), faceMask, failOp, passOp, depthFailOp, compareOp);
        return *this;
    }

    command_buffer &set_stencil_reference(VkStencilFaceFlags faceMask, uint32_t reference)
    {
        vkCmdSetStencilReference(cmd_buffer_ptr_.get(), faceMask, reference);
        return *this;
    }

    command_buffer &set_stencil_test_enable_ext(VkBool32 stencilTestEnable)
    {
        vkCmdSetStencilTestEnableEXT(cmd_buffer_ptr_.get(), stencilTestEnable);
        return *this;
    }

    command_buffer &set_stencil_write_mask(VkStencilFaceFlags faceMask, uint32_t writeMask)
    {
        vkCmdSetStencilWriteMask(cmd_buffer_ptr_.get(), faceMask, writeMask);
        return *this;
    }

    command_buffer &
    set_viewport(uint32_t firstViewport, uint32_t viewportCount, const VkViewport *pViewports)
    {
        vkCmdSetViewport(cmd_buffer_ptr_.get(), firstViewport, viewportCount, pViewports);
        return *this;
    }

    command_buffer &
    set_viewport_shading_rate_palette_nv(uint32_t firstViewport,
                                         uint32_t viewportCount,
                                         const VkShadingRatePaletteNV *pShadingRatePalettes)
    {
        vkCmdSetViewportShadingRatePaletteNV(
            cmd_buffer_ptr_.get(), firstViewport, viewportCount, pShadingRatePalettes);
        return *this;
    }

    command_buffer &set_viewport_w_scaling_nv(uint32_t firstViewport,
                                              uint32_t viewportCount,
                                              const VkViewportWScalingNV *pViewportWScalings)
    {
        vkCmdSetViewportWScalingNV(
            cmd_buffer_ptr_.get(), firstViewport, viewportCount, pViewportWScalings);
        return *this;
    }

    command_buffer &set_viewport_with_count_ext(uint32_t viewportCount,
                                                const VkViewport *pViewports)
    {
        vkCmdSetViewportWithCountEXT(cmd_buffer_ptr_.get(), viewportCount, pViewports);
        return *this;
    }

    command_buffer &trace_rays_nv(VkBuffer raygenShaderBindingTableBuffer,
                                  VkDeviceSize raygenShaderBindingOffset,
                                  VkBuffer missShaderBindingTableBuffer,
                                  VkDeviceSize missShaderBindingOffset,
                                  VkDeviceSize missShaderBindingStride,
                                  VkBuffer hitShaderBindingTableBuffer,
                                  VkDeviceSize hitShaderBindingOffset,
                                  VkDeviceSize hitShaderBindingStride,
                                  VkBuffer callableShaderBindingTableBuffer,
                                  VkDeviceSize callableShaderBindingOffset,
                                  VkDeviceSize callableShaderBindingStride,
                                  uint32_t width,
                                  uint32_t height,
                                  uint32_t depth)
    {
        vkCmdTraceRaysNV(cmd_buffer_ptr_.get(),
                         raygenShaderBindingTableBuffer,
                         raygenShaderBindingOffset,
                         missShaderBindingTableBuffer,
                         missShaderBindingOffset,
                         missShaderBindingStride,
                         hitShaderBindingTableBuffer,
                         hitShaderBindingOffset,
                         hitShaderBindingStride,
                         callableShaderBindingTableBuffer,
                         callableShaderBindingOffset,
                         callableShaderBindingStride,
                         width,
                         height,
                         depth);
        return *this;
    }

    command_buffer &update_buffer(VkBuffer dstBuffer,
                                  VkDeviceSize dstOffset,
                                  VkDeviceSize dataSize,
                                  const void *pData)
    {
        vkCmdUpdateBuffer(cmd_buffer_ptr_.get(), dstBuffer, dstOffset, dataSize, pData);
        return *this;
    }

    command_buffer &wait_events(uint32_t eventCount,
                                const VkEvent *pEvents,
                                VkPipelineStageFlags srcStageMask,
                                VkPipelineStageFlags dstStageMask,
                                uint32_t memoryBarrierCount,
                                const VkMemoryBarrier *pMemoryBarriers,
                                uint32_t bufferMemoryBarrierCount,
                                const VkBufferMemoryBarrier *pBufferMemoryBarriers,
                                uint32_t imageMemoryBarrierCount,
                                const VkImageMemoryBarrier *pImageMemoryBarriers)
    {
        vkCmdWaitEvents(cmd_buffer_ptr_.get(),
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
        return *this;
    }

    command_buffer &write_acceleration_structures_properties_khr(
        uint32_t accelerationStructureCount,
        const VkAccelerationStructureKHR *pAccelerationStructures,
        VkQueryType queryType,
        VkQueryPool queryPool,
        uint32_t firstQuery)
    {
        vkCmdWriteAccelerationStructuresPropertiesKHR(cmd_buffer_ptr_.get(),
                                                      accelerationStructureCount,
                                                      pAccelerationStructures,
                                                      queryType,
                                                      queryPool,
                                                      firstQuery);
        return *this;
    }

    command_buffer &write_acceleration_structures_properties_nv(
        uint32_t accelerationStructureCount,
        const VkAccelerationStructureKHR *pAccelerationStructures,
        VkQueryType queryType,
        VkQueryPool queryPool,
        uint32_t firstQuery)
    {
        vkCmdWriteAccelerationStructuresPropertiesNV(cmd_buffer_ptr_.get(),
                                                     accelerationStructureCount,
                                                     pAccelerationStructures,
                                                     queryType,
                                                     queryPool,
                                                     firstQuery);
        return *this;
    }

    command_buffer &write_buffer_marker_amd(VkPipelineStageFlagBits pipelineStage,
                                            VkBuffer dstBuffer,
                                            VkDeviceSize dstOffset,
                                            uint32_t marker)
    {
        vkCmdWriteBufferMarkerAMD(
            cmd_buffer_ptr_.get(), pipelineStage, dstBuffer, dstOffset, marker);
        return *this;
    }

    command_buffer &
    write_timestamp(VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query)
    {
        vkCmdWriteTimestamp(cmd_buffer_ptr_.get(), pipelineStage, queryPool, query);
        return *this;
    }

  private:
    std::shared_ptr<VkCommandBuffer_T> cmd_buffer_ptr_;

    command_pool pool_;
};

class command_buffer_builder {
  public:
    friend class command_buffer;
    command_buffer_builder(graphics_context &context, cory::vk::command_pool &&pool)
        : ctx_{context}
        , command_pool_{pool}
    {
        info_.commandPool = command_pool_.get();
    }

    command_buffer_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    command_buffer_builder &level(VkCommandBufferLevel level) noexcept
    {
        info_.level = level;
        return *this;
    }

    // NOTE: for simplicity, we only permit to build one buffer at a time for now
    // command_buffer_allocate_info_builder &command_buffer_count(uint32_t commandBufferCount)
    // noexcept
    //{
    //    info_.commandBufferCount = commandBufferCount;
    //    return *this;
    //}

    [[nodiscard]] command_buffer_builder create() { return command_buffer_builder(*this); }

  private:
    graphics_context &ctx_;
    VkCommandBufferAllocateInfo info_{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                      .commandBufferCount = 1};
    cory::vk::command_pool command_pool_;
};

class submit_info_builder {
  public:
    submit_info_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    submit_info_builder &wait_semaphores(std::vector<VkSemaphore> waitSemaphores) noexcept
    {
        wait_semaphores_ = waitSemaphores;
        return *this;
    }

    submit_info_builder &wait_dst_stage_mask(const VkPipelineStageFlags *pWaitDstStageMask) noexcept
    {
        info_.pWaitDstStageMask = pWaitDstStageMask;
        return *this;
    }

    submit_info_builder &command_buffers(std::vector<VkCommandBuffer> commandBuffers) noexcept
    {
        command_buffers_ = commandBuffers;
        return *this;
    }

    submit_info_builder &signal_semaphores(std::vector<VkSemaphore> signalSemaphores) noexcept
    {
        signal_semaphores_ = signalSemaphores;
        return *this;
    }

    [[nodiscard]] VkSubmitInfo *info() noexcept
    {
        info_.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores_.size());
        info_.pWaitSemaphores = wait_semaphores_.data();

        info_.signalSemaphoreCount = static_cast<uint32_t>(signal_semaphores_.size());
        info_.pSignalSemaphores = signal_semaphores_.data();

        info_.commandBufferCount = static_cast<uint32_t>(command_buffers_.size());
        info_.pCommandBuffers = command_buffers_.data();

        return &info_;
    }

  private:
    VkSubmitInfo info_{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    };
    std::vector<VkSemaphore> wait_semaphores_;
    std::vector<VkCommandBuffer> command_buffers_;
    std::vector<VkSemaphore> signal_semaphores_;
};

} // namespace vk
} // namespace cory