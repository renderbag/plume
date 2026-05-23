//
// plume
//
// Copyright (c) 2026 renderbag and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE file for details.
//

#include "plume_c.h"

#if defined(_WIN64)
#include "plume_d3d12.h"
#elif defined(__APPLE__)
#include "plume_metal.h"
#endif

#include "plume_vulkan.h"

// RenderBufferFormattedView
void plume_destroy_buffer_formatted_view(RenderBufferFormattedView *view) {
    delete view;
}

// RenderBuffer
void plume_destroy_render_buffer(RenderBuffer *buffer) {
    delete buffer;
}

void *plume_render_buffer_map(RenderBuffer *buffer, uint32_t subresource, const RenderRange *readRange) {
    return buffer->map(subresource, readRange);
}

void plume_render_buffer_unmap(RenderBuffer *buffer, uint32_t subresource, const RenderRange *writtenRange) {
    buffer->unmap(subresource, writtenRange);
}

RenderBufferFormattedView *plume_render_buffer_create_buffer_formatted_view(RenderBuffer *buffer, RenderFormat format) {
    return buffer->createBufferFormattedView(format).release();
}

void plume_render_buffer_set_name(RenderBuffer *buffer, const char *name) {
    buffer->setName(name);
}

uint64_t plume_render_buffer_get_device_address(RenderBuffer *buffer) {
    return buffer->getDeviceAddress();
}

// RenderTextureView
void plume_destroy_render_texture_view(RenderTextureView *view) {
    delete view;
}

// RenderTexture
void plume_destroy_render_texture(RenderTexture *texture) {
    delete texture;
}

RenderTextureView *plume_render_texture_create_texture_view(RenderTexture *texture, const RenderTextureViewDesc *desc) {
    return texture->createTextureView(*desc).release();
}

void plume_render_texture_set_name(RenderTexture *texture, const char *name) {
    texture->setName(name);
}

// RenderAccelerationStructure
void plume_destroy_render_acceleration_structure(RenderAccelerationStructure *accelerationStructure) {
    delete accelerationStructure;
}

// RenderShader
void plume_destroy_render_shader(RenderShader *shader) {
    delete shader;
}

void plume_render_shader_set_name(RenderShader *shader, const char *name) {
    shader->setName(name);
}

// RenderSampler
void plume_destroy_render_sampler(RenderSampler *sampler) {
    delete sampler;
}

// RenderPipeline
void plume_destroy_render_pipeline(RenderPipeline *pipeline) {
    delete pipeline;
}

void plume_render_pipeline_set_name(RenderPipeline *pipeline, const char *name) {
    pipeline->setName(name);
}

RenderPipelineProgram plume_render_pipeline_get_program(RenderPipeline *pipeline, const char *name) {
    return pipeline->getProgram(name);
}

// RenderPipelineLayout
void plume_destroy_render_pipeline_layout(RenderPipelineLayout *layout) {
    delete layout;
}

// RenderCommandFence
void plume_destroy_render_command_fence(RenderCommandFence *fence) {
    delete fence;
}

// RenderCommandSemaphore
void plume_destroy_render_command_semaphore(RenderCommandSemaphore *semaphore) {
    delete semaphore;
}

// RenderDescriptorSet
void plume_destroy_render_descriptor_set(RenderDescriptorSet *set) {
    delete set;
}

void plume_render_descriptor_set_set_buffer(RenderDescriptorSet *set, uint32_t descriptorIndex, const RenderBuffer *buffer, uint64_t bufferSize, const RenderBufferStructuredView *bufferStructuredView, const RenderBufferFormattedView *bufferFormattedView) {
    set->setBuffer(descriptorIndex, buffer, bufferSize, bufferStructuredView, bufferFormattedView);
}

void plume_render_descriptor_set_set_texture(RenderDescriptorSet *set, uint32_t descriptorIndex, const RenderTexture *texture, const RenderTextureLayout textureLayout, RenderTextureView *textureView) {
    set->setTexture(descriptorIndex, texture, textureLayout, textureView);
}

void plume_render_descriptor_set_set_sampler(RenderDescriptorSet *set, uint32_t descriptorIndex, const RenderSampler *sampler) {
    set->setSampler(descriptorIndex, sampler);
}

void plume_render_descriptor_set_set_acceleration_structure(RenderDescriptorSet *set, uint32_t descriptorIndex, const RenderAccelerationStructure *accelerationStructure) {
    set->setAccelerationStructure(descriptorIndex, accelerationStructure);
}

// RenderSwapChain
void plume_destroy_render_swapchain(RenderSwapChain *swapChain) {
    delete swapChain;
}

bool plume_render_swapchain_present(RenderSwapChain *swapChain, uint32_t textureIndex, RenderCommandSemaphore **waitSemaphores, uint32_t waitSemaphoreCount) {
    return swapChain->present(textureIndex, waitSemaphores, waitSemaphoreCount);
}

void plume_render_swapchain_wait(RenderSwapChain *swapChain) {
    swapChain->wait();
}

bool plume_render_swapchain_resize(RenderSwapChain *swapChain) {
    return swapChain->resize();
}

bool plume_render_swapchain_needs_resize(RenderSwapChain *swapChain) {
    return swapChain->needsResize();
}

void plume_render_swapchain_set_vsync_enabled(RenderSwapChain *swapChain, bool vsyncEnabled) {
    swapChain->setVsyncEnabled(vsyncEnabled);
}

bool plume_render_swapchain_is_vsync_enabled(RenderSwapChain *swapChain) {
    return swapChain->isVsyncEnabled();
}

uint32_t plume_render_swapchain_get_width(RenderSwapChain *swapChain) {
    return swapChain->getWidth();
}

uint32_t plume_render_swapchain_get_height(RenderSwapChain *swapChain) {
    return swapChain->getHeight();
}

RenderTexture *plume_render_swapchain_get_texture(RenderSwapChain *swapChain, uint32_t textureIndex) {
    return swapChain->getTexture(textureIndex);
}

uint32_t plume_render_swapchain_get_texture_count(RenderSwapChain *swapChain) {
    return swapChain->getTextureCount();
}

bool plume_render_swapchain_acquire_texture(RenderSwapChain *swapChain, RenderCommandSemaphore *signalSemaphore, uint32_t *textureIndex) {
    return swapChain->acquireTexture(signalSemaphore, textureIndex);
}

RenderWindow plume_render_swapchain_get_window(RenderSwapChain *swapChain) {
    return swapChain->getWindow();
}

bool plume_render_swapchain_is_empty(RenderSwapChain *swapChain) {
    return swapChain->isEmpty();
}

uint32_t plume_render_swapchain_get_refresh_rate(RenderSwapChain *swapChain) {
    return swapChain->getRefreshRate();
}

// RenderFramebuffer
void plume_destroy_render_framebuffer(RenderFramebuffer *framebuffer) {
    delete framebuffer;
}

uint32_t plume_render_framebuffer_get_width(RenderFramebuffer *framebuffer) {
    return framebuffer->getWidth();
}

uint32_t plume_render_framebuffer_get_height(RenderFramebuffer *framebuffer) {
    return framebuffer->getHeight();
}

// RenderCommandList
void plume_destroy_render_command_list(RenderCommandList *list) {
    delete list;
}

void plume_render_command_list_begin(RenderCommandList *list) {
    list->begin();
}

void plume_render_command_list_end(RenderCommandList *list) {
    list->end();
}

void plume_render_command_list_barriers(RenderCommandList *list, RenderBarrierStages stages, const RenderBufferBarrier *bufferBarriers, uint32_t bufferBarriersCount, const RenderTextureBarrier *textureBarriers, uint32_t textureBarrierCount) {
    list->barriers(stages, bufferBarriers, bufferBarriersCount, textureBarriers, textureBarrierCount);
}

void plume_render_command_list_dispatch(RenderCommandList *list, uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ) {
    list->dispatch(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
}

void plume_render_command_list_trace_rays(RenderCommandList *list, uint32_t width, uint32_t height, uint32_t depth, RenderBufferReference shaderBindingTable, const RenderShaderBindingGroupsInfo *shaderBindingGroupsInfo) {
    list->traceRays(width, height, depth, shaderBindingTable, *shaderBindingGroupsInfo);
}

void plume_render_command_list_draw_instanced(RenderCommandList *list, uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation) {
    list->drawInstanced(vertexCountPerInstance, instanceCount, startVertexLocation, startInstanceLocation);
}

void plume_render_commnad_list_draw_indexed_instanced(RenderCommandList *list, uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation, uint32_t baseVertexLocation, uint32_t startInstanceLocation) {
    list->drawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
}

void plume_render_command_list_set_pipeline(RenderCommandList *list, const RenderPipeline *pipeline) {
    list->setPipeline(pipeline);
}

void plume_render_command_list_set_compute_pipeline_layout(RenderCommandList *list, const RenderPipelineLayout *pipelineLayout) {
    list->setComputePipelineLayout(pipelineLayout);
}

void plume_render_command_list_set_compute_push_constants(RenderCommandList *list, uint32_t rangeIndex, const void* data, uint32_t offset, uint32_t size) {
    list->setComputePushConstants(rangeIndex, data, offset, size);
}

void plume_render_command_list_set_compute_descriptor_set(RenderCommandList *list, RenderDescriptorSet *descriptorSet, uint32_t setIndex) {
    list->setComputeDescriptorSet(descriptorSet, setIndex);
}

void plume_render_command_list_set_graphics_pipeline_layout(RenderCommandList *list, const RenderPipelineLayout *pipelineLayout) {
    list->setGraphicsPipelineLayout(pipelineLayout);
}

void plume_render_command_list_set_graphics_push_constants(RenderCommandList *list, uint32_t rangeIndex, const void* data, uint32_t offset, uint32_t size) {
    list->setGraphicsPushConstants(rangeIndex, data, offset, size);
}

void plume_render_command_list_set_graphics_descriptor_set(RenderCommandList *list, RenderDescriptorSet *descriptorSet, uint32_t setIndex) {
    list->setGraphicsDescriptorSet(descriptorSet, setIndex);
}

void plume_render_command_list_set_graphics_root_descriptor(RenderCommandList *list, RenderBufferReference *bufferReference, uint32_t rootDescriptorIndex) {
    list->setGraphicsRootDescriptor(*bufferReference, rootDescriptorIndex);
}

void plume_render_command_list_set_raytracing_pipeline_layout(RenderCommandList *list, const RenderPipelineLayout *pipelineLayout) {
    list->setRaytracingPipelineLayout(pipelineLayout);
}

void plume_render_command_list_set_raytracing_push_constants(RenderCommandList *list, uint32_t rangeIndex, const void *data, uint32_t offset, uint32_t size) {
    list->setRaytracingPushConstants(rangeIndex, data, offset, size);
}

void plume_render_command_list_set_raytracing_descriptor_set(RenderCommandList *list, RenderDescriptorSet *descriptorSet, uint32_t setIndex) {
    list->setRaytracingDescriptorSet(descriptorSet, setIndex);
}

void plume_render_command_list_set_index_buffer(RenderCommandList *list, const RenderIndexBufferView *view) {
    list->setIndexBuffer(view);
}

void plume_render_command_list_set_vertex_buffers(RenderCommandList *list, uint32_t startSlot, const RenderVertexBufferView *views, uint32_t viewCount, const RenderInputSlot *inputSlots) {
    list->setVertexBuffers(startSlot, views, viewCount, inputSlots);
}

void plume_render_command_list_set_viewports(RenderCommandList *list, const RenderViewport *viewports, uint32_t count) {
    list->setViewports(viewports, count);
}

void plume_render_command_list_set_scissors(RenderCommandList *list, const RenderRect *scissorRects, uint32_t count) {
    list->setScissors(scissorRects, count);
}

void plume_render_command_list_set_framebuffer(RenderCommandList *list, const RenderFramebuffer *framebuffer) {
    list->setFramebuffer(framebuffer);
}

void plume_render_command_list_set_depth_bias(RenderCommandList *list, float depthBias, float depthBiasClamp, float slopeScaledDepthBias) {
    list->setDepthBias(depthBias, depthBiasClamp, slopeScaledDepthBias);
}

void plume_render_command_list_clear_color(RenderCommandList *list, uint32_t attachmentIndex, RenderColor colorView, const RenderRect *clearRects, uint32_t clearReactsCount) {
    list->clearColor(attachmentIndex, colorView, clearRects, clearReactsCount);
}

void plume_render_command_list_clear_depth_stencil(RenderCommandList *list, bool clearDepth, bool clearStencil, float depthValue, uint32_t stencilValue, const RenderRect *clearRects, uint32_t clearRectsCount) {
    list->clearDepthStencil(clearDepth, clearStencil, depthValue, stencilValue, clearRects, clearRectsCount);
}

void plume_render_commnad_list_copy_buffer_region(RenderCommandList *list, RenderBufferReference dstBuffer, RenderBufferReference srcBuffer, uint64_t size) {
    list->copyBufferRegion(dstBuffer, srcBuffer, size);
}

void plume_render_command_list_copy_texture_region(RenderCommandList *list, const RenderTextureCopyLocation *dstLocation, const RenderTextureCopyLocation *srcLocation, uint32_t dstX, uint32_t dstY, uint32_t dstZ, const RenderBox *srcBox) {
    list->copyTextureRegion(*dstLocation, *srcLocation, dstX, dstY, dstZ, srcBox);
}

void plume_render_command_list_copy_buffer(RenderCommandList *list, const RenderBuffer *dstBuffer, const RenderBuffer *srcBuffer) {
    list->copyBuffer(dstBuffer, srcBuffer);
}

void plume_render_command_list_copy_texture(RenderCommandList *list, const RenderTexture *dstTexture, const RenderTexture *srcTexture) {
    list->copyTexture(dstTexture, srcTexture);
}

void plume_render_command_list_resolve_texture(RenderCommandList *list, const RenderTexture *dstTexture, const RenderTexture *srcTexture) {
    list->resolveTexture(dstTexture, srcTexture);
}

void plume_render_command_list_resolve_texture_region(RenderCommandList *list, const RenderTexture *dstTexture, uint32_t dstX, uint32_t dstY, const RenderTexture *srcTexture, const RenderRect *srcRect, RenderResolveMode resolveMode) {
    list->resolveTextureRegion(dstTexture, dstX, dstY, srcTexture, srcRect, resolveMode);
}

void plume_render_command_list_build_bottom_level_as(RenderCommandList *list, const RenderAccelerationStructure *dstAccelerationStructure, RenderBufferReference scratchBuffer, const RenderBottomLevelASBuildInfo *buildInfo) {
    list->buildBottomLevelAS(dstAccelerationStructure, scratchBuffer, *buildInfo);
}

void plume_render_command_list_build_top_level_as(RenderCommandList *list, const RenderAccelerationStructure *dstAccelerationStructure, RenderBufferReference scratchBuffer, RenderBufferReference instanceBuffer, const RenderTopLevelASBuildInfo *buildInfo) {
    list->buildTopLevelAS(dstAccelerationStructure, scratchBuffer, instanceBuffer, *buildInfo);
}

void plume_render_command_list_discard_texture(RenderCommandList *list, const RenderTexture *texture) {
    list->discardTexture(texture);
}

void plume_render_command_list_reset_query_pool(RenderCommandList *list, const RenderQueryPool *queryPool, uint32_t queryFirstIndex, uint32_t queryCount) {
    list->resetQueryPool(queryPool, queryFirstIndex, queryCount);
}

void plume_render_command_list_write_timestamp(RenderCommandList *list, const RenderQueryPool *queryPool, uint32_t queryIndex) {
    list->writeTimestamp(queryPool, queryIndex);
}

// RenderCommandQueue
void plume_destroy_render_command_queue(RenderCommandQueue *queue) {
    delete queue;
}

RenderCommandList *plume_render_command_queue_create_command_list(RenderCommandQueue *queue) {
    return queue->createCommandList().release();
}

RenderSwapChain *plume_render_command_queue_create_swapchain(RenderCommandQueue *queue, const RenderSwapChainDesc *desc) {
    return queue->createSwapChain(*desc).release();
}

void plume_render_command_queue_execute_command_lists(RenderCommandQueue *queue, const RenderCommandList **commandLists, uint32_t commandListCount, RenderCommandSemaphore **waitSemaphores, uint32_t waitSemaphoreCount, RenderCommandSemaphore **signalSemaphores, uint32_t signalSemaphoreCount, RenderCommandFence *signalFence) {
    queue->executeCommandLists(commandLists, commandListCount, waitSemaphores, waitSemaphoreCount, signalSemaphores, signalSemaphoreCount, signalFence);
}

void plume_render_command_queue_wait_for_command_queue(RenderCommandQueue *queue, RenderCommandFence *fence) {
    queue->waitForCommandFence(fence);
}

// RenderPool
void plume_destroy_render_pool(RenderPool *pool) {
    delete pool;
}

RenderBuffer *plume_render_pool_create_buffer(RenderPool *pool, const RenderBufferDesc *desc) {
    return pool->createBuffer(*desc).release();
}

RenderTexture *plume_render_pool_create_texture(RenderPool *pool, const RenderTextureDesc *desc) {
    return pool->createTexture(*desc).release();
}

// RenderQueryPool
void plume_destroy_render_query_pool(RenderQueryPool *pool) {
    delete pool;
}

void plume_render_query_pool_query_results(RenderQueryPool *pool) {
    pool->queryResults();
}

const uint64_t *plume_render_query_pool_get_results(RenderQueryPool *pool) {
    return pool->getResults();
}

uint32_t plume_render_query_pool_get_count(RenderQueryPool *pool) {
    return pool->getCount();
}

// RenderDevice
void plume_destroy_render_device(RenderDevice *device) {
    delete device;
}

RenderDescriptorSet *plume_render_device_create_descriptor_set(RenderDevice *device, const RenderDescriptorSetDesc *desc) {
    return device->createDescriptorSet(*desc).release();
}

RenderShader *plume_render_device_create_shader(RenderDevice *device, const void *data, uint64_t size, const char *entryPointName, RenderShaderFormat format) {
    return device->createShader(data, size, entryPointName, format).release();
}

RenderSampler *plume_render_device_create_sampler(RenderDevice *device, const RenderSamplerDesc *desc) {
    return device->createSampler(*desc).release();
}

RenderPipeline *plume_render_device_create_compute_pipeline(RenderDevice *device, const RenderComputePipelineDesc *desc) {
    return device->createComputePipeline(*desc).release();
}

RenderPipeline *plume_render_device_create_graphics_pipeline(RenderDevice *device, const RenderGraphicsPipelineDesc *desc) {
    return device->createGraphicsPipeline(*desc).release();
}

RenderPipeline *plume_render_device_create_raytracing_pipeline(RenderDevice *device, const RenderRaytracingPipelineDesc *desc, const RenderPipeline *previousPipeline) {
    return device->createRaytracingPipeline(*desc, previousPipeline).release();
}

RenderCommandQueue *plume_render_device_create_command_queue(RenderDevice *device, RenderCommandListType type) {
    return device->createCommandQueue(type).release();
}

RenderBuffer *plume_render_device_create_buffer(RenderDevice *device, const RenderBufferDesc *desc) {
    return device->createBuffer(*desc).release();
}

RenderTexture *plume_render_device_create_texture(RenderDevice *device, const RenderTextureDesc *desc) {
    return device->createTexture(*desc).release();
}

RenderAccelerationStructure *plume_render_device_create_acceleration_structure(RenderDevice *device, const RenderAccelerationStructureDesc *desc) {
    return device->createAccelerationStructure(*desc).release();
}

RenderPool *plume_render_device_create_pool(RenderDevice *device, const RenderPoolDesc *desc) {
    return device->createPool(*desc).release();
}

RenderPipelineLayout *plume_render_device_create_pipeline_layout(RenderDevice *device, const RenderPipelineLayoutDesc *desc) {
    return device->createPipelineLayout(*desc).release();
}

RenderCommandFence *plume_render_device_create_command_fence(RenderDevice *device) {
    return device->createCommandFence().release();
}

RenderCommandSemaphore *plume_render_device_create_command_semaphore(RenderDevice *device) {
    return device->createCommandSemaphore().release();
}

RenderFramebuffer *plume_render_device_create_framebuffer(RenderDevice *device, const RenderFramebufferDesc *desc) {
    return device->createFramebuffer(*desc).release();
}

RenderQueryPool *plume_render_device_create_query_pool(RenderDevice *device, uint32_t queryCount) {
    return device->createQueryPool(queryCount).release();
}

void plume_render_device_set_bottom_level_as_build_info(RenderDevice *device, RenderBottomLevelASBuildInfo *buildInfo, const RenderBottomLevelASMesh *meshes, uint32_t meshCount, bool preferFastBuild, bool preferFastTrace) {
    device->setBottomLevelASBuildInfo(*buildInfo, meshes, meshCount, preferFastBuild, preferFastTrace);
}

void plume_render_device_set_top_level_as_build_info(RenderDevice *device, RenderTopLevelASBuildInfo *buildInfo, const RenderTopLevelASInstance *instances, uint32_t instanceCount, bool preferFastBuild, bool preferFastTrace) {
    device->setTopLevelASBuildInfo(*buildInfo, instances, instanceCount, preferFastBuild, preferFastTrace);
}

void plume_render_device_set_shader_binding_table_info(RenderDevice *device, RenderShaderBindingTableInfo *tableInfo, const RenderShaderBindingGroups *groups, const RenderPipeline *pipeline, RenderDescriptorSet **descriptorSets, uint32_t descriptorSetCount) {
    device->setShaderBindingTableInfo(*tableInfo, *groups, pipeline, descriptorSets, descriptorSetCount);
}

const RenderDeviceCapabilities *plume_render_device_get_capabilities(RenderDevice *device) {
    return &device->getCapabilities();
}

const RenderDeviceDescription *plume_render_device_get_description(RenderDevice *device) {
    return &device->getDescription();
}

RenderSampleCounts plume_render_device_get_sample_counts_supported(RenderDevice *device, RenderFormat format) {
    return device->getSampleCountsSupported(format);
}

bool plume_render_device_begin_capture(RenderDevice *device) {
    return device->beginCapture();
}

bool plume_render_device_end_capture(RenderDevice *device) {
    return device->endCapture();
}

// RenderInterface
extern std::unique_ptr<RenderInterface> CreateMetalInterface();
extern std::unique_ptr<RenderInterface> CreateD3D12Interface();
#if PLUME_SDL_VULKAN_ENABLED
extern std::unique_ptr<RenderInterface> CreateVulkanInterface(RenderWindow sdlWindow);
#else
extern std::unique_ptr<RenderInterface> CreateVulkanInterface();
#endif

RenderInterface *plume_create_d3d12_interface(void) {
    #if defined(_WIN64)
    return CreateD3D12Interface().release();
    #endif

    return nullptr;
}

#if PLUME_SDL_VULKAN_ENABLED
RenderInterface *plume_create_vulkan_interface(RenderWindow sdlWindow) {
    return CreateVulkanInterface(sdlWindow).release();
}
#else
RenderInterface *plume_create_vulkan_interface(void) {
    return CreateVulkanInterface().release();
}
#endif

RenderInterface *plume_create_metal_interface(void) {
    #if defined(__APPLE__)
    return CreateMetalInterface().release();
    #endif

    return nullptr;
}

void plume_destroy_render_interface(RenderInterface *renderInterface) {
    delete renderInterface;
}

RenderDevice *plume_render_interface_create_device(RenderInterface *renderInterface, const char *preferredDeviceName) {
    return renderInterface->createDevice(std::string(preferredDeviceName)).release();
}

const char **plume_render_interface_get_device_names(RenderInterface *renderInterface) {
    // TODO: Deal with this.
    // return renderInterface->getDeviceNames();
}

const RenderInterfaceCapabilities *plume_render_interface_get_capabilities(RenderInterface *renderInterface) {
    return &renderInterface->getCapabilities();
}
