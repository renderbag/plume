//
// plume
//
// Copyright (c) 2024 renderbag and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE file for details.
//
#pragma once
#include "plume_render_interface.h"
#include <map>
#include <mutex>
#include <unordered_map>
#include <d3d11.h>
#include <dxgi1_2.h>

namespace plume {

struct D3D11Buffer;
struct D3D11CommandQueue;
struct D3D11Device;
struct D3D11GraphicsPipeline;
struct D3D11Interface;
struct D3D11Pipeline;
struct D3D11Pool;
struct D3D11PipelineLayout;
struct D3D11Texture;
struct D3D11TextureView;

// Simplified descriptor handling for D3D11 (uses direct SRV/CBV/UAV)
struct D3D11DescriptorSet : RenderDescriptorSet {
    D3D11Device* device = nullptr;
    struct Binding {
        uint32_t descriptorIndex;
        enum Type { Buffer, Texture, Sampler, AccelerationStructure } type;
        union {
            struct { const RenderBuffer* buffer; uint64_t bufferSize; };
            struct { const RenderTexture* texture; RenderTextureLayout textureLayout; const RenderTextureView* textureView; };
            struct { const RenderSampler* sampler; };
        };
    };
    std::vector<Binding> bindings;
    
    D3D11DescriptorSet(D3D11Device* device, const RenderDescriptorSetDesc& desc);
    ~D3D11DescriptorSet() override;
    
    void setBuffer(uint32_t descriptorIndex, const RenderBuffer* buffer, uint64_t bufferSize,
        const RenderBufferStructuredView* bufferStructuredView, const RenderBufferFormattedView* bufferFormattedView) override;
    void setTexture(uint32_t descriptorIndex, const RenderTexture* texture, RenderTextureLayout textureLayout,
        const RenderTextureView* textureView) override;
    void setSampler(uint32_t descriptorIndex, const RenderSampler* sampler) override;
    void setAccelerationStructure(uint32_t descriptorIndex, const RenderAccelerationStructure* accelerationStructure) override;
    
    // D3D11 specific: apply to context
    void apply(ID3D11DeviceContext* context, uint32_t setIndex);
};

struct D3D11SwapChain : RenderSwapChain {
    RenderSwapChainDesc desc;
    IDXGISwapChain* d3d = nullptr;
    D3D11CommandQueue* commandQueue = nullptr;
    std::vector<std::unique_ptr<D3D11Texture>> textures;
    DXGI_FORMAT nativeFormat = DXGI_FORMAT_UNKNOWN;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t refreshRate = 0;
    bool vsyncEnabled = true;
    
    D3D11SwapChain(D3D11CommandQueue* commandQueue, const RenderSwapChainDesc& desc);
    ~D3D11SwapChain() override;
    
    bool present(uint32_t textureIndex, RenderCommandSemaphore** waitSemaphores, uint32_t waitSemaphoreCount) override;
    void wait() override;
    bool resize() override;
    bool needsResize() const override;
    void setVsyncEnabled(bool vsyncEnabled) override;
    bool isVsyncEnabled() const override;
    uint32_t getWidth() const override;
    uint32_t getHeight() const override;
    RenderTexture* getTexture(uint32_t textureIndex) override;
    uint32_t getTextureCount() const override;
    bool acquireTexture(RenderCommandSemaphore* signalSemaphore, uint32_t* textureIndex) override;
    RenderWindow getWindow() const override;
    bool isEmpty() const override;
    uint32_t getRefreshRate() const override;
};

struct D3D11Framebuffer : RenderFramebuffer {
    D3D11Device* device = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<const D3D11Texture*> colorTargets;
    const D3D11Texture* depthTarget = nullptr;
    std::vector<ID3D11RenderTargetView*> colorRTVs;
    ID3D11DepthStencilView* depthDSV = nullptr;
    
    D3D11Framebuffer(D3D11Device* device, const RenderFramebufferDesc& desc);
    ~D3D11Framebuffer() override;
    
    uint32_t getWidth() const override;
    uint32_t getHeight() const override;
    
    void apply(ID3D11DeviceContext* context);
};

struct D3D11CommandList : RenderCommandList {
    ID3D11DeviceContext* deferredContext = nullptr; // For recording
    D3D11CommandQueue* queue = nullptr;
    const D3D11Framebuffer* targetFramebuffer = nullptr;
    bool open = false;
    
    // State tracking
    const D3D11PipelineLayout* activePipelineLayout = nullptr;
    const D3D11GraphicsPipeline* activeGraphicsPipeline = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY activeTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    
    D3D11CommandList(D3D11CommandQueue* queue);
    ~D3D11CommandList() override;
    
    void begin() override;
    void end() override;
    void barriers(RenderBarrierStages stages, const RenderBufferBarrier* bufferBarriers, uint32_t bufferBarriersCount,
        const RenderTextureBarrier* textureBarriers, uint32_t textureBarriersCount) override;
    void dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ) override;
    void traceRays(uint32_t width, uint32_t height, uint32_t depth, RenderBufferReference shaderBindingTable,
        const RenderShaderBindingGroupsInfo& shaderBindingGroupsInfo) override; // Not supported in D3D11
    void drawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount,
        uint32_t startVertexLocation, uint32_t startInstanceLocation) override;
    void drawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount,
        uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation) override;
    void setPipeline(const RenderPipeline* pipeline) override;
    void setComputePipelineLayout(const RenderPipelineLayout* pipelineLayout) override;
    void setComputePushConstants(uint32_t rangeIndex, const void* data, uint32_t offset, uint32_t size) override;
    void setComputeDescriptorSet(RenderDescriptorSet* descriptorSet, uint32_t setIndex) override;
    void setGraphicsPipelineLayout(const RenderPipelineLayout* pipelineLayout) override;
    void setGraphicsPushConstants(uint32_t rangeIndex, const void* data, uint32_t offset, uint32_t size) override;
    void setGraphicsDescriptorSet(RenderDescriptorSet* descriptorSet, uint32_t setIndex) override;
    void setGraphicsRootDescriptor(RenderBufferReference bufferReference, uint32_t rootDescriptorIndex) override;
    void setRaytracingPipelineLayout(const RenderPipelineLayout* pipelineLayout) override;
    void setRaytracingPushConstants(uint32_t rangeIndex, const void* data, uint32_t offset, uint32_t size) override;
    void setRaytracingDescriptorSet(RenderDescriptorSet* descriptorSet, uint32_t setIndex) override;
    void setIndexBuffer(const RenderIndexBufferView* view) override;
    void setVertexBuffers(uint32_t startSlot, const RenderVertexBufferView* views, uint32_t viewCount, const RenderInputSlot* inputSlots) override;
    void setViewports(const RenderViewport* viewports, uint32_t count) override;
    void setScissors(const RenderRect* scissorRects, uint32_t count) override;
    void setFramebuffer(const RenderFramebuffer* framebuffer) override;
    void setDepthBias(float depthBias, float depthBiasClamp, float slopeScaledDepthBias) override;
    void clearColor(uint32_t attachmentIndex, RenderColor colorValue, const RenderRect* clearRects, uint32_t clearRectsCount) override;
    void clearDepthStencil(bool clearDepth, bool clearStencil, float depthValue, uint32_t stencilValue, const RenderRect* clearRects, uint32_t clearRectsCount) override;
    void copyBufferRegion(RenderBufferReference dstBuffer, RenderBufferReference srcBuffer, uint64_t size) override;
    void copyTextureRegion(const RenderTextureCopyLocation& dstLocation, const RenderTextureCopyLocation& srcLocation,
        uint32_t dstX, uint32_t dstY, uint32_t dstZ, const RenderBox* srcBox) override;
    void copyBuffer(const RenderBuffer* dstBuffer, const RenderBuffer* srcBuffer) override;
    void copyTexture(const RenderTexture* dstTexture, const RenderTexture* srcTexture) override;
    void resolveTexture(const RenderTexture* dstTexture, const RenderTexture* srcTexture) override;
    void resolveTextureRegion(const RenderTexture* dstTexture, uint32_t dstX, uint32_t dstY,
        const RenderTexture* srcTexture, const RenderRect* srcRect, RenderResolveMode resolveMode) override;
    void buildBottomLevelAS(const RenderAccelerationStructure* dstAccelerationStructure, RenderBufferReference scratchBuffer,
        const RenderBottomLevelASBuildInfo& buildInfo) override; // Not supported
    void buildTopLevelAS(const RenderAccelerationStructure* dstAccelerationStructure, RenderBufferReference scratchBuffer,
        RenderBufferReference instancesBuffer, const RenderTopLevelASBuildInfo& buildInfo) override; // Not supported
    void discardTexture(const RenderTexture* texture) override;
    void resetQueryPool(const RenderQueryPool* queryPool, uint32_t queryFirstIndex, uint32_t queryCount) override;
    void writeTimestamp(const RenderQueryPool* queryPool, uint32_t queryIndex) override;
    
    // Execute recorded commands
    void execute(ID3D11DeviceContext* immediateContext);
};

struct D3D11CommandQueue : RenderCommandQueue {
    ID3D11DeviceContext* immediateContext = nullptr;
    D3D11Device* device = nullptr;
    RenderCommandListType type = RenderCommandListType::UNKNOWN;
    
    D3D11CommandQueue(D3D11Device* device, RenderCommandListType type);
    ~D3D11CommandQueue() override;
    
    std::unique_ptr<RenderCommandList> createCommandList() override;
    std::unique_ptr<RenderSwapChain> createSwapChain(const RenderSwapChainDesc& desc) override;
    void executeCommandLists(const RenderCommandList** commandLists, uint32_t commandListCount,
        RenderCommandSemaphore** waitSemaphores, uint32_t waitSemaphoreCount,
        RenderCommandSemaphore** signalSemaphores, uint32_t signalSemaphoreCount,
        RenderCommandFence* signalFence) override;
    void waitForCommandFence(RenderCommandFence* fence) override;
};

struct D3D11Buffer : RenderBuffer {
    ID3D11Buffer* d3d = nullptr;
    D3D11Device* device = nullptr;
    RenderBufferDesc desc;
    void* mappedMemory = nullptr;
    bool isMapped = false;
    
    D3D11Buffer(D3D11Device* device, D3D11Pool* pool, const RenderBufferDesc& desc);
    ~D3D11Buffer() override;
    
    void* map(uint32_t subresource, const RenderRange* readRange) override;
    void unmap(uint32_t subresource, const RenderRange* writtenRange) override;
    std::unique_ptr<RenderBufferFormattedView> createBufferFormattedView(RenderFormat format) override;
    void setName(const std::string& name) override;
    uint64_t getDeviceAddress() const override;
};

struct D3D11BufferFormattedView : RenderBufferFormattedView {
    RenderFormat format = RenderFormat::UNKNOWN;
    D3D11Buffer* buffer = nullptr;
    
    D3D11BufferFormattedView(D3D11Buffer* buffer, RenderFormat format);
    ~D3D11BufferFormattedView() override;
};

struct D3D11Texture : RenderTexture {
    ID3D11Resource* d3d = nullptr;
    ID3D11Texture2D* texture2D = nullptr;
    ID3D11Texture3D* texture3D = nullptr;
    D3D11Device* device = nullptr;
    RenderTextureDesc desc;
    RenderTextureLayout layout = RenderTextureLayout::UNKNOWN;
    
    mutable ID3D11ShaderResourceView* srv = nullptr;
    mutable ID3D11RenderTargetView* rtv = nullptr;
    mutable ID3D11DepthStencilView* dsv = nullptr;

    D3D11Texture(D3D11Device* device, D3D11Pool* pool, const RenderTextureDesc& desc);
    ~D3D11Texture() override;
    
    std::unique_ptr<RenderTextureView> createTextureView(const RenderTextureViewDesc& desc) const override;
    void setName(const std::string& name) override;
    
    ID3D11ShaderResourceView* getSRV() const;
    ID3D11RenderTargetView* getRTV() const;
    ID3D11DepthStencilView* getDSV() const;
};

struct D3D11TextureView : RenderTextureView {
    const D3D11Texture* texture = nullptr;
    RenderTextureViewDesc desc;
    ID3D11ShaderResourceView* srv = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    ID3D11DepthStencilView* dsv = nullptr;
    
    D3D11TextureView(const D3D11Texture* texture, const RenderTextureViewDesc& desc);
    ~D3D11TextureView() override;
};

struct D3D11Pool : RenderPool {
    D3D11Device* device = nullptr;
    RenderPoolDesc desc;
    
    D3D11Pool(D3D11Device* device, const RenderPoolDesc& desc);
    ~D3D11Pool() override;
    
    std::unique_ptr<RenderBuffer> createBuffer(const RenderBufferDesc& desc) override;
    std::unique_ptr<RenderTexture> createTexture(const RenderTextureDesc& desc) override;
};

struct D3D11Shader : RenderShader {
    union {
        ID3D11VertexShader* vs = nullptr;
        ID3D11PixelShader* ps;
        ID3D11GeometryShader* gs;
        ID3D11ComputeShader* cs;
    };
    D3D11Device* device = nullptr;
    RenderShaderFormat format = RenderShaderFormat::UNKNOWN;
    std::string entryPointName;
    
    D3D11Shader(D3D11Device* device, const void* data, uint64_t size, const char* entryPointName, RenderShaderFormat format);
    ~D3D11Shader() override;
    
    void setName(const std::string& name) override;
};

struct D3D11Sampler : RenderSampler {
    ID3D11SamplerState* d3d = nullptr;
    D3D11Device* device = nullptr;
    RenderSamplerDesc desc;
    
    D3D11Sampler(D3D11Device* device, const RenderSamplerDesc& desc);
    ~D3D11Sampler() override;
};

struct D3D11Pipeline : RenderPipeline {
    enum class Type { Unknown, Compute, Graphics };
    D3D11Device* device = nullptr;
    Type type = Type::Unknown;
    
    D3D11Pipeline(D3D11Device* device, Type type);
    ~D3D11Pipeline() override;
};

struct D3D11ComputePipeline : D3D11Pipeline {
    D3D11Shader* computeShader = nullptr;
    
    D3D11ComputePipeline(D3D11Device* device, const RenderComputePipelineDesc& desc);
    ~D3D11ComputePipeline() override;
    
    void setName(const std::string& name) override;
    RenderPipelineProgram getProgram(const std::string& name) const override;
    
    void apply(ID3D11DeviceContext* context);
};

struct D3D11GraphicsPipeline : D3D11Pipeline {
    D3D11Shader* vertexShader = nullptr;
    D3D11Shader* pixelShader = nullptr;
    D3D11Shader* geometryShader = nullptr;
    std::vector<RenderInputSlot> inputSlots;
    D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    ID3D11RasterizerState* rasterizerState = nullptr;
    ID3D11BlendState* blendState = nullptr;
    ID3D11DepthStencilState* depthStencilState = nullptr;
    uint32_t stencilRef = 0;
    D3D11_FILL_MODE fillMode = D3D11_FILL_SOLID;
    D3D11_CULL_MODE cullMode = D3D11_CULL_BACK;
    
    D3D11GraphicsPipeline(D3D11Device* device, const RenderGraphicsPipelineDesc& desc);
    ~D3D11GraphicsPipeline() override;
    
    void setName(const std::string& name) override;
    RenderPipelineProgram getProgram(const std::string& name) const override;
    
    void apply(ID3D11DeviceContext* context);
};

struct D3D11PipelineLayout : RenderPipelineLayout {
    D3D11Device* device = nullptr;
    std::vector<RenderPushConstantRange> pushConstantRanges;
    uint32_t setCount = 0;
    
    D3D11PipelineLayout(D3D11Device* device, const RenderPipelineLayoutDesc& desc);
    ~D3D11PipelineLayout() override;
};

struct D3D11Device : RenderDevice {
    ID3D11Device* d3d = nullptr;
    ID3D11DeviceContext* immediateContext = nullptr;
    D3D11Interface* renderInterface = nullptr;
    IDXGIAdapter1* adapter = nullptr;
    RenderDeviceCapabilities capabilities;
    RenderDeviceDescription description;
    
    D3D11Device(D3D11Interface* renderInterface, const std::string& preferredDeviceName);
    ~D3D11Device() override;
    
    std::unique_ptr<RenderDescriptorSet> createDescriptorSet(const RenderDescriptorSetDesc& desc) override;
    std::unique_ptr<RenderShader> createShader(const void* data, uint64_t size, const char* entryPointName, RenderShaderFormat format) override;
    std::unique_ptr<RenderSampler> createSampler(const RenderSamplerDesc& desc) override;
    std::unique_ptr<RenderPipeline> createComputePipeline(const RenderComputePipelineDesc& desc) override;
    std::unique_ptr<RenderPipeline> createGraphicsPipeline(const RenderGraphicsPipelineDesc& desc) override;
    std::unique_ptr<RenderPipeline> createRaytracingPipeline(const RenderRaytracingPipelineDesc& desc, const RenderPipeline* previousPipeline) override;
    std::unique_ptr<RenderCommandQueue> createCommandQueue(RenderCommandListType type) override;
    std::unique_ptr<RenderBuffer> createBuffer(const RenderBufferDesc& desc) override;
    std::unique_ptr<RenderTexture> createTexture(const RenderTextureDesc& desc) override;
    std::unique_ptr<RenderAccelerationStructure> createAccelerationStructure(const RenderAccelerationStructureDesc& desc) override;
    std::unique_ptr<RenderPool> createPool(const RenderPoolDesc& desc) override;
    std::unique_ptr<RenderPipelineLayout> createPipelineLayout(const RenderPipelineLayoutDesc& desc) override;
    std::unique_ptr<RenderCommandFence> createCommandFence() override;
    std::unique_ptr<RenderCommandSemaphore> createCommandSemaphore() override;
    std::unique_ptr<RenderFramebuffer> createFramebuffer(const RenderFramebufferDesc& desc) override;
    std::unique_ptr<RenderQueryPool> createQueryPool(uint32_t queryCount) override;
    
    void setBottomLevelASBuildInfo(RenderBottomLevelASBuildInfo& buildInfo, const RenderBottomLevelASMesh* meshes,
        uint32_t meshCount, bool preferFastBuild, bool preferFastTrace) override;
    void setTopLevelASBuildInfo(RenderTopLevelASBuildInfo& buildInfo, const RenderTopLevelASInstance* instances,
        uint32_t instanceCount, bool preferFastBuild, bool preferFastTrace) override;
    void setShaderBindingTableInfo(RenderShaderBindingTableInfo& tableInfo, const RenderShaderBindingGroups& groups,
        const RenderPipeline* pipeline, RenderDescriptorSet** descriptorSets, uint32_t descriptorSetCount) override;
    
    const RenderDeviceCapabilities& getCapabilities() const override;
    const RenderDeviceDescription& getDescription() const override;
    RenderSampleCounts getSampleCountsSupported(RenderFormat format) const override;
    
    bool beginCapture() override;
    bool endCapture() override;
    
    ID3D11Device* getDevice() const { return d3d; }
    ID3D11DeviceContext* getImmediateContext() const { return immediateContext; }
};

struct D3D11Interface : RenderInterface {
    IDXGIFactory1* dxgiFactory = nullptr;
    RenderInterfaceCapabilities capabilities;
    std::vector<std::string> deviceNames;
    bool allowTearing = false;
    
    D3D11Interface();
    ~D3D11Interface() override;
    
    std::unique_ptr<RenderDevice> createDevice(const std::string& preferredDeviceName) override;
    const RenderInterfaceCapabilities& getCapabilities() const override;
    const std::vector<std::string>& getDeviceNames() const override;
};
extern std::unique_ptr<RenderInterface> CreateD3D11Interface();
} // namespace plume