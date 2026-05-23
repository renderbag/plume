//
// plume
//
// Copyright (c) 2026 renderbag and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE file for details.
//

#pragma once

#include "plume_render_interface_types.h"

#ifdef __cplusplus
#include "plume_render_interface.h"
using namespace plume;

extern "C" {
#else
typedef struct RenderBufferFormattedView RenderBufferFormattedView;
typedef struct RenderBuffer RenderBuffer;
typedef struct RenderTextureView RenderTextureView;
typedef struct RenderTexture RenderTexture;
typedef struct RenderAccelerationStructure RenderAccelerationStructure;
typedef struct RenderShader RenderShader;
typedef struct RenderSampler RenderSampler;
typedef struct RenderPipeline RenderPipeline;
typedef struct RenderPipelineLayout RenderPipelineLayout;
typedef struct RenderCommandFence RenderCommandFence;
typedef struct RenderCommandSemaphore RenderCommandSemaphore;
typedef struct RenderDescriptorSet RenderDescriptorSet;
typedef struct RenderSwapChain RenderSwapChain;
typedef struct RenderFramebuffer RenderFramebuffer;
typedef struct RenderCommandList RenderCommandList;
typedef struct RenderCommandQueue RenderCommandQueue;
typedef struct RenderPool RenderPool;
typedef struct RenderQueryPool RenderQueryPool;
typedef struct RenderDevice RenderDevice;
typedef struct RenderInterface RenderInterface;
#endif

// RenderBufferFormattedView
void plume_destroy_buffer_formatted_view(RenderBufferFormattedView *view);

// RenderBuffer
void plume_destroy_render_buffer(RenderBuffer *buffer);
void *plume_render_buffer_map(RenderBuffer *buffer, uint32_t subresource, const RenderRange *readRange);
void plume_render_buffer_unmap(RenderBuffer *buffer, uint32_t subresource, const RenderRange *writtenRange);
RenderBufferFormattedView *plume_render_buffer_create_buffer_formatted_view(RenderBuffer *buffer, RenderFormat format);
void plume_render_buffer_set_name(RenderBuffer *buffer, const char *name);
uint64_t plume_render_buffer_get_device_address(RenderBuffer *buffer);

// RenderTextureView
void plume_destroy_render_texture_view(RenderTextureView *view);

// RenderTexture
void plume_destroy_render_texture(RenderTexture *texture);
RenderTextureView *plume_render_texture_create_texture_view(RenderTexture *texture, const RenderTextureViewDesc *desc);
void plume_render_texture_set_name(RenderTexture *texture, const char *name);

// RenderAccelerationStructure
void plume_destroy_render_acceleration_structure(RenderAccelerationStructure *accelerationStructure);

// RenderShader
void plume_destroy_render_shader(RenderShader *shader);
void plume_render_shader_set_name(RenderShader *shader, const char *name);

// RenderSampler
void plume_destroy_render_sampler(RenderSampler *sampler);

// RenderPipeline
void plume_destroy_render_pipeline(RenderPipeline *pipeline);
void plume_render_pipeline_set_name(RenderPipeline *pipeline, const char *name);
RenderPipelineProgram plume_render_pipeline_get_program(RenderPipeline *pipeline, const char *name);

// RenderPipelineLayout
void plume_destroy_render_pipeline_layout(RenderPipelineLayout *layout);

// RenderCommandFence
void plume_destroy_render_command_fence(RenderCommandFence *fence);

// RenderCommandSemaphore
void plume_destroy_render_command_semaphore(RenderCommandSemaphore *semaphore);

// RenderDescriptorSet
void plume_destroy_render_descriptor_set(RenderDescriptorSet *set);
void plume_render_descriptor_set_set_buffer(RenderDescriptorSet *set, uint32_t descriptorIndex, const RenderBuffer *buffer, uint64_t bufferSize, const RenderBufferStructuredView *bufferStructuredView, const RenderBufferFormattedView *bufferFormattedView);
void plume_render_descriptor_set_set_texture(RenderDescriptorSet *set, uint32_t descriptorIndex, const RenderTexture *texture, const RenderTextureLayout textureLayout, RenderTextureView *textureView);
void plume_render_descriptor_set_set_sampler(RenderDescriptorSet *set, uint32_t descriptorIndex, const RenderSampler *sampler);
void plume_render_descriptor_set_set_acceleration_structure(RenderDescriptorSet *set, uint32_t descriptorIndex, const RenderAccelerationStructure *accelerationStructure);

// RenderSwapChain
void plume_destroy_render_swapchain(RenderSwapChain *swapChain);
bool plume_render_swapchain_present(RenderSwapChain *swapChain, uint32_t textureIndex, RenderCommandSemaphore **waitSemaphores, uint32_t waitSemaphoreCount);
void plume_render_swapchain_wait(RenderSwapChain *swapChain);
bool plume_render_swapchain_resize(RenderSwapChain *swapChain);
bool plume_render_swapchain_needs_resize(RenderSwapChain *swapChain);
void plume_render_swapchain_set_vsync_enabled(RenderSwapChain *swapChain, bool vsyncEnabled);
bool plume_render_swapchain_is_vsync_enabled(RenderSwapChain *swapChain);
uint32_t plume_render_swapchain_get_width(RenderSwapChain *swapChain);
uint32_t plume_render_swapchain_get_height(RenderSwapChain *swapChain);
RenderTexture *plume_render_swapchain_get_texture(RenderSwapChain *swapChain, uint32_t textureIndex);
uint32_t plume_render_swapchain_get_texture_count(RenderSwapChain *swapChain);
bool plume_render_swapchain_acquire_texture(RenderSwapChain *swapChain, RenderCommandSemaphore *signalSemaphore, uint32_t *textureIndex);
RenderWindow plume_render_swapchain_get_window(RenderSwapChain *swapChain);
bool plume_render_swapchain_is_empty(RenderSwapChain *swapChain);
uint32_t plume_render_swapchain_get_refresh_rate(RenderSwapChain *swapChain);

// RenderFramebuffer
void plume_destroy_render_framebuffer(RenderFramebuffer *framebuffer);
uint32_t plume_render_framebuffer_get_width(RenderFramebuffer *framebuffer);
uint32_t plume_render_framebuffer_get_height(RenderFramebuffer *framebuffer);

// RenderCommandList
void plume_destroy_render_command_list(RenderCommandList *list);
void plume_render_command_list_begin(RenderCommandList *list);
void plume_render_command_list_end(RenderCommandList *list);
void plume_render_command_list_barriers(RenderCommandList *list, RenderBarrierStages stages, const RenderBufferBarrier *bufferBarriers, uint32_t bufferBarriersCount, const RenderTextureBarrier *textureBarriers, uint32_t textureBarrierCount);
void plume_render_command_list_dispatch(RenderCommandList *list, uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ);
void plume_render_command_list_trace_rays(RenderCommandList *list, uint32_t width, uint32_t height, uint32_t depth, RenderBufferReference shaderBindingTable, const RenderShaderBindingGroupsInfo *shaderBindingGroupsInfo);
void plume_render_command_list_draw_instanced(RenderCommandList *list, uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation);
void plume_render_commnad_list_draw_indexed_instanced(RenderCommandList *list, uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation, uint32_t baseVertexLocation, uint32_t startInstanceLocation);
void plume_render_command_list_set_pipeline(RenderCommandList *list, const RenderPipeline *pipeline);
void plume_render_command_list_set_compute_pipeline_layout(RenderCommandList *list, const RenderPipelineLayout *pipelineLayout);
void plume_render_command_list_set_compute_push_constants(RenderCommandList *list, uint32_t rangeIndex, const void* data, uint32_t offset, uint32_t size);
void plume_render_command_list_set_compute_descriptor_set(RenderCommandList *list, RenderDescriptorSet *descriptorSet, uint32_t setIndex);
void plume_render_command_list_set_graphics_pipeline_layout(RenderCommandList *list, const RenderPipelineLayout *pipelineLayout);
void plume_render_command_list_set_graphics_push_constants(RenderCommandList *list, uint32_t rangeIndex, const void* data, uint32_t offset, uint32_t size);
void plume_render_command_list_set_graphics_descriptor_set(RenderCommandList *list, RenderDescriptorSet *descriptorSet, uint32_t setIndex);
void plume_render_command_list_set_graphics_root_descriptor(RenderCommandList *list, RenderBufferReference *bufferReference, uint32_t rootDescriptorIndex);
void plume_render_command_list_set_raytracing_pipeline_layout(RenderCommandList *list, const RenderPipelineLayout *pipelineLayout);
void plume_render_command_list_set_raytracing_push_constants(RenderCommandList *list, uint32_t rangeIndex, const void *data, uint32_t offset, uint32_t size);
void plume_render_command_list_set_raytracing_descriptor_set(RenderCommandList *list, RenderDescriptorSet *descriptorSet, uint32_t setIndex);
void plume_render_command_list_set_index_buffer(RenderCommandList *list, const RenderIndexBufferView *view);
void plume_render_command_list_set_vertex_buffers(RenderCommandList *list, uint32_t startSlot, const RenderVertexBufferView *views, uint32_t viewCount, const RenderInputSlot *inputSlots);
void plume_render_command_list_set_viewports(RenderCommandList *list, const RenderViewport *viewports, uint32_t count);
void plume_render_command_list_set_scissors(RenderCommandList *list, const RenderRect *scissorRects, uint32_t count);
void plume_render_command_list_set_framebuffer(RenderCommandList *list, const RenderFramebuffer *framebuffer);
void plume_render_command_list_set_depth_bias(RenderCommandList *list, float depthBias, float depthBiasClamp, float slopeScaledDepthBias);
void plume_render_command_list_clear_color(RenderCommandList *list, uint32_t attachmentIndex, RenderColor colorView, const RenderRect *clearRects, uint32_t clearReactsCount);
void plume_render_command_list_clear_depth_stencil(RenderCommandList *list, bool clearDepth, bool clearStencil, float depthValue, uint32_t stencilValue, const RenderRect *clearRects, uint32_t clearRectsCount);
void plume_render_commnad_list_copy_buffer_region(RenderCommandList *list, RenderBufferReference dstBuffer, RenderBufferReference srcBuffer, uint64_t size);
void plume_render_command_list_copy_texture_region(RenderCommandList *list, const RenderTextureCopyLocation *dstLocation, const RenderTextureCopyLocation *srcLocation, uint32_t dstX, uint32_t dstY, uint32_t dstZ, const RenderBox *srcBox);
void plume_render_command_list_copy_buffer(RenderCommandList *list, const RenderBuffer *dstBuffer, const RenderBuffer *srcBuffer);
void plume_render_command_list_copy_texture(RenderCommandList *list, const RenderTexture *dstTexture, const RenderTexture *srcTexture);
void plume_render_command_list_resolve_texture(RenderCommandList *list, const RenderTexture *dstTexture, const RenderTexture *srcTexture);
void plume_render_command_list_resolve_texture_region(RenderCommandList *list, const RenderTexture *dstTexture, uint32_t dstX, uint32_t dstY, const RenderTexture *srcTexture, const RenderRect *srcRect, RenderResolveMode resolveMode);
void plume_render_command_list_build_bottom_level_as(RenderCommandList *list, const RenderAccelerationStructure *dstAccelerationStructure, RenderBufferReference scratchBuffer, const RenderBottomLevelASBuildInfo *buildInfo);
void plume_render_command_list_build_top_level_as(RenderCommandList *list, const RenderAccelerationStructure *dstAccelerationStructure, RenderBufferReference scratchBuffer, RenderBufferReference instanceBuffer, const RenderTopLevelASBuildInfo *buildInfo);
void plume_render_command_list_discard_texture(RenderCommandList *list, const RenderTexture *texture); // D3D12 only.
void plume_render_command_list_reset_query_pool(RenderCommandList *list, const RenderQueryPool *queryPool, uint32_t queryFirstIndex, uint32_t queryCount);
void plume_render_command_list_write_timestamp(RenderCommandList *list, const RenderQueryPool *queryPool, uint32_t queryIndex);

// Concrete implementation shortcuts.
void plume_render_command_list_buffer_barrier(RenderCommandList *list, RenderBarrierStages stages, const RenderBufferBarrier *barrier) {
    plume_render_command_list_barriers(list, stages, barrier, 1, 0, 0);
}

void plume_render_command_list_texture_barrier(RenderCommandList *list, RenderBarrierStages stages, const RenderTextureBarrier *barrier) {
    plume_render_command_list_barriers(list, stages, 0, 0, barrier, 1);
}

void plume_render_command_list_buffer_texture_barrier(RenderCommandList *list, RenderBarrierStages stages, const RenderBufferBarrier *bufferBarrier, const RenderTextureBarrier *textureBarrier) {
    plume_render_command_list_barriers(list, stages, bufferBarrier, 1, textureBarrier, 1);
}

void plume_render_command_list_buffer_barriers(RenderCommandList *list, RenderBarrierStages stages, const RenderBufferBarrier *bufferBarriers, uint32_t bufferBarriersCount) {
    plume_render_command_list_barriers(list, stages, bufferBarriers, bufferBarriersCount, 0, 0);
}

void plume_render_command_list_texture_barriers(RenderCommandList *list, RenderBarrierStages stages, const RenderTextureBarrier *textureBarriers, uint32_t textureBarriersCount) {
    plume_render_command_list_barriers(list, stages, 0, 0, textureBarriers, textureBarriersCount);
}

void plume_render_command_list_set_viewport(RenderCommandList *list, const RenderViewport *viewport) {
    plume_render_command_list_set_viewports(list, viewport, 1);
}

void plume_render_command_list_set_scissor(RenderCommandList *list, const RenderRect *scissorRect) {
    plume_render_command_list_set_scissors(list, scissorRect, 1);
}

void plume_render_command_list_clear_depth(RenderCommandList *list, bool clearDepth, float depthValue, const RenderRect *clearRects, uint32_t clearRectsCount) {
    plume_render_command_list_clear_depth_stencil(list, clearDepth, false, depthValue, 0, clearRects, clearRectsCount);
}

// RenderCommandQueue
void plume_destroy_render_command_queue(RenderCommandQueue *queue);
RenderCommandList *plume_render_command_queue_create_command_list(RenderCommandQueue *queue);
RenderSwapChain *plume_render_command_queue_create_swapchain(RenderCommandQueue *queue, const RenderSwapChainDesc *desc);
void plume_render_command_queue_execute_command_lists(RenderCommandQueue *queue, const RenderCommandList **commandLists, uint32_t commandListCount, RenderCommandSemaphore **waitSemaphores, uint32_t waitSemaphoreCount, RenderCommandSemaphore **signalSemaphores, uint32_t signalSemaphoreCount, RenderCommandFence *signalFence);
void plume_render_command_queue_wait_for_command_queue(RenderCommandQueue *queue, RenderCommandFence *fence);

// Concrete implementation shortcuts.
void plume_render_command_queue_execute_command_list(RenderCommandQueue *queue, const RenderCommandList *commandList, RenderCommandFence *signalFence) {
    plume_render_command_queue_execute_command_lists(queue, &commandList, 1, 0, 0, 0, 0, signalFence);
}

// RenderPool
void plume_destroy_render_pool(RenderPool *pool);
RenderBuffer *plume_render_pool_create_buffer(RenderPool *pool, const RenderBufferDesc *desc);
RenderTexture *plume_render_pool_create_texture(RenderPool *pool, const RenderTextureDesc *desc);

// RenderQueryPool
void plume_destroy_render_query_pool(RenderQueryPool *pool);
void plume_render_query_pool_query_results(RenderQueryPool *pool);
const uint64_t *plume_render_query_pool_get_results(RenderQueryPool *pool);
uint32_t plume_render_query_pool_get_count(RenderQueryPool *pool);

// RenderDevice
void plume_destroy_render_device(RenderDevice *device);
RenderDescriptorSet *plume_render_device_create_descriptor_set(RenderDevice *device, const RenderDescriptorSetDesc *desc);
RenderShader *plume_render_device_create_shader(RenderDevice *device, const void *data, uint64_t size, const char *entryPointName, RenderShaderFormat format);
RenderSampler *plume_render_device_create_sampler(RenderDevice *device, const RenderSamplerDesc *desc);
RenderPipeline *plume_render_device_create_compute_pipeline(RenderDevice *device, const RenderComputePipelineDesc *desc);
RenderPipeline *plume_render_device_create_graphics_pipeline(RenderDevice *device, const RenderGraphicsPipelineDesc *desc);
RenderPipeline *plume_render_device_create_raytracing_pipeline(RenderDevice *device, const RenderRaytracingPipelineDesc *desc, const RenderPipeline *previousPipeline);
RenderCommandQueue *plume_render_device_create_command_queue(RenderDevice *device, RenderCommandListType type);
RenderBuffer *plume_render_device_create_buffer(RenderDevice *device, const RenderBufferDesc *desc);
RenderTexture *plume_render_device_create_texture(RenderDevice *device, const RenderTextureDesc *desc);
RenderAccelerationStructure *plume_render_device_create_acceleration_structure(RenderDevice *device, const RenderAccelerationStructureDesc *desc);
RenderPool *plume_render_device_create_pool(RenderDevice *device, const RenderPoolDesc *desc);
RenderPipelineLayout *plume_render_device_create_pipeline_layout(RenderDevice *device, const RenderPipelineLayoutDesc *desc);
RenderCommandFence *plume_render_device_create_command_fence(RenderDevice *device);
RenderCommandSemaphore *plume_render_device_create_command_semaphore(RenderDevice *device);
RenderFramebuffer *plume_render_device_create_framebuffer(RenderDevice *device, const RenderFramebufferDesc *desc);
RenderQueryPool *plume_render_device_create_query_pool(RenderDevice *device, uint32_t queryCount);
void plume_render_device_set_bottom_level_as_build_info(RenderDevice *device, RenderBottomLevelASBuildInfo *buildInfo, const RenderBottomLevelASMesh *meshes, uint32_t meshCount, bool preferFastBuild, bool preferFastTrace);
void plume_render_device_set_top_level_as_build_info(RenderDevice *device, RenderTopLevelASBuildInfo *buildInfo, const RenderTopLevelASInstance *instances, uint32_t instanceCount, bool preferFastBuild, bool preferFastTrace);
void plume_render_device_set_shader_binding_table_info(RenderDevice *device, RenderShaderBindingTableInfo *tableInfo, const RenderShaderBindingGroups *groups, const RenderPipeline *pipeline, RenderDescriptorSet **descriptorSets, uint32_t descriptorSetCount);
const RenderDeviceCapabilities *plume_render_device_get_capabilities(RenderDevice *device);
const RenderDeviceDescription *plume_render_device_get_description(RenderDevice *device);
RenderSampleCounts plume_render_device_get_sample_counts_supported(RenderDevice *device, RenderFormat format);
bool plume_render_device_begin_capture(RenderDevice *device);
bool plume_render_device_end_capture(RenderDevice *device);

// RenderInterface
RenderInterface *plume_create_d3d12_interface(void);
#if PLUME_SDL_VULKAN_ENABLED
RenderInterface *plume_create_vulkan_interface(RenderWindow sdlWindow);
#else
RenderInterface *plume_create_vulkan_interface(void);
#endif
RenderInterface *plume_create_metal_interface(void);
void plume_destroy_render_interface(RenderInterface *renderInterface);
RenderDevice *plume_render_interface_create_device(RenderInterface *renderInterface, const char *preferredDeviceName);
const char **plume_render_interface_get_device_names(RenderInterface *renderInterface);
const RenderInterfaceCapabilities *plume_render_interface_get_capabilities(RenderInterface *renderInterface);

#ifdef __cplusplus
}
#endif
