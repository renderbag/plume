//
// plume
//
// Copyright (c) 2024 renderbag and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE file for details.
//
#include "plume_d3d11.h"
#include <unordered_set>
#include <dxgi1_2.h>
#include <cstring>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-undefined-compare"
#pragma clang diagnostic ignored "-Wswitch"
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#ifndef NDEBUG
# define D3D11_DEBUG_LAYER_ENABLED
#endif

namespace plume {

// Helper functions
static std::wstring Utf8ToUtf16(const std::string_view& value) {
    std::wstring wideStr;
    wideStr.resize(MultiByteToWideChar(CP_UTF8, 0, value.data(), int(value.size()), nullptr, 0));
    MultiByteToWideChar(CP_UTF8, 0, value.data(), int(value.size()), wideStr.data(), int(wideStr.size()));
    return wideStr;
}

static std::string Utf16ToUtf8(const std::wstring_view& value) {
    std::string multiByteStr;
    multiByteStr.resize(WideCharToMultiByte(CP_UTF8, 0, value.data(), int(value.size()), nullptr, 0, nullptr, FALSE));
    WideCharToMultiByte(CP_UTF8, 0, value.data(), int(value.size()), multiByteStr.data(), int(multiByteStr.size()), nullptr, FALSE);
    return multiByteStr;
}

static uint32_t roundUp(uint32_t value, uint32_t powerOf2Alignment) {
    return (value + powerOf2Alignment - 1) & ~(powerOf2Alignment - 1);
}

static uint64_t roundUp(uint64_t value, uint64_t powerOf2Alignment) {
    return (value + powerOf2Alignment - 1) & ~(powerOf2Alignment - 1);
}

// Format conversion
static DXGI_FORMAT toDXGI(RenderFormat format) {
    switch (format) {
        case RenderFormat::UNKNOWN: return DXGI_FORMAT_UNKNOWN;
        case RenderFormat::R32G32B32A32_FLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case RenderFormat::R32G32B32A32_UINT: return DXGI_FORMAT_R32G32B32A32_UINT;
        case RenderFormat::R32G32B32A32_SINT: return DXGI_FORMAT_R32G32B32A32_SINT;
        case RenderFormat::R32G32B32_FLOAT: return DXGI_FORMAT_R32G32B32_FLOAT;
        case RenderFormat::R16G16B16A16_FLOAT: return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case RenderFormat::R16G16B16A16_UNORM: return DXGI_FORMAT_R16G16B16A16_UNORM;
        case RenderFormat::R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case RenderFormat::B8G8R8A8_UNORM: return DXGI_FORMAT_B8G8R8A8_UNORM;
        case RenderFormat::R32_FLOAT: return DXGI_FORMAT_R32_FLOAT;
        case RenderFormat::R32_UINT: return DXGI_FORMAT_R32_UINT;
        case RenderFormat::R16_UNORM: return DXGI_FORMAT_R16_UNORM;
        case RenderFormat::R8_UNORM: return DXGI_FORMAT_R8_UNORM;
        case RenderFormat::BC1_UNORM: return DXGI_FORMAT_BC1_UNORM;
        case RenderFormat::BC2_UNORM: return DXGI_FORMAT_BC2_UNORM;
        case RenderFormat::BC3_UNORM: return DXGI_FORMAT_BC3_UNORM;
        default:
            assert(false && "Unknown format.");
            return DXGI_FORMAT_UNKNOWN;
    }
}

// D3D11 specific conversions
static D3D11_FILTER toFilter(RenderFilter minFilter, RenderFilter magFilter, RenderMipmapMode mipmapMode, bool anisotropyEnabled, bool comparisonEnabled) {
    if (anisotropyEnabled) {
        return comparisonEnabled ? D3D11_FILTER_COMPARISON_ANISOTROPIC : D3D11_FILTER_ANISOTROPIC;
    }
    uint32_t filter = 0;
    if (minFilter == RenderFilter::LINEAR) filter |= 0x1;
    if (magFilter == RenderFilter::LINEAR) filter |= 0x2;
    if (mipmapMode == RenderMipmapMode::LINEAR) filter |= 0x4;
    if (comparisonEnabled) filter |= 0x80;
    return (D3D11_FILTER)filter;
}

static D3D11_TEXTURE_ADDRESS_MODE toD3D11(RenderTextureAddressMode mode) {
    switch (mode) {
        case RenderTextureAddressMode::WRAP: return D3D11_TEXTURE_ADDRESS_WRAP;
        case RenderTextureAddressMode::MIRROR: return D3D11_TEXTURE_ADDRESS_MIRROR;
        case RenderTextureAddressMode::CLAMP: return D3D11_TEXTURE_ADDRESS_CLAMP;
        case RenderTextureAddressMode::BORDER: return D3D11_TEXTURE_ADDRESS_BORDER;
        case RenderTextureAddressMode::MIRROR_ONCE: return D3D11_TEXTURE_ADDRESS_MIRROR_ONCE;
        default: return D3D11_TEXTURE_ADDRESS_WRAP;
    }
}

static D3D11_CULL_MODE toD3D11CullMode(RenderCullMode mode) {
    switch (mode) {
        case RenderCullMode::NONE: return D3D11_CULL_NONE;
        case RenderCullMode::FRONT: return D3D11_CULL_FRONT;
        case RenderCullMode::BACK: return D3D11_CULL_BACK;
        default: return D3D11_CULL_BACK;
    }
}

static D3D11_BLEND toD3D11Blend(RenderBlend blend) {
    switch (blend) {
        case RenderBlend::ZERO: return D3D11_BLEND_ZERO;
        case RenderBlend::ONE: return D3D11_BLEND_ONE;
        case RenderBlend::SRC_COLOR: return D3D11_BLEND_SRC_COLOR;
        case RenderBlend::INV_SRC_COLOR: return D3D11_BLEND_INV_SRC_COLOR;
        case RenderBlend::SRC_ALPHA: return D3D11_BLEND_SRC_ALPHA;
        case RenderBlend::INV_SRC_ALPHA: return D3D11_BLEND_INV_SRC_ALPHA;
        case RenderBlend::DEST_ALPHA: return D3D11_BLEND_DEST_ALPHA;
        case RenderBlend::INV_DEST_ALPHA: return D3D11_BLEND_INV_DEST_ALPHA;
        case RenderBlend::DEST_COLOR: return D3D11_BLEND_DEST_COLOR;
        case RenderBlend::INV_DEST_COLOR: return D3D11_BLEND_INV_DEST_COLOR;
        default: return D3D11_BLEND_ONE;
    }
}

static D3D11_BLEND_OP toD3D11BlendOp(RenderBlendOperation op) {
    switch (op) {
        case RenderBlendOperation::ADD: return D3D11_BLEND_OP_ADD;
        case RenderBlendOperation::SUBTRACT: return D3D11_BLEND_OP_SUBTRACT;
        case RenderBlendOperation::REV_SUBTRACT: return D3D11_BLEND_OP_REV_SUBTRACT;
        case RenderBlendOperation::MIN: return D3D11_BLEND_OP_MIN;
        case RenderBlendOperation::MAX: return D3D11_BLEND_OP_MAX;
        default: return D3D11_BLEND_OP_ADD;
    }
}

static D3D11_COMPARISON_FUNC toD3D11Comparison(RenderComparisonFunction func) {
    switch (func) {
        case RenderComparisonFunction::NEVER: return D3D11_COMPARISON_NEVER;
        case RenderComparisonFunction::LESS: return D3D11_COMPARISON_LESS;
        case RenderComparisonFunction::EQUAL: return D3D11_COMPARISON_EQUAL;
        case RenderComparisonFunction::LESS_EQUAL: return D3D11_COMPARISON_LESS_EQUAL;
        case RenderComparisonFunction::GREATER: return D3D11_COMPARISON_GREATER;
        case RenderComparisonFunction::NOT_EQUAL: return D3D11_COMPARISON_NOT_EQUAL;
        case RenderComparisonFunction::GREATER_EQUAL: return D3D11_COMPARISON_GREATER_EQUAL;
        case RenderComparisonFunction::ALWAYS: return D3D11_COMPARISON_ALWAYS;
        default: return D3D11_COMPARISON_ALWAYS;
    }
}

static D3D11_STENCIL_OP toD3D11StencilOp(RenderStencilOp op) {
    switch (op) {
        case RenderStencilOp::KEEP: return D3D11_STENCIL_OP_KEEP;
        case RenderStencilOp::ZERO: return D3D11_STENCIL_OP_ZERO;
        case RenderStencilOp::REPLACE: return D3D11_STENCIL_OP_REPLACE;
        case RenderStencilOp::INCREMENT_AND_CLAMP: return D3D11_STENCIL_OP_INCR_SAT;
        case RenderStencilOp::DECREMENT_AND_CLAMP: return D3D11_STENCIL_OP_DECR_SAT;
        case RenderStencilOp::INVERT: return D3D11_STENCIL_OP_INVERT;
        case RenderStencilOp::INCREMENT_AND_WRAP: return D3D11_STENCIL_OP_INCR;
        case RenderStencilOp::DECREMENT_AND_WRAP: return D3D11_STENCIL_OP_DECR;
        default: return D3D11_STENCIL_OP_KEEP;
    }
}

static D3D11_PRIMITIVE_TOPOLOGY toD3D11(RenderPrimitiveTopology topology) {
    switch (topology) {
        case RenderPrimitiveTopology::POINT_LIST: return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
        case RenderPrimitiveTopology::LINE_LIST: return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        case RenderPrimitiveTopology::LINE_STRIP: return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case RenderPrimitiveTopology::TRIANGLE_LIST: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case RenderPrimitiveTopology::TRIANGLE_STRIP: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        default: return D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    }
}

// D3D11DescriptorSet implementation
D3D11DescriptorSet::D3D11DescriptorSet(D3D11Device* device, const RenderDescriptorSetDesc& desc) {
    this->device = device;
}

D3D11DescriptorSet::~D3D11DescriptorSet() {
}

void D3D11DescriptorSet::setBuffer(uint32_t descriptorIndex, const RenderBuffer* buffer, uint64_t bufferSize,
    const RenderBufferStructuredView* bufferStructuredView, const RenderBufferFormattedView* bufferFormattedView) {
    Binding binding;
    binding.descriptorIndex = descriptorIndex;
    binding.type = Binding::Type::Buffer;
    binding.buffer = buffer;
    binding.bufferSize = bufferSize;
    bindings.push_back(binding);
}

void D3D11DescriptorSet::setTexture(uint32_t descriptorIndex, const RenderTexture* texture, RenderTextureLayout textureLayout,
    const RenderTextureView* textureView) {
    Binding binding;
    binding.descriptorIndex = descriptorIndex;
    binding.type = Binding::Type::Texture;
    binding.texture = texture;
    binding.textureLayout = textureLayout;
    binding.textureView = textureView;
    bindings.push_back(binding);
}

void D3D11DescriptorSet::setSampler(uint32_t descriptorIndex, const RenderSampler* sampler) {
    Binding binding;
    binding.descriptorIndex = descriptorIndex;
    binding.type = Binding::Type::Sampler;
    binding.sampler = sampler;
    bindings.push_back(binding);
}

void D3D11DescriptorSet::setAccelerationStructure(uint32_t descriptorIndex, const RenderAccelerationStructure* accelerationStructure) {
    // Not supported in D3D11
}

void D3D11DescriptorSet::apply(ID3D11DeviceContext* context, uint32_t setIndex) {
    for (const auto& binding : bindings) {
        switch (binding.type) {
            case Binding::Type::Buffer: {
                const D3D11Buffer* d3dBuffer = static_cast<const D3D11Buffer*>(binding.buffer);
                if (d3dBuffer && d3dBuffer->d3d) {
                    context->VSSetConstantBuffers(setIndex, 1, &d3dBuffer->d3d);
                    context->PSSetConstantBuffers(setIndex, 1, &d3dBuffer->d3d);
                }
                break;
            }
            case Binding::Type::Texture: {
                const D3D11Texture* d3dTexture = static_cast<const D3D11Texture*>(binding.texture);
                if (d3dTexture && d3dTexture->getSRV()) {
                    context->VSSetShaderResources(setIndex, 1, &d3dTexture->getSRV());
                    context->PSSetShaderResources(setIndex, 1, &d3dTexture->getSRV());
                }
                break;
            }
            case Binding::Type::Sampler: {
                const D3D11Sampler* d3dSampler = static_cast<const D3D11Sampler*>(binding.sampler);
                if (d3dSampler && d3dSampler->d3d) {
                    context->VSSetSamplers(setIndex, 1, &d3dSampler->d3d);
                    context->PSSetSamplers(setIndex, 1, &d3dSampler->d3d);
                }
                break;
            }
        }
    }
}

// D3D11SwapChain implementation
D3D11SwapChain::D3D11SwapChain(D3D11CommandQueue* commandQueue, const RenderSwapChainDesc& desc) {
    assert(commandQueue != nullptr);
    assert(desc.renderWindow != 0);
    this->commandQueue = commandQueue;
    this->desc = desc;
    
    nativeFormat = toDXGI(desc.format);
    
    RECT rect;
    GetClientRect(desc.renderWindow, &rect);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;
    
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = desc.textureCount;
    swapChainDesc.BufferDesc.Width = width;
    swapChainDesc.BufferDesc.Height = height;
    swapChainDesc.BufferDesc.Format = nativeFormat;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = desc.renderWindow;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swapChainDesc.Flags = 0;
    
    IDXGIFactory1* dxgiFactory = commandQueue->device->renderInterface->dxgiFactory;
    IDXGISwapChain* swapChain;
    HRESULT res = dxgiFactory->CreateSwapChain(commandQueue->immediateContext, &swapChainDesc, &swapChain);
    if (FAILED(res)) {
        fprintf(stderr, "CreateSwapChain failed with error code 0x%lX.\n", res);
        return;
    }
    
    d3d = swapChain;
    textures.resize(desc.textureCount);
    
    for (uint32_t i = 0; i < desc.textureCount; i++) {
        textures[i] = std::make_unique<D3D11Texture>();
        textures[i]->device = commandQueue->device;
        textures[i]->desc.dimension = RenderTextureDimension::TEXTURE_2D;
        textures[i]->desc.format = desc.format;
        textures[i]->desc.width = width;
        textures[i]->desc.height = height;
        textures[i]->desc.depth = 1;
        textures[i]->desc.mipLevels = 1;
        textures[i]->desc.arraySize = 1;
        textures[i]->desc.flags = RenderTextureFlag::RENDER_TARGET;
        
        d3d->GetBuffer(i, IID_PPV_ARGS(&textures[i]->d3d));
        textures[i]->texture2D = static_cast<ID3D11Texture2D*>(textures[i]->d3d);
        
        ID3D11RenderTargetView* rtv;
        commandQueue->device->d3d->CreateRenderTargetView(textures[i]->d3d, nullptr, &rtv);
        textures[i]->rtv = rtv;
    }
}

D3D11SwapChain::~D3D11SwapChain() {
    for (auto& tex : textures) {
        if (tex->rtv) tex->rtv->Release();
        if (tex->d3d) tex->d3d->Release();
    }
    if (d3d) d3d->Release();
}

bool D3D11SwapChain::present(uint32_t textureIndex, RenderCommandSemaphore** waitSemaphores, uint32_t waitSemaphoreCount) {
    UINT syncInterval = vsyncEnabled ? 1 : 0;
    HRESULT res = d3d->Present(syncInterval, 0);
    return SUCCEEDED(res);
}

void D3D11SwapChain::wait() {
    // No waitable object in D3D11
}

bool D3D11SwapChain::resize() {
    RECT rect;
    GetClientRect(desc.renderWindow, &rect);
    uint32_t newWidth = rect.right - rect.left;
    uint32_t newHeight = rect.bottom - rect.top;
    
    if (newWidth == 0 || newHeight == 0) return false;
    
    for (auto& tex : textures) {
        if (tex->rtv) tex->rtv->Release();
        if (tex->d3d) tex->d3d->Release();
        tex->d3d = nullptr;
        tex->rtv = nullptr;
    }
    
    HRESULT res = d3d->ResizeBuffers(desc.textureCount, newWidth, newHeight, nativeFormat, 0);
    if (FAILED(res)) return false;
    
    width = newWidth;
    height = newHeight;
    
    for (uint32_t i = 0; i < desc.textureCount; i++) {
        d3d->GetBuffer(i, IID_PPV_ARGS(&textures[i]->d3d));
        textures[i]->texture2D = static_cast<ID3D11Texture2D*>(textures[i]->d3d);
        textures[i]->desc.width = width;
        textures[i]->desc.height = height;
        commandQueue->device->d3d->CreateRenderTargetView(textures[i]->d3d, nullptr, &textures[i]->rtv);
    }
    
    return true;
}

bool D3D11SwapChain::needsResize() const {
    RECT rect;
    GetClientRect(desc.renderWindow, &rect);
    uint32_t newWidth = rect.right - rect.left;
    uint32_t newHeight = rect.bottom - rect.top;
    return (newWidth != width) || (newHeight != height);
}

void D3D11SwapChain::setVsyncEnabled(bool vsyncEnabled) {
    this->vsyncEnabled = vsyncEnabled;
}

bool D3D11SwapChain::isVsyncEnabled() const {
    return vsyncEnabled;
}

uint32_t D3D11SwapChain::getWidth() const {
    return width;
}

uint32_t D3D11SwapChain::getHeight() const {
    return height;
}

RenderTexture* D3D11SwapChain::getTexture(uint32_t textureIndex) {
    return textures[textureIndex].get();
}

uint32_t D3D11SwapChain::getTextureCount() const {
    return desc.textureCount;
}

bool D3D11SwapChain::acquireTexture(RenderCommandSemaphore* signalSemaphore, uint32_t* textureIndex) {
    *textureIndex = 0;
    return true;
}

RenderWindow D3D11SwapChain::getWindow() const {
    return desc.renderWindow;
}

bool D3D11SwapChain::isEmpty() const {
    return (d3d == nullptr) || (width == 0) || (height == 0);
}

uint32_t D3D11SwapChain::getRefreshRate() const {
    return 0;
}

// D3D11Framebuffer implementation
D3D11Framebuffer::D3D11Framebuffer(D3D11Device* device, const RenderFramebufferDesc& desc) {
    assert(device != nullptr);
    this->device = device;
    
    if (desc.colorAttachmentsCount > 0) {
        for (uint32_t i = 0; i < desc.colorAttachmentsCount; i++) {
            const D3D11TextureView* interfaceTextureView = desc.colorAttachmentViews && desc.colorAttachmentViews[i]
                ? static_cast<const D3D11TextureView*>(desc.colorAttachmentViews[i]) : nullptr;
            const D3D11Texture* interfaceTexture = interfaceTextureView
                ? interfaceTextureView->texture
                : static_cast<const D3D11Texture*>(desc.colorAttachments[i]);
            
            assert((interfaceTexture->desc.flags & RenderTextureFlag::RENDER_TARGET) && "Color attachment must be a render target.");
            
            ID3D11RenderTargetView* rtv = interfaceTextureView ? interfaceTextureView->rtv : interfaceTexture->getRTV();
            assert(rtv && "Failed to get RTV for color attachment");
            
            colorTargets.emplace_back(interfaceTexture);
            colorRTVs.emplace_back(rtv);
            
            if (i == 0) {
                width = interfaceTexture->desc.width;
                height = interfaceTexture->desc.height;
            }
        }
    }
    
    if (desc.depthAttachment != nullptr || desc.depthAttachmentView != nullptr) {
        const D3D11TextureView* interfaceTextureView = static_cast<const D3D11TextureView*>(desc.depthAttachmentView);
        const D3D11Texture* interfaceTexture = interfaceTextureView
            ? interfaceTextureView->texture
            : static_cast<const D3D11Texture*>(desc.depthAttachment);
        
        assert((interfaceTexture->desc.flags & RenderTextureFlag::DEPTH_TARGET) && "Depth attachment must be a depth target.");
        
        ID3D11DepthStencilView* dsv = interfaceTextureView ? interfaceTextureView->dsv : interfaceTexture->getDSV();
        assert(dsv && "Failed to get DSV for depth attachment");
        
        depthTarget = interfaceTexture;
        depthDSV = dsv;
        
        if (desc.colorAttachmentsCount == 0) {
            width = interfaceTexture->desc.width;
            height = interfaceTexture->desc.height;
        }
    }
}

D3D11Framebuffer::~D3D11Framebuffer() {
}

uint32_t D3D11Framebuffer::getWidth() const {
    return width;
}

uint32_t D3D11Framebuffer::getHeight() const {
    return height;
}

void D3D11Framebuffer::apply(ID3D11DeviceContext* context) {
    ID3D11RenderTargetView* rtvArray[8] = {};
    for (size_t i = 0; i < colorRTVs.size(); i++) {
        rtvArray[i] = colorRTVs[i];
    }
    context->OMSetRenderTargets((UINT)colorRTVs.size(), rtvArray, depthDSV);
}

// D3D11CommandList implementation
D3D11CommandList::D3D11CommandList(D3D11CommandQueue* queue) {
    this->queue = queue;
    this->deferredContext = nullptr;
}

D3D11CommandList::~D3D11CommandList() {
    if (deferredContext) deferredContext->Release();
}

void D3D11CommandList::begin() {
    open = true;
}

void D3D11CommandList::end() {
    open = false;
}

void D3D11CommandList::barriers(RenderBarrierStages stages, const RenderBufferBarrier* bufferBarriers, uint32_t bufferBarriersCount,
    const RenderTextureBarrier* textureBarriers, uint32_t textureBarriersCount) {
    ID3D11DeviceContext* ctx = deferredContext ? deferredContext : queue->immediateContext;
    
    if (bufferBarriersCount > 0 || textureBarriersCount > 0) {
        bool hasUAVBarrier = false;
        for (uint32_t i = 0; i < bufferBarriersCount; i++) {
            if (bufferBarriers[i].accessBefore & RenderBufferAccess::WRITE ||
                bufferBarriers[i].accessAfter & RenderBufferAccess::WRITE) {
                hasUAVBarrier = true;
                break;
            }
        }
        for (uint32_t i = 0; i < textureBarriersCount; i++) {
            if (textureBarriers[i].layoutBefore == RenderTextureLayout::READ_WRITE ||
                textureBarriers[i].layoutAfter == RenderTextureLayout::READ_WRITE) {
                hasUAVBarrier = true;
                break;
            }
        }
        
        if (hasUAVBarrier) {
            ctx->CSSetUnorderedAccessViews(0, 0, nullptr, nullptr);
        }
    }
}

void D3D11CommandList::dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ) {
    ID3D11DeviceContext* ctx = deferredContext ? deferredContext : queue->immediateContext;
    ctx->Dispatch(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
}

void D3D11CommandList::traceRays(uint32_t width, uint32_t height, uint32_t depth, RenderBufferReference shaderBindingTable,
    const RenderShaderBindingGroupsInfo& shaderBindingGroupsInfo) {
    // Not supported in D3D11
}

void D3D11CommandList::drawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount,
    uint32_t startVertexLocation, uint32_t startInstanceLocation) {
    ID3D11DeviceContext* ctx = deferredContext ? deferredContext : queue->immediateContext;
    ctx->DrawInstanced(vertexCountPerInstance, instanceCount, startVertexLocation, startInstanceLocation);
}

void D3D11CommandList::drawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount,
    uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation) {
    ID3D11DeviceContext* ctx = deferredContext ? deferredContext : queue->immediateContext;
    ctx->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
}

void D3D11CommandList::setPipeline(const RenderPipeline* pipeline) {
    const D3D11Pipeline* d3dPipeline = static_cast<const D3D11Pipeline*>(pipeline);
    if (!d3dPipeline) return;
    
    ID3D11DeviceContext* ctx = deferredContext ? deferredContext : queue->immediateContext;
    
    if (d3dPipeline->type == D3D11Pipeline::Type::Compute) {
        const D3D11ComputePipeline* computePipeline = static_cast<const D3D11ComputePipeline*>(pipeline);
        computePipeline->apply(ctx);
    } else if (d3dPipeline->type == D3D11Pipeline::Type::Graphics) {
        const D3D11GraphicsPipeline* graphicsPipeline = static_cast<const D3D11GraphicsPipeline*>(pipeline);
        graphicsPipeline->apply(ctx);
        activeGraphicsPipeline = graphicsPipeline;
    }
}

void D3D11CommandList::setComputePipelineLayout(const RenderPipelineLayout* pipelineLayout) {
    activePipelineLayout = static_cast<const D3D11PipelineLayout*>(pipelineLayout);
}

void D3D11CommandList::setComputePushConstants(uint32_t rangeIndex, const void* data, uint32_t offset, uint32_t size) {
    if (!activePipelineLayout || rangeIndex >= activePipelineLayout->pushConstantRanges.size()) return;
    ID3D11DeviceContext* ctx = deferredContext ? deferredContext : queue->immediateContext;
    // TODO: Implement proper push constants using constant buffers
}

void D3D11CommandList::setComputeDescriptorSet(RenderDescriptorSet* descriptorSet, uint32_t setIndex) {
    D3D11DescriptorSet* d3dSet = static_cast<D3D11DescriptorSet*>(descriptorSet);
    if (d3dSet) {
        ID3D11DeviceContext* ctx = deferredContext ? deferredContext : queue->immediateContext;
        d3dSet->apply(ctx, setIndex);
    }
}

void D3D11CommandList::setGraphicsPipelineLayout(const RenderPipelineLayout* pipelineLayout) {
    activePipelineLayout = static_cast<const D3D11PipelineLayout*>(pipelineLayout);
}

void D3D11CommandList::setGraphicsPushConstants(uint32_t rangeIndex, const void* data, uint32_t offset, uint32_t size) {
    // TODO: Implement proper push constants
}

void D3D11CommandList::setGraphicsDescriptorSet(RenderDescriptorSet* descriptorSet, uint32_t setIndex) {
    D3D11DescriptorSet* d3dSet = static_cast<D3D11DescriptorSet*>(descriptorSet);
    if (d3dSet) {
        ID3D11DeviceContext* ctx = deferredContext ? deferredContext : queue->immediateContext;
        d3dSet->apply(ctx, setIndex);
    }
}

void D3D11CommandList::setGraphicsRootDescriptor(RenderBufferReference bufferReference, uint32_t rootDescriptorIndex) {
    // Not directly supported
}

void D3D11CommandList::setRaytracingPipelineLayout(const RenderPipelineLayout* pipelineLayout) {
    // Not supported
}

void D3D11CommandList::setRaytracingPushConstants(uint32_t rangeIndex, const void* data, uint32_t offset, uint32_t size) {
    // Not supported
}

void D3D11CommandList::setRaytracingDescriptorSet(RenderDescriptorSet* descriptorSet, uint32_t setIndex) {
    // Not supported
}

void D3D11CommandList::setIndexBuffer(const RenderIndexBufferView* view) {
    if (!view || !view->buffer) return;
    
    const D3D11Buffer* d3dBuffer = static_cast<const D3D11Buffer*>(view->buffer);
    ID3D11DeviceContext* ctx = deferredContext ? deferredContext : queue->immediateContext;
    DXGI_FORMAT format = (view->format == RenderFormat::R16_UINT) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    ctx->IASetIndexBuffer(d3dBuffer->d3d, format, view->offset);
}

void D3D11CommandList::setVertexBuffers(uint32_t startSlot, const RenderVertexBufferView* views, uint32_t viewCount, const RenderInputSlot* inputSlots) {
    ID3D11DeviceContext* ctx = deferredContext ? deferredContext : queue->immediateContext;
    
    std::vector<ID3D11Buffer*> buffers(viewCount);
    std::vector<UINT> strides(viewCount);
    std::vector<UINT> offsets(viewCount);
    
    for (uint32_t i = 0; i < viewCount; i++) {
        const D3D11Buffer* d3dBuffer = static_cast<const D3D11Buffer*>(views[i].buffer);
        buffers[i] = d3dBuffer ? d3dBuffer->d3d : nullptr;
        strides[i] = views[i].stride;
        offsets[i] = views[i].offset;
    }
    
    ctx->IASetVertexBuffers(startSlot, viewCount, buffers.data(), strides.data(), offsets.data());
}

void D3D11CommandList::setViewports(const RenderViewport* viewports, uint32_t count) {
    ID3D11DeviceContext* ctx = deferredContext ? deferredContext : queue->immediateContext;
    D3D11_VIEWPORT d3dViewports[16];
    
    for (uint32_t i = 0; i < count; i++) {
        d3dViewports[i].TopLeftX = viewports[i].x;
        d3dViewports[i].TopLeftY = viewports[i].y;
        d3dViewports[i].Width = viewports[i].width;
        d3dViewports[i].Height = viewports[i].height;
        d3dViewports[i].MinDepth = viewports[i].minDepth;
        d3dViewports[i].MaxDepth = viewports[i].maxDepth;
    }
    
    ctx->RSSetViewports(count, d3dViewports);
}

void D3D11CommandList::setScissors(const RenderRect* scissorRects, uint32_t count) {
    ID3D11DeviceContext* ctx = deferredContext ? deferredContext : queue->immediateContext;
    D3D11_RECT d3dRects[16];
    
    for (uint32_t i = 0; i < count; i++) {
        d3dRects[i].left = scissorRects[i].left;
        d3dRects[i].top = scissorRects[i].top;
        d3dRects[i].right = scissorRects[i].right;
        d3dRects[i].bottom = scissorRects[i].bottom;
    }
    
    ctx->RSSetScissorRects(count, d3dRects);
}

void D3D11CommandList::setFramebuffer(const RenderFramebuffer* framebuffer) {
    const D3D11Framebuffer* d3dFramebuffer = static_cast<const D3D11Framebuffer*>(framebuffer);
    targetFramebuffer = d3dFramebuffer;
    ID3D11DeviceContext* ctx = deferredContext ? deferredContext : queue->immediateContext;
    
    if (d3dFramebuffer) {
        d3dFramebuffer->apply(ctx);
    } else {
        ID3D11RenderTargetView* nullRTV[8] = {};
        ctx->OMSetRenderTargets(8, nullRTV, nullptr);
    }
}

void D3D11CommandList::setDepthBias(float depthBias, float depthBiasClamp, float slopeScaledDepthBias) {
    // Handled by rasterizer state
}

void D3D11CommandList::clearColor(uint32_t attachmentIndex, RenderColor colorValue, const RenderRect* clearRects, uint32_t clearRectsCount) {
    ID3D11DeviceContext* ctx = deferredContext ? deferredContext : queue->immediateContext;
    
    if (targetFramebuffer && attachmentIndex < targetFramebuffer->colorRTVs.size()) {
        float color[4] = { colorValue.r, colorValue.g, colorValue.b, colorValue.a };
        if (clearRectsCount > 0) {
            D3D11_RECT oldScissor;
            UINT numScissors = 1;
            ctx->RSGetScissorRects(&numScissors, &oldScissor);
            
            for (uint32_t i = 0; i < clearRectsCount; i++) {
                D3D11_RECT rect = { clearRects[i].left, clearRects[i].top, clearRects[i].right, clearRects[i].bottom };
                ctx->RSSetScissorRects(1, &rect);
                ctx->ClearRenderTargetView(targetFramebuffer->colorRTVs[attachmentIndex], color);
            }
            
            ctx->RSSetScissorRects(numScissors, &oldScissor);
        } else {
            ctx->ClearRenderTargetView(targetFramebuffer->colorRTVs[attachmentIndex], color);
        }
    }
}

void D3D11CommandList::clearDepthStencil(bool clearDepth, bool clearStencil, float depthValue, uint32_t stencilValue,
    const RenderRect* clearRects, uint32_t clearRectsCount) {
    ID3D11DeviceContext* ctx = deferredContext ? deferredContext : queue->immediateContext;
    
    if (targetFramebuffer && targetFramebuffer->depthDSV) {
        UINT flags = 0;
        if (clearDepth) flags |= D3D11_CLEAR_DEPTH;
        if (clearStencil) flags |= D3D11_CLEAR_STENCIL;
        ctx->ClearDepthStencilView(targetFramebuffer->depthDSV, flags, depthValue, stencilValue);
    }
}

void D3D11CommandList::copyBufferRegion(RenderBufferReference dstBuffer, RenderBufferReference srcBuffer, uint64_t size) {
    ID3D11DeviceContext* ctx = deferredContext ? deferredContext : queue->immediateContext;
    
    const D3D11Buffer* dst = static_cast<const D3D11Buffer*>(dstBuffer.buffer);
    const D3D11Buffer* src = static_cast<const D3D11Buffer*>(srcBuffer.buffer);
    
    if (dst && src) {
        ctx->CopySubresourceRegion(dst->d3d, 0, (UINT)dstBuffer.offset, 0, 0, src->d3d, 0, nullptr);
    }
}

void D3D11CommandList::copyTextureRegion(const RenderTextureCopyLocation& dstLocation, const RenderTextureCopyLocation& srcLocation,
    uint32_t dstX, uint32_t dstY, uint32_t dstZ, const RenderBox* srcBox) {
    ID3D11DeviceContext* ctx = deferredContext ? deferredContext : queue->immediateContext;
    
    const D3D11Texture* dstTex = static_cast<const D3D11Texture*>(dstLocation.texture);
    const D3D11Texture* srcTex = static_cast<const D3D11Texture*>(srcLocation.texture);
    
    if (dstTex && srcTex) {
        if (srcBox) {
            D3D11_BOX box = { srcBox->left, srcBox->top, srcBox->front, srcBox->right, srcBox->bottom, srcBox->back };
            ctx->CopySubresourceRegion(dstTex->d3d, 0, dstX, dstY, dstZ, srcTex->d3d, 0, &box);
        } else {
            ctx->CopySubresourceRegion(dstTex->d3d, 0, dstX, dstY, dstZ, srcTex->d3d, 0, nullptr);
        }
    }
}

void D3D11CommandList::copyBuffer(const RenderBuffer* dstBuffer, const RenderBuffer* srcBuffer) {
    ID3D11DeviceContext* ctx = deferredContext ? deferredContext : queue->immediateContext;
    
    const D3D11Buffer* dst = static_cast<const D3D11Buffer*>(dstBuffer);
    const D3D11Buffer* src = static_cast<const D3D11Buffer*>(srcBuffer);
    
    if (dst && src) {
        ctx->CopyResource(dst->d3d, src->d3d);
    }
}

void D3D11CommandList::copyTexture(const RenderTexture* dstTexture, const RenderTexture* srcTexture) {
    ID3D11DeviceContext* ctx = deferredContext ? deferredContext : queue->immediateContext;
    
    const D3D11Texture* dst = static_cast<const D3D11Texture*>(dstTexture);
    const D3D11Texture* src = static_cast<const D3D11Texture*>(srcTexture);
    
    if (dst && src) {
        ctx->CopyResource(dst->d3d, src->d3d);
    }
}

void D3D11CommandList::resolveTexture(const RenderTexture* dstTexture, const RenderTexture* srcTexture) {
    ID3D11DeviceContext* ctx = deferredContext ? deferredContext : queue->immediateContext;
    
    const D3D11Texture* dst = static_cast<const D3D11Texture*>(dstTexture);
    const D3D11Texture* src = static_cast<const D3D11Texture*>(srcTexture);
    
    if (dst && src) {
        ctx->ResolveSubresource(dst->d3d, 0, src->d3d, 0, toDXGI(dst->desc.format));
    }
}

void D3D11CommandList::resolveTextureRegion(const RenderTexture* dstTexture, uint32_t dstX, uint32_t dstY,
    const RenderTexture* srcTexture, const RenderRect* srcRect, RenderResolveMode resolveMode) {
    resolveTexture(dstTexture, srcTexture);
}

void D3D11CommandList::buildBottomLevelAS(const RenderAccelerationStructure* dstAccelerationStructure, RenderBufferReference scratchBuffer,
    const RenderBottomLevelASBuildInfo& buildInfo) {
    // Not supported
}

void D3D11CommandList::buildTopLevelAS(const RenderAccelerationStructure* dstAccelerationStructure, RenderBufferReference scratchBuffer,
    RenderBufferReference instancesBuffer, const RenderTopLevelASBuildInfo& buildInfo) {
    // Not supported
}

void D3D11CommandList::discardTexture(const RenderTexture* texture) {
    // Not needed in D3D11
}

void D3D11CommandList::resetQueryPool(const RenderQueryPool* queryPool, uint32_t queryFirstIndex, uint32_t queryCount) {
    // TODO: Implement if needed
}

void D3D11CommandList::writeTimestamp(const RenderQueryPool* queryPool, uint32_t queryIndex) {
    // TODO: Implement if needed
}

void D3D11CommandList::execute(ID3D11DeviceContext* immediateContext) {
    if (deferredContext) {
        ID3D11CommandList* executedList = nullptr;
        HRESULT res = deferredContext->FinishCommandList(FALSE, &executedList);
        if (SUCCEEDED(res) && executedList) {
            immediateContext->ExecuteCommandList(executedList, FALSE);
            executedList->Release();
        }
    }
}

// D3D11Buffer implementation
D3D11Buffer::D3D11Buffer(D3D11Device* device, D3D11Pool* pool, const RenderBufferDesc& desc) {
    this->device = device;
    this->desc = desc;
    
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.ByteWidth = (UINT)desc.size;
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = 0;
    bufferDesc.CPUAccessFlags = 0;
    
    if (desc.flags & RenderBufferFlag::VERTEX) {
        bufferDesc.BindFlags |= D3D11_BIND_VERTEX_BUFFER;
    }
    if (desc.flags & RenderBufferFlag::INDEX) {
        bufferDesc.BindFlags |= D3D11_BIND_INDEX_BUFFER;
    }
    if (desc.flags & RenderBufferFlag::CONSTANT) {
        bufferDesc.BindFlags |= D3D11_BIND_CONSTANT_BUFFER;
        bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        bufferDesc.ByteWidth = (UINT)roundUp(desc.size, 16);
    }
    if (desc.flags & RenderBufferFlag::SHADER_RESOURCE) {
        bufferDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }
    if (desc.flags & RenderBufferFlag::UNORDERED_ACCESS) {
        bufferDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    }
    if (desc.flags & RenderBufferFlag::COPY_SOURCE) {
        bufferDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }
    
    if (desc.heapType == RenderHeapType::UPLOAD || desc.heapType == RenderHeapType::GPU_UPLOAD) {
        bufferDesc.Usage = D3D11_USAGE_STAGING;
        bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        bufferDesc.BindFlags = 0;
    } else if (desc.heapType == RenderHeapType::READBACK) {
        bufferDesc.Usage = D3D11_USAGE_STAGING;
        bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        bufferDesc.BindFlags = 0;
    }
    
    HRESULT res = device->d3d->CreateBuffer(&bufferDesc, nullptr, &d3d);
    if (FAILED(res)) {
        fprintf(stderr, "CreateBuffer failed with error code 0x%lX.\n", res);
        d3d = nullptr;
    }
}

D3D11Buffer::~D3D11Buffer() {
    if (d3d) {
        d3d->Release();
        d3d = nullptr;
    }
}

void* D3D11Buffer::map(uint32_t subresource, const RenderRange* readRange) {
    if (!d3d) return nullptr;
    
    ID3D11DeviceContext* context = device->getImmediateContext();
    D3D11_MAP mapType = D3D11_MAP_WRITE_DISCARD;
    
    if (desc.heapType == RenderHeapType::READBACK) {
        mapType = D3D11_MAP_READ;
    } else if (desc.flags & RenderBufferFlag::CONSTANT) {
        mapType = D3D11_MAP_WRITE_DISCARD;
    }
    
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT res = context->Map(d3d, 0, mapType, 0, &mapped);
    if (FAILED(res)) {
        fprintf(stderr, "Map failed with error code 0x%lX.\n", res);
        return nullptr;
    }
    
    mappedMemory = mapped.pData;
    isMapped = true;
    return mappedMemory;
}

void D3D11Buffer::unmap(uint32_t subresource, const RenderRange* writtenRange) {
    if (!d3d || !isMapped) return;
    
    ID3D11DeviceContext* context = device->getImmediateContext();
    context->Unmap(d3d, 0);
    isMapped = false;
    mappedMemory = nullptr;
}

std::unique_ptr<RenderBufferFormattedView> D3D11Buffer::createBufferFormattedView(RenderFormat format) {
    return std::make_unique<D3D11BufferFormattedView>(this, format);
}

void D3D11Buffer::setName(const std::string& name) {
    if (d3d) {
        const std::wstring wideName = Utf8ToUtf16(name);
        d3d->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)wideName.size() * 2, wideName.c_str());
    }
}

uint64_t D3D11Buffer::getDeviceAddress() const {
    return 0;
}

// D3D11BufferFormattedView
D3D11BufferFormattedView::D3D11BufferFormattedView(D3D11Buffer* buffer, RenderFormat format) {
    this->buffer = buffer;
    this->format = format;
}

D3D11BufferFormattedView::~D3D11BufferFormattedView() {
}

// D3D11Texture implementation
D3D11Texture::D3D11Texture(D3D11Device* device, D3D11Pool* pool, const RenderTextureDesc& desc) {
    this->device = device;
    this->desc = desc;
    this->layout = RenderTextureLayout::UNKNOWN;
    this->srv = nullptr;
    this->rtv = nullptr;
    this->dsv = nullptr;
    
    DXGI_FORMAT dxgiFormat = toDXGI(desc.format);
    
    if (desc.dimension == RenderTextureDimension::TEXTURE_2D) {
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = desc.width;
        texDesc.Height = desc.height;
        texDesc.MipLevels = desc.mipLevels;
        texDesc.ArraySize = desc.arraySize;
        texDesc.Format = dxgiFormat;
        texDesc.SampleDesc.Count = (UINT)desc.multisampling.sampleCount;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = 0;
        texDesc.CPUAccessFlags = 0;
        
        if (desc.flags & RenderTextureFlag::SHADER_RESOURCE) {
            texDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
        }
        if (desc.flags & RenderTextureFlag::RENDER_TARGET) {
            texDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
        }
        if (desc.flags & RenderTextureFlag::DEPTH_TARGET) {
            texDesc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
        }
        if (desc.flags & RenderTextureFlag::UNORDERED_ACCESS) {
            texDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
        }
        
        HRESULT res = device->d3d->CreateTexture2D(&texDesc, nullptr, &texture2D);
        if (FAILED(res)) {
            fprintf(stderr, "CreateTexture2D failed with error code 0x%lX.\n", res);
            texture2D = nullptr;
        }
        d3d = texture2D;
    } else if (desc.dimension == RenderTextureDimension::TEXTURE_3D) {
        D3D11_TEXTURE3D_DESC texDesc = {};
        texDesc.Width = desc.width;
        texDesc.Height = desc.height;
        texDesc.Depth = desc.depth;
        texDesc.MipLevels = desc.mipLevels;
        texDesc.Format = dxgiFormat;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = 0;
        
        if (desc.flags & RenderTextureFlag::UNORDERED_ACCESS) {
            texDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
        }
        
        HRESULT res = device->d3d->CreateTexture3D(&texDesc, nullptr, &texture3D);
        if (FAILED(res)) {
            fprintf(stderr, "CreateTexture3D failed with error code 0x%lX.\n", res);
            texture3D = nullptr;
        }
        d3d = texture3D;
    }
}

D3D11Texture::~D3D11Texture() {
    if (srv) srv->Release();
    if (rtv) rtv->Release();
    if (dsv) dsv->Release();
    if (texture2D) texture2D->Release();
    if (texture3D) texture3D->Release();
}

std::unique_ptr<RenderTextureView> D3D11Texture::createTextureView(const RenderTextureViewDesc& viewDesc) const {
    return std::make_unique<D3D11TextureView>(this, viewDesc);
}

void D3D11Texture::setName(const std::string& name) {
    if (d3d) {
        const std::wstring wideName = Utf8ToUtf16(name);
        d3d->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)wideName.size() * 2, wideName.c_str());
    }
}

ID3D11ShaderResourceView* D3D11Texture::getSRV() const {
    if (!srv && (desc.flags & RenderTextureFlag::SHADER_RESOURCE)) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = toDXGI(desc.format);
        
        if (desc.dimension == RenderTextureDimension::TEXTURE_2D) {
            if (desc.arraySize > 1) {
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                srvDesc.Texture2DArray.MipLevels = desc.mipLevels;
                srvDesc.Texture2DArray.ArraySize = desc.arraySize;
            } else {
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MipLevels = desc.mipLevels;
            }
        } else if (desc.dimension == RenderTextureDimension::TEXTURE_3D) {
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
            srvDesc.Texture3D.MipLevels = desc.mipLevels;
        }
        
        device->d3d->CreateShaderResourceView(d3d, &srvDesc, &const_cast<D3D11Texture*>(this)->srv);
    }
    return srv;
}

ID3D11RenderTargetView* D3D11Texture::getRTV() const {
    if (!rtv && (desc.flags & RenderTextureFlag::RENDER_TARGET)) {
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = toDXGI(desc.format);
        
        if (desc.arraySize > 1) {
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
            rtvDesc.Texture2DArray.MipSlice = 0;
            rtvDesc.Texture2DArray.FirstArraySlice = 0;
            rtvDesc.Texture2DArray.ArraySize = desc.arraySize;
        } else {
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Texture2D.MipSlice = 0;
        }
        
        device->d3d->CreateRenderTargetView(d3d, &rtvDesc, &const_cast<D3D11Texture*>(this)->rtv);
    }
    return rtv;
}

ID3D11DepthStencilView* D3D11Texture::getDSV() const {
    if (!dsv && (desc.flags & RenderTextureFlag::DEPTH_TARGET)) {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = toDXGI(desc.format);
        
        if (desc.arraySize > 1) {
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
            dsvDesc.Texture2DArray.MipSlice = 0;
            dsvDesc.Texture2DArray.FirstArraySlice = 0;
            dsvDesc.Texture2DArray.ArraySize = desc.arraySize;
        } else {
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = 0;
        }
        
        device->d3d->CreateDepthStencilView(d3d, &dsvDesc, &const_cast<D3D11Texture*>(this)->dsv);
    }
    return dsv;
}

// D3D11TextureView
D3D11TextureView::D3D11TextureView(const D3D11Texture* texture, const RenderTextureViewDesc& viewDesc) {
    this->texture = texture;
    this->desc = viewDesc;
    this->srv = nullptr;
    this->rtv = nullptr;
    this->dsv = nullptr;
    
    ID3D11Device* device = texture->device->d3d;
    DXGI_FORMAT format = toDXGI(viewDesc.format);
    
    if (viewDesc.dimension == RenderTextureViewDimension::TEXTURE_2D) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = format;
        
        if (texture->desc.arraySize > 1) {
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            srvDesc.Texture2DArray.MostDetailedMip = viewDesc.mipSlice;
            srvDesc.Texture2DArray.MipLevels = viewDesc.mipLevels;
            srvDesc.Texture2DArray.FirstArraySlice = viewDesc.arrayIndex;
            srvDesc.Texture2DArray.ArraySize = viewDesc.arraySize;
        } else {
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = viewDesc.mipSlice;
            srvDesc.Texture2D.MipLevels = viewDesc.mipLevels;
        }
        
        device->CreateShaderResourceView(texture->d3d, &srvDesc, &srv);
    } else if (viewDesc.dimension == RenderTextureViewDimension::TEXTURE_3D) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
        srvDesc.Texture3D.MostDetailedMip = viewDesc.mipSlice;
        srvDesc.Texture3D.MipLevels = viewDesc.mipLevels;
        device->CreateShaderResourceView(texture->d3d, &srvDesc, &srv);
    }
    
    if (texture->desc.flags & RenderTextureFlag::RENDER_TARGET) {
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = format;
        
        if (texture->desc.arraySize > 1) {
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
            rtvDesc.Texture2DArray.MipSlice = viewDesc.mipSlice;
            rtvDesc.Texture2DArray.FirstArraySlice = viewDesc.arrayIndex;
            rtvDesc.Texture2DArray.ArraySize = viewDesc.arraySize;
        } else {
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Texture2D.MipSlice = viewDesc.mipSlice;
        }
        
        device->CreateRenderTargetView(texture->d3d, &rtvDesc, &rtv);
    }
    
    if (texture->desc.flags & RenderTextureFlag::DEPTH_TARGET) {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = format;
        
        if (texture->desc.arraySize > 1) {
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
            dsvDesc.Texture2DArray.MipSlice = viewDesc.mipSlice;
            dsvDesc.Texture2DArray.FirstArraySlice = viewDesc.arrayIndex;
            dsvDesc.Texture2DArray.ArraySize = viewDesc.arraySize;
        } else {
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = viewDesc.mipSlice;
        }
        
        device->CreateDepthStencilView(texture->d3d, &dsvDesc, &dsv);
    }
}

D3D11TextureView::~D3D11TextureView() {
    if (srv) srv->Release();
    if (rtv) rtv->Release();
    if (dsv) dsv->Release();
}

// D3D11Shader implementation
D3D11Shader::D3D11Shader(D3D11Device* device, const void* data, uint64_t size, const char* entryPointName, RenderShaderFormat format) {
    this->device = device;
    this->format = format;
    this->entryPointName = entryPointName ? entryPointName : "";
    
    if (format == RenderShaderFormat::DXBC) {
        if (strstr(entryPointName, "vs") || strstr(entryPointName, "VS") || strstr(entryPointName, "vertex")) {
            device->d3d->CreateVertexShader(data, (SIZE_T)size, nullptr, &vs);
        } else if (strstr(entryPointName, "ps") || strstr(entryPointName, "PS") || strstr(entryPointName, "pixel")) {
            device->d3d->CreatePixelShader(data, (SIZE_T)size, nullptr, &ps);
        } else if (strstr(entryPointName, "gs") || strstr(entryPointName, "GS") || strstr(entryPointName, "geometry")) {
            device->d3d->CreateGeometryShader(data, (SIZE_T)size, nullptr, &gs);
        } else if (strstr(entryPointName, "cs") || strstr(entryPointName, "CS") || strstr(entryPointName, "compute")) {
            device->d3d->CreateComputeShader(data, (SIZE_T)size, nullptr, &cs);
        }
    } else {
        fprintf(stderr, "D3D11: Non-DXBC shader format not supported yet\n");
    }
}

D3D11Shader::~D3D11Shader() {
    if (vs) vs->Release();
    if (ps) ps->Release();
    if (gs) gs->Release();
    if (cs) cs->Release();
}

void D3D11Shader::setName(const std::string& name) {
    if (vs) {
        const std::wstring wideName = Utf8ToUtf16(name + " (VS)");
        vs->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)wideName.size() * 2, wideName.c_str());
    }
    if (ps) {
        const std::wstring wideName = Utf8ToUtf16(name + " (PS)");
        ps->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)wideName.size() * 2, wideName.c_str());
    }
}

// D3D11Sampler
D3D11Sampler::D3D11Sampler(D3D11Device* device, const RenderSamplerDesc& desc) {
    this->device = device;
    this->desc = desc;
    
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = toFilter(desc.minFilter, desc.magFilter, desc.mipmapMode, desc.anisotropyEnabled, desc.comparisonEnabled);
    samplerDesc.AddressU = toD3D11(desc.addressModeU);
    samplerDesc.AddressV = toD3D11(desc.addressModeV);
    samplerDesc.AddressW = toD3D11(desc.addressModeW);
    samplerDesc.MipLODBias = desc.mipLODBias;
    samplerDesc.MaxAnisotropy = desc.maxAnisotropy;
    samplerDesc.ComparisonFunc = desc.comparisonEnabled ? toD3D11Comparison(desc.comparisonFunc) : D3D11_COMPARISON_NEVER;
    samplerDesc.BorderColor[0] = desc.borderColor[0];
    samplerDesc.BorderColor[1] = desc.borderColor[1];
    samplerDesc.BorderColor[2] = desc.borderColor[2];
    samplerDesc.BorderColor[3] = desc.borderColor[3];
    samplerDesc.MinLOD = desc.minLOD;
    samplerDesc.MaxLOD = desc.maxLOD;
    
    HRESULT res = device->d3d->CreateSamplerState(&samplerDesc, &d3d);
    if (FAILED(res)) {
        fprintf(stderr, "CreateSamplerState failed with error code 0x%lX.\n", res);
        d3d = nullptr;
    }
}

D3D11Sampler::~D3D11Sampler() {
    if (d3d) d3d->Release();
}

// D3D11Pipeline
D3D11Pipeline::D3D11Pipeline(D3D11Device* device, Type type) {
    this->device = device;
    this->type = type;
}

D3D11Pipeline::~D3D11Pipeline() {
}

// D3D11ComputePipeline
D3D11ComputePipeline::D3D11ComputePipeline(D3D11Device* device, const RenderComputePipelineDesc& desc) : D3D11Pipeline(device, Type::Compute) {
    computeShader = static_cast<D3D11Shader*>(desc.computeShader);
}

D3D11ComputePipeline::~D3D11ComputePipeline() {
}

void D3D11ComputePipeline::setName(const std::string& name) {
    if (computeShader) computeShader->setName(name);
}

RenderPipelineProgram D3D11ComputePipeline::getProgram(const std::string& name) const {
    return RenderPipelineProgram{};
}

void D3D11ComputePipeline::apply(ID3D11DeviceContext* context) {
    if (computeShader && computeShader->cs) {
        context->CSSetShader(computeShader->cs, nullptr, 0);
    }
}

// D3D11GraphicsPipeline
D3D11GraphicsPipeline::D3D11GraphicsPipeline(D3D11Device* device, const RenderGraphicsPipelineDesc& desc) : D3D11Pipeline(device, Type::Graphics) {
    vertexShader = static_cast<D3D11Shader*>(desc.vertexShader);
    pixelShader = static_cast<D3D11Shader*>(desc.pixelShader);
    geometryShader = static_cast<D3D11Shader*>(desc.geometryShader);
    
    if (desc.inputSlots) {
        inputSlots.assign(desc.inputSlots, desc.inputSlots + desc.inputSlotsCount);
    }
    
    topology = toD3D11(desc.primitiveTopology);
    
    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = (desc.polygonMode == RenderPolygonMode::FILL) ? D3D11_FILL_SOLID : D3D11_FILL_WIREFRAME;
    rasterDesc.CullMode = toD3D11CullMode(desc.cullMode);
    rasterDesc.FrontCounterClockwise = TRUE;
    rasterDesc.DepthBias = (INT)desc.depthBias;
    rasterDesc.DepthBiasClamp = desc.depthBiasClamp;
    rasterDesc.SlopeScaledDepthBias = desc.slopeScaledDepthBias;
    rasterDesc.DepthClipEnable = TRUE;
    rasterDesc.ScissorEnable = TRUE;
    rasterDesc.MultisampleEnable = (desc.sampleCount > RenderSampleCount::COUNT_1);
    rasterDesc.AntialiasedLineEnable = FALSE;
    device->d3d->CreateRasterizerState(&rasterDesc, &rasterizerState);
    
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = desc.blendState.enabled;
    blendDesc.RenderTarget[0].SrcBlend = toD3D11Blend(desc.blendState.srcColorBlend);
    blendDesc.RenderTarget[0].DestBlend = toD3D11Blend(desc.blendState.dstColorBlend);
    blendDesc.RenderTarget[0].BlendOp = toD3D11BlendOp(desc.blendState.colorBlendOp);
    blendDesc.RenderTarget[0].SrcBlendAlpha = toD3D11Blend(desc.blendState.srcAlphaBlend);
    blendDesc.RenderTarget[0].DestBlendAlpha = toD3D11Blend(desc.blendState.dstAlphaBlend);
    blendDesc.RenderTarget[0].BlendOpAlpha = toD3D11BlendOp(desc.blendState.alphaBlendOp);
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device->d3d->CreateBlendState(&blendDesc, &blendState);
    
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = desc.depthStencilState.depthTestEnabled;
    dsDesc.DepthWriteMask = desc.depthStencilState.depthWriteEnabled ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.DepthFunc = toD3D11Comparison(desc.depthStencilState.depthComparison);
    dsDesc.StencilEnable = desc.depthStencilState.stencilTestEnabled;
    dsDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
    dsDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
    dsDesc.FrontFace.StencilFunc = toD3D11Comparison(desc.depthStencilState.frontFace.stencilComparison);
    dsDesc.FrontFace.StencilDepthFailOp = toD3D11StencilOp(desc.depthStencilState.frontFace.depthFailOp);
    dsDesc.FrontFace.StencilPassOp = toD3D11StencilOp(desc.depthStencilState.frontFace.passOp);
    dsDesc.FrontFace.StencilFailOp = toD3D11StencilOp(desc.depthStencilState.frontFace.failOp);
    dsDesc.BackFace = dsDesc.FrontFace;
    device->d3d->CreateDepthStencilState(&dsDesc, &depthStencilState);
}

D3D11GraphicsPipeline::~D3D11GraphicsPipeline() {
    if (rasterizerState) rasterizerState->Release();
    if (blendState) blendState->Release();
    if (depthStencilState) depthStencilState->Release();
}

void D3D11GraphicsPipeline::setName(const std::string& name) {
    if (vertexShader) vertexShader->setName(name + " VS");
    if (pixelShader) pixelShader->setName(name + " PS");
}

RenderPipelineProgram D3D11GraphicsPipeline::getProgram(const std::string& name) const {
    return RenderPipelineProgram{};
}

void D3D11GraphicsPipeline::apply(ID3D11DeviceContext* context) {
    context->VSSetShader(vertexShader ? vertexShader->vs : nullptr, nullptr, 0);
    context->PSSetShader(pixelShader ? pixelShader->ps : nullptr, nullptr, 0);
    context->GSSetShader(geometryShader ? geometryShader->gs : nullptr, nullptr, 0);
    context->RSSetState(rasterizerState);
    context->OMSetBlendState(blendState, nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(depthStencilState, stencilRef);
    context->IASetPrimitiveTopology(topology);
}

// D3D11PipelineLayout
D3D11PipelineLayout::D3D11PipelineLayout(D3D11Device* device, const RenderPipelineLayoutDesc& desc) {
    this->device = device;
    
    if (desc.pushConstantRanges) {
        pushConstantRanges.assign(desc.pushConstantRanges, desc.pushConstantRanges + desc.pushConstantRangesCount);
    }
    
    setCount = desc.setCount;
}

D3D11PipelineLayout::~D3D11PipelineLayout() {
}

// D3D11CommandQueue
D3D11CommandQueue::D3D11CommandQueue(D3D11Device* device, RenderCommandListType type) {
    this->device = device;
    this->type = type;
    immediateContext = device->getImmediateContext();
}

D3D11CommandQueue::~D3D11CommandQueue() {
}

std::unique_ptr<RenderCommandList> D3D11CommandQueue::createCommandList() {
    ID3D11DeviceContext* deferredContext = nullptr;
    HRESULT res = device->d3d->CreateDeferredContext(0, &deferredContext);
    if (FAILED(res)) {
        return std::make_unique<D3D11CommandList>(this);
    }
    
    auto cmdList = std::make_unique<D3D11CommandList>(this);
    cmdList->deferredContext = deferredContext;
    return cmdList;
}

std::unique_ptr<RenderSwapChain> D3D11CommandQueue::createSwapChain(const RenderSwapChainDesc& desc) {
    return std::make_unique<D3D11SwapChain>(this, desc);
}

void D3D11CommandQueue::executeCommandLists(const RenderCommandList** commandLists, uint32_t commandListCount,
    RenderCommandSemaphore** waitSemaphores, uint32_t waitSemaphoreCount,
    RenderCommandSemaphore** signalSemaphores, uint32_t signalSemaphoreCount,
    RenderCommandFence* signalFence) {
    for (uint32_t i = 0; i < commandListCount; i++) {
        const D3D11CommandList* cmdList = static_cast<const D3D11CommandList*>(commandLists[i]);
        if (cmdList) {
            const_cast<D3D11CommandList*>(cmdList)->execute(immediateContext);
        }
    }
}

void D3D11CommandQueue::waitForCommandFence(RenderCommandFence* fence) {
    immediateContext->Flush();
}

// D3D11Pool
D3D11Pool::D3D11Pool(D3D11Device* device, const RenderPoolDesc& desc) {
    this->device = device;
    this->desc = desc;
}

D3D11Pool::~D3D11Pool() {
}

std::unique_ptr<RenderBuffer> D3D11Pool::createBuffer(const RenderBufferDesc& desc) {
    return std::make_unique<D3D11Buffer>(device, this, desc);
}

std::unique_ptr<RenderTexture> D3D11Pool::createTexture(const RenderTextureDesc& desc) {
    return std::make_unique<D3D11Texture>(device, this, desc);
}

// D3D11Device
D3D11Device::D3D11Device(D3D11Interface* renderInterface, const std::string& preferredDeviceName) {
    this->renderInterface = renderInterface;
    
    IDXGIAdapter1* selectedAdapter = nullptr;
    if (!preferredDeviceName.empty()) {
        UINT adapterIndex = 0;
        IDXGIAdapter1* adapter = nullptr;
        while (renderInterface->dxgiFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND) {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);
            std::string adapterName = Utf16ToUtf8(desc.Description);
            if (adapterName == preferredDeviceName) {
                selectedAdapter = adapter;
                break;
            }
            adapter->Release();
            adapterIndex++;
        }
    }
    
    if (!selectedAdapter) {
        renderInterface->dxgiFactory->EnumAdapters1(0, &selectedAdapter);
    }
    
    this->adapter = selectedAdapter;
    
    UINT createDeviceFlags = 0;
#ifdef D3D11_DEBUG_LAYER_ENABLED
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    
    D3D_FEATURE_LEVEL featureLevel;
    
    HRESULT res = D3D11CreateDevice(
        selectedAdapter,
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        createDeviceFlags,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &d3d,
        &featureLevel,
        &immediateContext
    );
    
    if (FAILED(res)) {
        fprintf(stderr, "D3D11CreateDevice failed with error code 0x%lX.\n", res);
        d3d = nullptr;
        immediateContext = nullptr;
        return;
    }
    
    capabilities.deviceName = "D3D11 Device";
    capabilities.maxTextureSize = 16384;
    capabilities.maxTextureArrayLayers = 2048;
    capabilities.maxColorAttachments = 8;
    capabilities.timestampQueriesSupported = false;
    capabilities.raytracingSupported = false;
    capabilities.bufferDeviceAddressSupported = false;
    capabilities.shaderModel = D3D_SHADER_MODEL_5_0;
    
    if (selectedAdapter) {
        DXGI_ADAPTER_DESC1 adapterDesc;
        selectedAdapter->GetDesc1(&adapterDesc);
        description.name = Utf16ToUtf8(adapterDesc.Description);
        description.vendorId = adapterDesc.VendorId;
        description.deviceId = adapterDesc.DeviceId;
        description.dedicatedVideoMemory = adapterDesc.DedicatedVideoMemory;
        description.dedicatedSystemMemory = adapterDesc.DedicatedSystemMemory;
        description.sharedSystemMemory = adapterDesc.SharedSystemMemory;
    }
    
    description.apiVersion = "D3D11";
}

D3D11Device::~D3D11Device() {
    if (immediateContext) immediateContext->Release();
    if (d3d) d3d->Release();
    if (adapter) adapter->Release();
}

ID3D11DeviceContext* D3D11Device::getImmediateContext() const {
    return immediateContext;
}

std::unique_ptr<RenderDescriptorSet> D3D11Device::createDescriptorSet(const RenderDescriptorSetDesc& desc) {
    return std::make_unique<D3D11DescriptorSet>(this, desc);
}

std::unique_ptr<RenderShader> D3D11Device::createShader(const void* data, uint64_t size, const char* entryPointName, RenderShaderFormat format) {
    return std::make_unique<D3D11Shader>(this, data, size, entryPointName, format);
}

std::unique_ptr<RenderSampler> D3D11Device::createSampler(const RenderSamplerDesc& desc) {
    return std::make_unique<D3D11Sampler>(this, desc);
}

std::unique_ptr<RenderPipeline> D3D11Device::createComputePipeline(const RenderComputePipelineDesc& desc) {
    return std::make_unique<D3D11ComputePipeline>(this, desc);
}

std::unique_ptr<RenderPipeline> D3D11Device::createGraphicsPipeline(const RenderGraphicsPipelineDesc& desc) {
    return std::make_unique<D3D11GraphicsPipeline>(this, desc);
}

std::unique_ptr<RenderPipeline> D3D11Device::createRaytracingPipeline(const RenderRaytracingPipelineDesc& desc, const RenderPipeline* previousPipeline) {
    fprintf(stderr, "D3D11: Raytracing not supported\n");
    return nullptr;
}

std::unique_ptr<RenderCommandQueue> D3D11Device::createCommandQueue(RenderCommandListType type) {
    return std::make_unique<D3D11CommandQueue>(this, type);
}

std::unique_ptr<RenderBuffer> D3D11Device::createBuffer(const RenderBufferDesc& desc) {
    return std::make_unique<D3D11Buffer>(this, nullptr, desc);
}

std::unique_ptr<RenderTexture> D3D11Device::createTexture(const RenderTextureDesc& desc) {
    return std::make_unique<D3D11Texture>(this, nullptr, desc);
}

std::unique_ptr<RenderAccelerationStructure> D3D11Device::createAccelerationStructure(const RenderAccelerationStructureDesc& desc) {
    fprintf(stderr, "D3D11: Acceleration structures not supported\n");
    return nullptr;
}

std::unique_ptr<RenderPool> D3D11Device::createPool(const RenderPoolDesc& desc) {
    return std::make_unique<D3D11Pool>(this, desc);
}

std::unique_ptr<RenderPipelineLayout> D3D11Device::createPipelineLayout(const RenderPipelineLayoutDesc& desc) {
    return std::make_unique<D3D11PipelineLayout>(this, desc);
}

std::unique_ptr<RenderCommandFence> D3D11Device::createCommandFence() {
    fprintf(stderr, "D3D11: Command fences not supported\n");
    return nullptr;
}

std::unique_ptr<RenderCommandSemaphore> D3D11Device::createCommandSemaphore() {
    fprintf(stderr, "D3D11: Command semaphores not supported\n");
    return nullptr;
}

std::unique_ptr<RenderFramebuffer> D3D11Device::createFramebuffer(const RenderFramebufferDesc& desc) {
    return std::make_unique<D3D11Framebuffer>(this, desc);
}

std::unique_ptr<RenderQueryPool> D3D11Device::createQueryPool(uint32_t queryCount) {
    fprintf(stderr, "D3D11: Query pools not implemented\n");
    return nullptr;
}

void D3D11Device::setBottomLevelASBuildInfo(RenderBottomLevelASBuildInfo& buildInfo, const RenderBottomLevelASMesh* meshes,
    uint32_t meshCount, bool preferFastBuild, bool preferFastTrace) {
    // Not supported
}

void D3D11Device::setTopLevelASBuildInfo(RenderTopLevelASBuildInfo& buildInfo, const RenderTopLevelASInstance* instances,
    uint32_t instanceCount, bool preferFastBuild, bool preferFastTrace) {
    // Not supported
}

void D3D11Device::setShaderBindingTableInfo(RenderShaderBindingTableInfo& tableInfo, const RenderShaderBindingGroups& groups,
    const RenderPipeline* pipeline, RenderDescriptorSet** descriptorSets, uint32_t descriptorSetCount) {
    // Not supported
}

const RenderDeviceCapabilities& D3D11Device::getCapabilities() const {
    return capabilities;
}

const RenderDeviceDescription& D3D11Device::getDescription() const {
    return description;
}

RenderSampleCounts D3D11Device::getSampleCountsSupported(RenderFormat format) const {
    UINT qualityLevels;
    RenderSampleCounts counts = RenderSampleCount::COUNT_1;
    
    DXGI_FORMAT dxgiFormat = toDXGI(format);
    if (SUCCEEDED(d3d->CheckMultisampleQualityLevels(dxgiFormat, 2, &qualityLevels)) && qualityLevels > 0) {
        counts = static_cast<RenderSampleCounts>(counts | RenderSampleCount::COUNT_2);
    }
    if (SUCCEEDED(d3d->CheckMultisampleQualityLevels(dxgiFormat, 4, &qualityLevels)) && qualityLevels > 0) {
        counts = static_cast<RenderSampleCounts>(counts | RenderSampleCount::COUNT_4);
    }
    if (SUCCEEDED(d3d->CheckMultisampleQualityLevels(dxgiFormat, 8, &qualityLevels)) && qualityLevels > 0) {
        counts = static_cast<RenderSampleCounts>(counts | RenderSampleCount::COUNT_8);
    }
    
    return counts;
}

bool D3D11Device::beginCapture() {
    return false;
}

bool D3D11Device::endCapture() {
    return false;
}

// D3D11Interface
D3D11Interface::D3D11Interface() {
    HRESULT res = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
    if (FAILED(res)) {
        fprintf(stderr, "CreateDXGIFactory1 failed with error code 0x%lX.\n", res);
        dxgiFactory = nullptr;
        return;
    }
    
    UINT adapterIndex = 0;
    IDXGIAdapter1* adapter = nullptr;
    while (dxgiFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        deviceNames.push_back(Utf16ToUtf8(desc.Description));
        adapter->Release();
        adapterIndex++;
    }
    
    capabilities.backendName = "D3D11";
    capabilities.raytracingSupported = false;
    capabilities.timelineSemaphoresSupported = false;
    capabilities.swapChainPresentWaitSupported = false;
    capabilities.displayTimingSupported = false;
}

D3D11Interface::~D3D11Interface() {
    if (dxgiFactory) dxgiFactory->Release();
}

std::unique_ptr<RenderDevice> D3D11Interface::createDevice(const std::string& preferredDeviceName) {
    return std::make_unique<D3D11Device>(this, preferredDeviceName);
}

const RenderInterfaceCapabilities& D3D11Interface::getCapabilities() const {
    return capabilities;
}

const std::vector<std::string>& D3D11Interface::getDeviceNames() const {
    return deviceNames;
}
std::unique_ptr<RenderInterface> CreateD3D11Interface() {
    return std::make_unique<D3D11Interface>();
}

} // namespace plume