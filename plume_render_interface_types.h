//
// plume
//
// Copyright (c) 2024 renderbag and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE file for details.
//

#pragma once

#include <assert.h>
#include <float.h>
#include <stdint.h>

#ifdef __cplusplus
#include <list>
#include <memory>
#include <string>
#include <vector>
#else
#include <stdbool.h>
#endif

#if defined(_WIN64)
#include <Windows.h>
#elif defined(__ANDROID__)
#include "android/native_window.h"
#elif defined(__linux__) && !defined(PLUME_SDL_VULKAN_ENABLED)
#include "X11/Xlib.h"
#undef None
#undef Status
#undef LockMask
#undef ControlMask
#undef Success
#undef Always
#endif

#ifdef PLUME_SDL_VULKAN_ENABLED
#include <SDL_vulkan.h>
#endif

#ifdef __cplusplus
#define PLUME_ENUM_BEGIN(NAME) enum class NAME {
#define PLUME_ENUM_END(NAME) };
#define PLUME_ENUM_VALUE(NAME, VAL) VAL
#define PLUME_DEFAULT(value) = value
#else
#define PLUME_ENUM_BEGIN(NAME) typedef enum {
#define PLUME_ENUM_END(NAME) } NAME;
#define PLUME_ENUM_VALUE(NAME, VAL) NAME##_##VAL
#define PLUME_DEFAULT(value)
#endif

#ifndef __cplusplus
typedef struct RenderColor RenderColor;
typedef struct RenderAffineTransform RenderAffineTransform;
typedef struct RenderDepth RenderDepth;
typedef struct RenderMultisamplingLocation RenderMultisamplingLocation;
typedef struct RenderMultisampling RenderMultisampling;
typedef struct RenderBufferReference RenderBufferReference;
typedef struct RenderBufferBarrier RenderBufferBarrier;
typedef struct RenderBufferStructuredView RenderBufferStructuredView;
typedef struct RenderTextureBarrier RenderTextureBarrier;
typedef struct RenderClearValue RenderClearValue;
typedef struct RenderBufferDesc RenderBufferDesc;
typedef struct RenderTextureDesc RenderTextureDesc;
typedef struct RenderComponentMapping RenderComponentMapping;
typedef struct RenderTextureViewDesc RenderTextureViewDesc;
typedef struct RenderAccelerationStructureDesc RenderAccelerationStructureDesc;
typedef struct RenderTextureCopyLocation RenderTextureCopyLocation;
typedef struct RenderPoolDesc RenderPoolDesc;
typedef struct RenderSwapChainDesc RenderSwapChainDesc;
typedef struct RenderInputSlot RenderInputSlot;
typedef struct RenderInputElement RenderInputElement;
typedef struct RenderBlendDesc RenderBlendDesc;
typedef struct RenderStencilFaceDesc RenderStencilFaceDesc;
typedef struct RenderSpecConstant RenderSpecConstant;
typedef struct RenderComputePipelineDesc RenderComputePipelineDesc;
typedef struct RenderGraphicsPipelineDesc RenderGraphicsPipelineDesc;
typedef struct RenderRaytracingPipelineLibrarySymbol RenderRaytracingPipelineLibrarySymbol;
typedef struct RenderRaytracingPipelineLibrary RenderRaytracingPipelineLibrary;
typedef struct RenderRaytracingPipelineHitGroup RenderRaytracingPipelineHitGroup;
typedef struct RenderRaytracingPipelineDesc RenderRaytracingPipelineDesc;
typedef struct RenderPipelineProgram RenderPipelineProgram;
typedef struct RenderSamplerDesc RenderSamplerDesc;
typedef struct RenderDescriptorRange RenderDescriptorRange;
typedef struct RenderDescriptorSetDesc RenderDescriptorSetDesc;
typedef struct RenderPushConstantRange RenderPushConstantRange;
typedef struct RenderRootDescriptorDesc RenderRootDescriptorDesc;
typedef struct RenderPipelineLayoutDesc RenderPipelineLayoutDesc;
typedef struct RenderIndexBufferView RenderIndexBufferView;
typedef struct RenderVertexBufferView RenderVertexBufferView;
typedef struct RenderViewport RenderViewport;
typedef struct RenderRect RenderRect;
typedef struct RenderBox RenderBox;
typedef struct RenderRange RenderRange;
typedef struct RenderFramebufferDesc RenderFramebufferDesc;
typedef struct RenderBottomLevelASMesh RenderBottomLevelASMesh;
typedef struct RenderBottomLevelASBuildInfo RenderBottomLevelASBuildInfo;
typedef struct RenderTopLevelASInstance RenderTopLevelASInstance;
typedef struct RenderTopLevelASBuildInfo RenderTopLevelASBuildInfo;
typedef struct RenderShaderBindingGroup RenderShaderBindingGroup;
typedef struct RenderShaderBindingGroups RenderShaderBindingGroups;
typedef struct RenderShaderBindingGroupInfo RenderShaderBindingGroupInfo;
typedef struct RenderShaderBindingGroupsInfo RenderShaderBindingGroupsInfo;
typedef struct RenderShaderBindingTableInfo RenderShaderBindingTableInfo;
typedef struct RenderDeviceDescription RenderDeviceDescription;
typedef struct RenderDeviceCapabilities RenderDeviceCapabilities;
typedef struct RenderWindow RenderWindow;
typedef struct RenderInterfaceCapabilities RenderInterfaceCapabilities;
typedef struct RenderBufferFormattedView RenderBufferFormattedView;
#endif

#ifdef __cplusplus
namespace plume {
#endif
#if defined(_WIN64)
    // Native HWND handle to the target window.
    typedef HWND RenderWindow;
#elif defined(__ANDROID__)
    typedef ANativeWindow* RenderWindow;
#elif defined(PLUME_SDL_VULKAN_ENABLED)
    typedef SDL_Window *RenderWindow;
#elif defined(__linux__)
    struct RenderWindow {
        Display* display;
        Window window;

    #ifdef __cplusplus
        bool operator==(const struct RenderWindow& rhs) const {
            return display == rhs.display && window == rhs.window;
        }
        bool operator!=(const struct RenderWindow& rhs) const { return !(*this == rhs); }
    #endif
    };
#elif defined(__APPLE__)
    struct RenderWindow {
        void* window;
        void* view;

    #ifdef __cplusplus
        bool operator==(const struct RenderWindow& rhs) const {
            return window == rhs.window && view == rhs.view;
        }
        bool operator!=(const struct RenderWindow& rhs) const { return !(*this == rhs); }
    #endif
    };
#else
    static_assert(false, "RenderWindow was not defined for this platform.");
#endif

    typedef struct RenderBuffer RenderBuffer;
    typedef struct RenderDescriptorSet RenderDescriptorSet;
    typedef struct RenderPipeline RenderPipeline;
    typedef struct RenderPipelineLayout RenderPipelineLayout;
    typedef struct RenderSampler RenderSampler;
    typedef struct RenderShader RenderShader;
    typedef struct RenderTexture RenderTexture;
    typedef struct RenderTextureView RenderTextureView;
    typedef struct RenderQueryPool RenderQueryPool;

    // Enums.

    PLUME_ENUM_BEGIN(RenderDeviceVendor)
        PLUME_ENUM_VALUE(RenderDeviceVencor, UNKNOWN) = 0x0,
        PLUME_ENUM_VALUE(RenderDeviceVencor, AMD) = 0x1002,
        PLUME_ENUM_VALUE(RenderDeviceVencor, NVIDIA) = 0x10DE,
        PLUME_ENUM_VALUE(RenderDeviceVencor, INTEL) = 0x8086,
        PLUME_ENUM_VALUE(RenderDeviceVencor, APPLE) = 0x106B,
    PLUME_ENUM_END(RenderDeviceVendor)

    PLUME_ENUM_BEGIN(RenderFormat)
        PLUME_ENUM_VALUE(RenderFormat, UNKNOWN),
        PLUME_ENUM_VALUE(RenderFormat, R32G32B32A32_TYPELESS),
        PLUME_ENUM_VALUE(RenderFormat, R32G32B32A32_FLOAT),
        PLUME_ENUM_VALUE(RenderFormat, R32G32B32A32_UINT),
        PLUME_ENUM_VALUE(RenderFormat, R32G32B32A32_SINT),
        PLUME_ENUM_VALUE(RenderFormat, R32G32B32_TYPELESS),
        PLUME_ENUM_VALUE(RenderFormat, R32G32B32_FLOAT),
        PLUME_ENUM_VALUE(RenderFormat, R32G32B32_UINT),
        PLUME_ENUM_VALUE(RenderFormat, R32G32B32_SINT),
        PLUME_ENUM_VALUE(RenderFormat, R16G16B16A16_TYPELESS),
        PLUME_ENUM_VALUE(RenderFormat, R16G16B16A16_FLOAT),
        PLUME_ENUM_VALUE(RenderFormat, R16G16B16A16_UNORM),
        PLUME_ENUM_VALUE(RenderFormat, R16G16B16A16_UINT),
        PLUME_ENUM_VALUE(RenderFormat, R16G16B16A16_SNORM),
        PLUME_ENUM_VALUE(RenderFormat, R16G16B16A16_SINT),
        PLUME_ENUM_VALUE(RenderFormat, R32G32_TYPELESS),
        PLUME_ENUM_VALUE(RenderFormat, R32G32_FLOAT),
        PLUME_ENUM_VALUE(RenderFormat, R32G32_UINT),
        PLUME_ENUM_VALUE(RenderFormat, R32G32_SINT),
        PLUME_ENUM_VALUE(RenderFormat, R8G8B8A8_TYPELESS),
        PLUME_ENUM_VALUE(RenderFormat, R8G8B8A8_UNORM),
        PLUME_ENUM_VALUE(RenderFormat, R8G8B8A8_UINT),
        PLUME_ENUM_VALUE(RenderFormat, R8G8B8A8_SNORM),
        PLUME_ENUM_VALUE(RenderFormat, R8G8B8A8_SINT),
        PLUME_ENUM_VALUE(RenderFormat, B8G8R8A8_UNORM),
        PLUME_ENUM_VALUE(RenderFormat, R16G16_TYPELESS),
        PLUME_ENUM_VALUE(RenderFormat, R16G16_FLOAT),
        PLUME_ENUM_VALUE(RenderFormat, R16G16_UNORM),
        PLUME_ENUM_VALUE(RenderFormat, R16G16_UINT),
        PLUME_ENUM_VALUE(RenderFormat, R16G16_SNORM),
        PLUME_ENUM_VALUE(RenderFormat, R16G16_SINT),
        PLUME_ENUM_VALUE(RenderFormat, R32_TYPELESS),
        PLUME_ENUM_VALUE(RenderFormat, D32_FLOAT),
        PLUME_ENUM_VALUE(RenderFormat, D32_FLOAT_S8_UINT),
        PLUME_ENUM_VALUE(RenderFormat, R32_FLOAT),
        PLUME_ENUM_VALUE(RenderFormat, R32_UINT),
        PLUME_ENUM_VALUE(RenderFormat, R32_SINT),
        PLUME_ENUM_VALUE(RenderFormat, R8G8_TYPELESS),
        PLUME_ENUM_VALUE(RenderFormat, R8G8_UNORM),
        PLUME_ENUM_VALUE(RenderFormat, R8G8_UINT),
        PLUME_ENUM_VALUE(RenderFormat, R8G8_SNORM),
        PLUME_ENUM_VALUE(RenderFormat, R8G8_SINT),
        PLUME_ENUM_VALUE(RenderFormat, R16_TYPELESS),
        PLUME_ENUM_VALUE(RenderFormat, R16_FLOAT),
        PLUME_ENUM_VALUE(RenderFormat, D16_UNORM),
        PLUME_ENUM_VALUE(RenderFormat, R16_UNORM),
        PLUME_ENUM_VALUE(RenderFormat, R16_UINT),
        PLUME_ENUM_VALUE(RenderFormat, R16_SNORM),
        PLUME_ENUM_VALUE(RenderFormat, R16_SINT),
        PLUME_ENUM_VALUE(RenderFormat, R8_TYPELESS),
        PLUME_ENUM_VALUE(RenderFormat, R8_UNORM),
        PLUME_ENUM_VALUE(RenderFormat, R8_UINT),
        PLUME_ENUM_VALUE(RenderFormat, R8_SNORM),
        PLUME_ENUM_VALUE(RenderFormat, R8_SINT),
        PLUME_ENUM_VALUE(RenderFormat, BC1_TYPELESS),
        PLUME_ENUM_VALUE(RenderFormat, BC1_UNORM),
        PLUME_ENUM_VALUE(RenderFormat, BC1_UNORM_SRGB),
        PLUME_ENUM_VALUE(RenderFormat, BC2_TYPELESS),
        PLUME_ENUM_VALUE(RenderFormat, BC2_UNORM),
        PLUME_ENUM_VALUE(RenderFormat, BC2_UNORM_SRGB),
        PLUME_ENUM_VALUE(RenderFormat, BC3_TYPELESS),
        PLUME_ENUM_VALUE(RenderFormat, BC3_UNORM),
        PLUME_ENUM_VALUE(RenderFormat, BC3_UNORM_SRGB),
        PLUME_ENUM_VALUE(RenderFormat, BC4_TYPELESS),
        PLUME_ENUM_VALUE(RenderFormat, BC4_UNORM),
        PLUME_ENUM_VALUE(RenderFormat, BC4_SNORM),
        PLUME_ENUM_VALUE(RenderFormat, BC5_TYPELESS),
        PLUME_ENUM_VALUE(RenderFormat, BC5_UNORM),
        PLUME_ENUM_VALUE(RenderFormat, BC5_SNORM),
        PLUME_ENUM_VALUE(RenderFormat, BC6H_TYPELESS),
        PLUME_ENUM_VALUE(RenderFormat, BC6H_UF16),
        PLUME_ENUM_VALUE(RenderFormat, BC6H_SF16),
        PLUME_ENUM_VALUE(RenderFormat, BC7_TYPELESS),
        PLUME_ENUM_VALUE(RenderFormat, BC7_UNORM),
        PLUME_ENUM_VALUE(RenderFormat, BC7_UNORM_SRGB),
        PLUME_ENUM_VALUE(RenderFormat, MAX)
    PLUME_ENUM_END(RenderFormat)

    PLUME_ENUM_BEGIN(RenderTextureDimension)
        PLUME_ENUM_VALUE(RenderTextureDimension, UNKNOWN),
        PLUME_ENUM_VALUE(RenderTextureDimension, TEXTURE_1D),
        PLUME_ENUM_VALUE(RenderTextureDimension, TEXTURE_2D),
        PLUME_ENUM_VALUE(RenderTextureDimension, TEXTURE_3D)
    PLUME_ENUM_END(RenderTextureDimension)

    PLUME_ENUM_BEGIN(RenderTextureViewDimension)
        PLUME_ENUM_VALUE(RenderTextureViewDimension, UNKNOWN),
        PLUME_ENUM_VALUE(RenderTextureViewDimension, TEXTURE_1D),
        PLUME_ENUM_VALUE(RenderTextureViewDimension, TEXTURE_2D),
        PLUME_ENUM_VALUE(RenderTextureViewDimension, TEXTURE_3D),
        PLUME_ENUM_VALUE(RenderTextureViewDimension, TEXTURE_CUBE)
    PLUME_ENUM_END(RenderTextureViewDimension)

    PLUME_ENUM_BEGIN(RenderCommandListType)
        PLUME_ENUM_VALUE(RenderCommandListType, UNKNOWN),
        PLUME_ENUM_VALUE(RenderCommandListType, DIRECT),
        PLUME_ENUM_VALUE(RenderCommandListType, COMPUTE),
        PLUME_ENUM_VALUE(RenderCommandListType, COPY)
    PLUME_ENUM_END(RenderCommandListType)

    PLUME_ENUM_BEGIN(RenderPrimitiveTopology)
        PLUME_ENUM_VALUE(RenderPrimitiveTopology, UNKNOWN),
        PLUME_ENUM_VALUE(RenderPrimitiveTopology, POINT_LIST),
        PLUME_ENUM_VALUE(RenderPrimitiveTopology, LINE_LIST),
        PLUME_ENUM_VALUE(RenderPrimitiveTopology, LINE_STRIP),
        PLUME_ENUM_VALUE(RenderPrimitiveTopology, TRIANGLE_LIST),
        PLUME_ENUM_VALUE(RenderPrimitiveTopology, TRIANGLE_STRIP),
        PLUME_ENUM_VALUE(RenderPrimitiveTopology, TRIANGLE_FAN)
    PLUME_ENUM_END(RenderPrimitiveTopology)

    PLUME_ENUM_BEGIN(RenderCullMode)
        PLUME_ENUM_VALUE(RenderCullMode, UNKNOWN),
        PLUME_ENUM_VALUE(RenderCullMode, NONE),
        PLUME_ENUM_VALUE(RenderCullMode, FRONT),
        PLUME_ENUM_VALUE(RenderCullMode, BACK)
    PLUME_ENUM_END(RenderCullMode)

    PLUME_ENUM_BEGIN(RenderFrontFace)
        PLUME_ENUM_VALUE(RenderFrontFace, UNKNOWN),
        PLUME_ENUM_VALUE(RenderFrontFace, CLOCKWISE),
        PLUME_ENUM_VALUE(RenderFrontFace, COUNTER_CLOCKWISE)
    PLUME_ENUM_END(RenderFrontFace)

    PLUME_ENUM_BEGIN(RenderComparisonFunction)
        PLUME_ENUM_VALUE(RenderComparisonFunction, UNKNOWN),
        PLUME_ENUM_VALUE(RenderComparisonFunction, NEVER),
        PLUME_ENUM_VALUE(RenderComparisonFunction, LESS),
        PLUME_ENUM_VALUE(RenderComparisonFunction, EQUAL),
        PLUME_ENUM_VALUE(RenderComparisonFunction, LESS_EQUAL),
        PLUME_ENUM_VALUE(RenderComparisonFunction, GREATER),
        PLUME_ENUM_VALUE(RenderComparisonFunction, NOT_EQUAL),
        PLUME_ENUM_VALUE(RenderComparisonFunction, GREATER_EQUAL),
        PLUME_ENUM_VALUE(RenderComparisonFunction, ALWAYS)
    PLUME_ENUM_END(RenderComparisonFunction)

    PLUME_ENUM_BEGIN(RenderStencilOp)
        PLUME_ENUM_VALUE(RenderStencilOp, UNKNOWN),
        PLUME_ENUM_VALUE(RenderStencilOp, KEEP),
        PLUME_ENUM_VALUE(RenderStencilOp, ZERO),
        PLUME_ENUM_VALUE(RenderStencilOp, REPLACE),
        PLUME_ENUM_VALUE(RenderStencilOp, INCREMENT_AND_CLAMP),
        PLUME_ENUM_VALUE(RenderStencilOp, DECREMENT_AND_CLAMP),
        PLUME_ENUM_VALUE(RenderStencilOp, INVERT),
        PLUME_ENUM_VALUE(RenderStencilOp, INCREMENT_AND_WRAP),
        PLUME_ENUM_VALUE(RenderStencilOp, DECREMENT_AND_WRAP)
    PLUME_ENUM_END(RenderStencilOp)

    PLUME_ENUM_BEGIN(RenderInputSlotClassification)
        PLUME_ENUM_VALUE(RenderInputSlotClassification, UNKNOWN),
        PLUME_ENUM_VALUE(RenderInputSlotClassification, PER_VERTEX_DATA),
        PLUME_ENUM_VALUE(RenderInputSlotClassification, PER_INSTANCE_DATA)
    PLUME_ENUM_END(RenderInputSlotClassification)

    PLUME_ENUM_BEGIN(RenderBlend)
        PLUME_ENUM_VALUE(RenderBlend, UNKNOWN),
        PLUME_ENUM_VALUE(RenderBlend, ZERO),
        PLUME_ENUM_VALUE(RenderBlend, ONE),
        PLUME_ENUM_VALUE(RenderBlend, SRC_COLOR),
        PLUME_ENUM_VALUE(RenderBlend, INV_SRC_COLOR),
        PLUME_ENUM_VALUE(RenderBlend, SRC_ALPHA),
        PLUME_ENUM_VALUE(RenderBlend, INV_SRC_ALPHA),
        PLUME_ENUM_VALUE(RenderBlend, DEST_ALPHA),
        PLUME_ENUM_VALUE(RenderBlend, INV_DEST_ALPHA),
        PLUME_ENUM_VALUE(RenderBlend, DEST_COLOR),
        PLUME_ENUM_VALUE(RenderBlend, INV_DEST_COLOR),
        PLUME_ENUM_VALUE(RenderBlend, SRC_ALPHA_SAT),
        PLUME_ENUM_VALUE(RenderBlend, BLEND_FACTOR),
        PLUME_ENUM_VALUE(RenderBlend, INV_BLEND_FACTOR),
        PLUME_ENUM_VALUE(RenderBlend, SRC1_COLOR),
        PLUME_ENUM_VALUE(RenderBlend, INV_SRC1_COLOR),
        PLUME_ENUM_VALUE(RenderBlend, SRC1_ALPHA),
        PLUME_ENUM_VALUE(RenderBlend, INV_SRC1_ALPHA)
    PLUME_ENUM_END(RenderBlend)

    PLUME_ENUM_BEGIN(RenderBlendOperation)
        PLUME_ENUM_VALUE(RenderBlendOperation, UNKNOWN),
        PLUME_ENUM_VALUE(RenderBlendOperation, ADD),
        PLUME_ENUM_VALUE(RenderBlendOperation, SUBTRACT),
        PLUME_ENUM_VALUE(RenderBlendOperation, REV_SUBTRACT),
        PLUME_ENUM_VALUE(RenderBlendOperation, MIN),
        PLUME_ENUM_VALUE(RenderBlendOperation, MAX)
    PLUME_ENUM_END(RenderBlendOperation)

    PLUME_ENUM_BEGIN(RenderLogicOperation)
        PLUME_ENUM_VALUE(RenderLogicOperation, UNKNOWN),
        PLUME_ENUM_VALUE(RenderLogicOperation, CLEAR),
        PLUME_ENUM_VALUE(RenderLogicOperation, SET),
        PLUME_ENUM_VALUE(RenderLogicOperation, COPY),
        PLUME_ENUM_VALUE(RenderLogicOperation, COPY_INVERTED),
        PLUME_ENUM_VALUE(RenderLogicOperation, NOOP),
        PLUME_ENUM_VALUE(RenderLogicOperation, INVERT),
        PLUME_ENUM_VALUE(RenderLogicOperation, AND),
        PLUME_ENUM_VALUE(RenderLogicOperation, NAND),
        PLUME_ENUM_VALUE(RenderLogicOperation, OR),
        PLUME_ENUM_VALUE(RenderLogicOperation, NOR),
        PLUME_ENUM_VALUE(RenderLogicOperation, XOR),
        PLUME_ENUM_VALUE(RenderLogicOperation, EQUIV),
        PLUME_ENUM_VALUE(RenderLogicOperation, AND_REVERSE),
        PLUME_ENUM_VALUE(RenderLogicOperation, AND_INVERTED),
        PLUME_ENUM_VALUE(RenderLogicOperation, OR_REVERSE),
        PLUME_ENUM_VALUE(RenderLogicOperation, OR_INVERTED)
    PLUME_ENUM_END(RenderLogicOperation)

    PLUME_ENUM_BEGIN(RenderFilter)
        PLUME_ENUM_VALUE(RenderFilter, UNKNOWN),
        PLUME_ENUM_VALUE(RenderFilter, NEAREST),
        PLUME_ENUM_VALUE(RenderFilter, LINEAR)
    PLUME_ENUM_END(RenderFilter)

    PLUME_ENUM_BEGIN(RenderMipmapMode)
        PLUME_ENUM_VALUE(RenderMipmapMode, UNKNOWN),
        PLUME_ENUM_VALUE(RenderMipmapMode, NEAREST),
        PLUME_ENUM_VALUE(RenderMipmapMode, LINEAR)
    PLUME_ENUM_END(RenderMipmapMode)

    PLUME_ENUM_BEGIN(RenderTextureAddressMode)
        PLUME_ENUM_VALUE(RenderTextureAddressMode, UNKNOWN),
        PLUME_ENUM_VALUE(RenderTextureAddressMode, WRAP),
        PLUME_ENUM_VALUE(RenderTextureAddressMode, MIRROR),
        PLUME_ENUM_VALUE(RenderTextureAddressMode, CLAMP),
        PLUME_ENUM_VALUE(RenderTextureAddressMode, BORDER),
        PLUME_ENUM_VALUE(RenderTextureAddressMode, MIRROR_ONCE)
    PLUME_ENUM_END(RenderTextureAddressMode)

    PLUME_ENUM_BEGIN(RenderBorderColor)
        PLUME_ENUM_VALUE(RenderBorderColor, UNKNOWN),
        PLUME_ENUM_VALUE(RenderBorderColor, TRANSPARENT_BLACK),
        PLUME_ENUM_VALUE(RenderBorderColor, OPAQUE_BLACK),
        PLUME_ENUM_VALUE(RenderBorderColor, OPAQUE_WHITE)
    PLUME_ENUM_END(RenderBorderColor)

    PLUME_ENUM_BEGIN(RenderShaderVisibility)
        PLUME_ENUM_VALUE(RenderShaderVisibility, UNKNOWN),
        PLUME_ENUM_VALUE(RenderShaderVisibility, ALL),
        PLUME_ENUM_VALUE(RenderShaderVisibility, VERTEX),
        PLUME_ENUM_VALUE(RenderShaderVisibility, GEOMETRY),
        PLUME_ENUM_VALUE(RenderShaderVisibility, PIXEL)
    PLUME_ENUM_END(RenderShaderVisibility)

    PLUME_ENUM_BEGIN(RenderDescriptorRangeType)
        PLUME_ENUM_VALUE(RenderDescriptorRangeType, UNKNOWN),
        PLUME_ENUM_VALUE(RenderDescriptorRangeType, CONSTANT_BUFFER),
        PLUME_ENUM_VALUE(RenderDescriptorRangeType, FORMATTED_BUFFER),
        PLUME_ENUM_VALUE(RenderDescriptorRangeType, READ_WRITE_FORMATTED_BUFFER),
        PLUME_ENUM_VALUE(RenderDescriptorRangeType, TEXTURE),
        PLUME_ENUM_VALUE(RenderDescriptorRangeType, READ_WRITE_TEXTURE),
        PLUME_ENUM_VALUE(RenderDescriptorRangeType, SAMPLER),
        PLUME_ENUM_VALUE(RenderDescriptorRangeType, STRUCTURED_BUFFER),
        PLUME_ENUM_VALUE(RenderDescriptorRangeType, READ_WRITE_STRUCTURED_BUFFER),
        PLUME_ENUM_VALUE(RenderDescriptorRangeType, BYTE_ADDRESS_BUFFER),
        PLUME_ENUM_VALUE(RenderDescriptorRangeType, READ_WRITE_BYTE_ADDRESS_BUFFER),
        PLUME_ENUM_VALUE(RenderDescriptorRangeType, ACCELERATION_STRUCTURE)
    PLUME_ENUM_END(RenderDescriptorRangeType)

    PLUME_ENUM_BEGIN(RenderRootDescriptorType)
        PLUME_ENUM_VALUE(RenderRootDescriptorType, UNKNOWN),
        PLUME_ENUM_VALUE(RenderRootDescriptorType, CONSTANT_BUFFER),
        PLUME_ENUM_VALUE(RenderRootDescriptorType, SHADER_RESOURCE),
        PLUME_ENUM_VALUE(RenderRootDescriptorType, UNORDERED_ACCESS)
    PLUME_ENUM_END(RenderRootDescriptorType)

    PLUME_ENUM_BEGIN(RenderHeapType)
        PLUME_ENUM_VALUE(RenderHeapType, UNKNOWN),
        PLUME_ENUM_VALUE(RenderHeapType, DEFAULT),
        PLUME_ENUM_VALUE(RenderHeapType, UPLOAD),
        PLUME_ENUM_VALUE(RenderHeapType, READBACK),
        PLUME_ENUM_VALUE(RenderHeapType, GPU_UPLOAD)
    PLUME_ENUM_END(RenderHeapType)

    PLUME_ENUM_BEGIN(RenderTextureArrangement)
        PLUME_ENUM_VALUE(RenderTextureArrangement, UNKNOWN),
        PLUME_ENUM_VALUE(RenderTextureArrangement, ROW_MAJOR)
    PLUME_ENUM_END(RenderTextureArrangement)

    PLUME_ENUM_BEGIN(RenderShaderFormat)
        PLUME_ENUM_VALUE(RenderShaderFormat, UNKNOWN),
        PLUME_ENUM_VALUE(RenderShaderFormat, DXIL),
        PLUME_ENUM_VALUE(RenderShaderFormat, SPIRV),
        PLUME_ENUM_VALUE(RenderShaderFormat, METAL)
    PLUME_ENUM_END(RenderShaderFormat)

    PLUME_ENUM_BEGIN(RenderRaytracingPipelineLibrarySymbolType)
        PLUME_ENUM_VALUE(RenderRaytracingPipelineLibrarySymbolType, UNKNOWN),
        PLUME_ENUM_VALUE(RenderRaytracingPipelineLibrarySymbolType, RAYGEN),
        PLUME_ENUM_VALUE(RenderRaytracingPipelineLibrarySymbolType, MISS),
        PLUME_ENUM_VALUE(RenderRaytracingPipelineLibrarySymbolType, CLOSEST_HIT),
        PLUME_ENUM_VALUE(RenderRaytracingPipelineLibrarySymbolType, ANY_HIT),
        PLUME_ENUM_VALUE(RenderRaytracingPipelineLibrarySymbolType, INTERSECTION),
        PLUME_ENUM_VALUE(RenderRaytracingPipelineLibrarySymbolType, CALLABLE)
    PLUME_ENUM_END(RenderRaytracingPipelineLibrarySymbolType)

    PLUME_ENUM_BEGIN(RenderAccelerationStructureType)
        PLUME_ENUM_VALUE(RenderAccelerationStructureType, UNKNOWN),
        PLUME_ENUM_VALUE(RenderAccelerationStructureType, TOP_LEVEL),
        PLUME_ENUM_VALUE(RenderAccelerationStructureType, BOTTOM_LEVEL)
    PLUME_ENUM_END(RenderAccelerationStructureType)

    PLUME_ENUM_BEGIN(RenderTextureLayout)
        PLUME_ENUM_VALUE(RenderTextureLayout, UNKNOWN),
        PLUME_ENUM_VALUE(RenderTextureLayout, GENERAL),
        PLUME_ENUM_VALUE(RenderTextureLayout, SHADER_READ),
        PLUME_ENUM_VALUE(RenderTextureLayout, COLOR_WRITE),
        PLUME_ENUM_VALUE(RenderTextureLayout, DEPTH_WRITE),
        PLUME_ENUM_VALUE(RenderTextureLayout, DEPTH_READ),
        PLUME_ENUM_VALUE(RenderTextureLayout, COPY_SOURCE),
        PLUME_ENUM_VALUE(RenderTextureLayout, COPY_DEST),
        PLUME_ENUM_VALUE(RenderTextureLayout, RESOLVE_SOURCE),
        PLUME_ENUM_VALUE(RenderTextureLayout, RESOLVE_DEST),
        PLUME_ENUM_VALUE(RenderTextureLayout, PRESENT)
    PLUME_ENUM_END(RenderTextureLayout)

    PLUME_ENUM_BEGIN(RenderDeviceType)
        PLUME_ENUM_VALUE(RenderDeviceType, UNKNOWN),
        PLUME_ENUM_VALUE(RenderDeviceType, INTEGRATED),
        PLUME_ENUM_VALUE(RenderDeviceType, DISCRETE),
        PLUME_ENUM_VALUE(RenderDeviceType, VIRTUAL),
        PLUME_ENUM_VALUE(RenderDeviceType, CPU)
    PLUME_ENUM_END(RenderDeviceType)

    PLUME_ENUM_BEGIN(RenderResolveMode)
        PLUME_ENUM_VALUE(RenderResolveMode, MIN),
        PLUME_ENUM_VALUE(RenderResolveMode, MAX),
        PLUME_ENUM_VALUE(RenderResolveMode, AVERAGE)
    PLUME_ENUM_END(RenderResolveMode)

    PLUME_ENUM_BEGIN(RenderTextureCopyType)
        PLUME_ENUM_VALUE(RenderTextureCopyType, UNKNOWN),
        PLUME_ENUM_VALUE(RenderTextureCopyType, SUBRESOURCE),
        PLUME_ENUM_VALUE(RenderTextureCopyType, PLACED_FOOTPRINT)
    PLUME_ENUM_END(RenderTextureCopyType)

    #ifdef __cplusplus
    enum class RenderColorWriteEnable : uint8_t {
        UNKNOWN = 0x0,
        RED = 0x1,
        GREEN = 0x2,
        BLUE = 0x4,
        ALPHA = 0x8,
        ALL = RED | GREEN | BLUE | ALPHA
    };
    #else
    typedef enum {
        RenderColorWriteEnable_UNKNOWN = 0x0,
        RenderColorWriteEnable_RED = 0x1,
        RenderColorWriteEnable_GREEN = 0x2,
        RenderColorWriteEnable_BLUE = 0x4,
        RenderColorWriteEnable_ALPHA = 0x8,
        RenderColorWriteEnable_ALL = RenderColorWriteEnable_RED | RenderColorWriteEnable_GREEN | RenderColorWriteEnable_BLUE | RenderColorWriteEnable_ALPHA
    } RenderColorWriteEnable;
    #endif

    #ifdef __cplusplus
    enum class RenderSwizzle : uint8_t
    {
        IDENTITY = 0,
        ZERO = 1,
        ONE = 2,
        R = 3,
        G = 4,
        B = 5,
        A = 6
    };
    #else
    typedef enum {
        RenderSwizzle_IDENTITY = 0,
        RenderSwizzle_ZERO = 1,
        RenderSwizzle_ONE = 2,
        RenderSwizzle_R = 3,
        RenderSwizzle_G = 4,
        RenderSwizzle_B = 5,
        RenderSwizzle_A = 6
    } RenderSwizzle;
    #endif

    #ifdef __cplusplus
    namespace RenderShaderStageFlag {
        enum Bits : uint32_t {
            NONE = 0U,
            VERTEX = 1U << 0,
            GEOMETRY = 1U << 1,
            PIXEL = 1U << 2,
            COMPUTE = 1U << 3,
            RAYGEN = 1U << 4,
            ANY_HIT = 1U << 5,
            CLOSEST_HIT = 1U << 6,
            MISS = 1U << 7,
            INTERSECTION = 1U << 8,
            CALLABLE = 1U << 9
        };
    };
    #else
    enum RenderShaderStageFlag {
        RenderShaderStageFlag_NONE = 0U,
        RenderShaderStageFlag_VERTEX = 1U << 0,
        RenderShaderStageFlag_GEOMETRY = 1U << 1,
        RenderShaderStageFlag_PIXEL = 1U << 2,
        RenderShaderStageFlag_COMPUTE = 1U << 3,
        RenderShaderStageFlag_RAYGEN = 1U << 4,
        RenderShaderStageFlag_ANY_HIT = 1U << 5,
        RenderShaderStageFlag_CLOSEST_HIT = 1U << 6,
        RenderShaderStageFlag_MISS = 1U << 7,
        RenderShaderStageFlag_INTERSECTION = 1U << 8,
        RenderShaderStageFlag_CALLABLE = 1U << 9
    };
    #endif

    typedef uint32_t RenderShaderStageFlags;

    #ifdef __cplusplus
    namespace RenderBufferFlag {
        enum Bits : uint32_t {
            NONE = 0U,
            VERTEX = 1U << 0,
            INDEX = 1U << 1,
            STORAGE = 1U << 2,
            CONSTANT = 1U << 3,
            FORMATTED = 1U << 4,
            ACCELERATION_STRUCTURE = 1U << 5,
            ACCELERATION_STRUCTURE_INPUT = 1U << 6,
            ACCELERATION_STRUCTURE_SCRATCH = 1U << 7,
            SHADER_BINDING_TABLE = 1U << 8,
            UNORDERED_ACCESS = 1U << 9,
            DEVICE_ADDRESSABLE = 1U << 10
        };
    };
    #else
    enum RenderBufferFlag {
        RenderBufferFlag_NONE = 0U,
        RenderBufferFlag_VERTEX = 1U << 0,
        RenderBufferFlag_INDEX = 1U << 1,
        RenderBufferFlag_STORAGE = 1U << 2,
        RenderBufferFlag_CONSTANT = 1U << 3,
        RenderBufferFlag_FORMATTED = 1U << 4,
        RenderBufferFlag_ACCELERATION_STRUCTURE = 1U << 5,
        RenderBufferFlag_ACCELERATION_STRUCTURE_INPUT = 1U << 6,
        RenderBufferFlag_ACCELERATION_STRUCTURE_SCRATCH = 1U << 7,
        RenderBufferFlag_SHADER_BINDING_TABLE = 1U << 8,
        RenderBufferFlag_UNORDERED_ACCESS = 1U << 9,
        RenderBufferFlag_DEVICE_ADDRESSABLE = 1U << 10
    };
    #endif
    typedef uint32_t RenderBufferFlags;

    #ifdef __cplusplus
    namespace RenderTextureFlag {
        enum Bits : uint32_t {
            NONE = 0U,
            RENDER_TARGET = 1U << 0,
            DEPTH_TARGET = 1U << 1,
            STORAGE = 1U << 2,
            UNORDERED_ACCESS = 1U << 3,
            CUBE = 1U << 4
        };
    };
    #else
    enum RenderTextureFlag {
        RenderTextureFlag_NONE = 0U,
        RenderTextureFlag_RENDER_TARGET = 1U << 0,
        RenderTextureFlag_DEPTH_TARGET = 1U << 1,
        RenderTextureFlag_STORAGE = 1U << 2,
        RenderTextureFlag_UNORDERED_ACCESS = 1U << 3,
        RenderTextureFlag_CUBE = 1U << 4
    };
    #endif

    typedef uint32_t RenderTextureFlags;

    #if __cplusplus
    namespace RenderBarrierStage {
        enum Bits : uint32_t {
            NONE = 0U,
            GRAPHICS = 1U << 0,
            COMPUTE = 1U << 1,
            COPY = 1U << 2,
            GRAPHICS_AND_COMPUTE = GRAPHICS | COMPUTE,
            ALL = GRAPHICS | COMPUTE | COPY
        };
    };
    #else
    enum RenderBarrierStage {
        RenderBarrierStage_NONE = 0U,
        RenderBarrierStage_GRAPHICS = 1U << 0,
        RenderBarrierStage_COMPUTE = 1U << 1,
        RenderBarrierStage_COPY = 1U << 2,
        RenderBarrierStage_GRAPHICS_AND_COMPUTE = RenderBarrierStage_GRAPHICS | RenderBarrierStage_COMPUTE,
        RenderBarrierStage_ALL = RenderBarrierStage_GRAPHICS | RenderBarrierStage_COMPUTE | RenderBarrierStage_COPY
    };
    #endif

    typedef uint32_t RenderBarrierStages;

    #ifdef __cplusplus
    namespace RenderBufferAccess {
        enum Bits : uint32_t {
            NONE = 0U,
            READ = 1U << 0,
            WRITE = 1U << 1
        };
    };
    #else
    enum RenderBufferAccess {
        RenderBufferAccess_NONE = 0U,
        RenderBufferAccess_READ = 1U << 0,
        RenderBufferAccess_WRITE = 1U << 1
    };
    #endif

    typedef uint32_t RenderBufferAccessBits;

    #ifdef __cplusplus
    namespace RenderSampleCount {
        enum Bits : uint32_t {
            COUNT_0 = 0x0,
            COUNT_1 = 0x1,
            COUNT_2 = 0x2,
            COUNT_4 = 0x4,
            COUNT_8 = 0x8,
            COUNT_16 = 0x10,
            COUNT_32 = 0x20,
            COUNT_64 = 0x40,
            COUNT_MAX = COUNT_64
        };
    };
    #else
    enum RenderSampleCount {
        RenderSampleCount_COUNT_0 = 0x0,
        RenderSampleCount_COUNT_1 = 0x1,
        RenderSampleCount_COUNT_2 = 0x2,
        RenderSampleCount_COUNT_4 = 0x4,
        RenderSampleCount_COUNT_8 = 0x8,
        RenderSampleCount_COUNT_16 = 0x10,
        RenderSampleCount_COUNT_32 = 0x20,
        RenderSampleCount_COUNT_64 = 0x40,
        RenderSampleCount_COUNT_MAX = RenderSampleCount_COUNT_64
    };
    #endif

    typedef uint32_t RenderSampleCounts;

    // Global functions.

#ifdef __cplusplus
    constexpr uint32_t RenderFormatSize(RenderFormat format) {
        switch (format) {
        case RenderFormat::R32G32B32A32_TYPELESS:
        case RenderFormat::R32G32B32A32_FLOAT:
        case RenderFormat::R32G32B32A32_UINT:
        case RenderFormat::R32G32B32A32_SINT:
            return 16;
        case RenderFormat::R32G32B32_TYPELESS:
        case RenderFormat::R32G32B32_FLOAT:
        case RenderFormat::R32G32B32_UINT:
        case RenderFormat::R32G32B32_SINT:
            return 12;
        case RenderFormat::R16G16B16A16_TYPELESS:
        case RenderFormat::R16G16B16A16_FLOAT:
        case RenderFormat::R16G16B16A16_UNORM:
        case RenderFormat::R16G16B16A16_UINT:
        case RenderFormat::R16G16B16A16_SNORM:
        case RenderFormat::R16G16B16A16_SINT:
        case RenderFormat::R32G32_TYPELESS:
        case RenderFormat::R32G32_FLOAT:
        case RenderFormat::R32G32_UINT:
        case RenderFormat::R32G32_SINT:
        case RenderFormat::D32_FLOAT_S8_UINT: // Has 24 unused bits
            return 8;
        case RenderFormat::R8G8B8A8_TYPELESS:
        case RenderFormat::R8G8B8A8_UNORM:
        case RenderFormat::R8G8B8A8_UINT:
        case RenderFormat::R8G8B8A8_SNORM:
        case RenderFormat::R8G8B8A8_SINT:
        case RenderFormat::B8G8R8A8_UNORM:
        case RenderFormat::R16G16_TYPELESS:
        case RenderFormat::R16G16_FLOAT:
        case RenderFormat::R16G16_UNORM:
        case RenderFormat::R16G16_UINT:
        case RenderFormat::R16G16_SNORM:
        case RenderFormat::R16G16_SINT:
        case RenderFormat::R32_TYPELESS:
        case RenderFormat::D32_FLOAT:
        case RenderFormat::R32_FLOAT:
        case RenderFormat::R32_UINT:
        case RenderFormat::R32_SINT:
            return 4;
        case RenderFormat::R8G8_TYPELESS:
        case RenderFormat::R8G8_UNORM:
        case RenderFormat::R8G8_UINT:
        case RenderFormat::R8G8_SNORM:
        case RenderFormat::R8G8_SINT:
        case RenderFormat::R16_TYPELESS:
        case RenderFormat::R16_FLOAT:
        case RenderFormat::D16_UNORM:
        case RenderFormat::R16_UNORM:
        case RenderFormat::R16_UINT:
        case RenderFormat::R16_SNORM:
        case RenderFormat::R16_SINT:
            return 2;
        case RenderFormat::R8_TYPELESS:
        case RenderFormat::R8_UNORM:
        case RenderFormat::R8_UINT:
        case RenderFormat::R8_SNORM:
        case RenderFormat::R8_SINT:
            return 1;
        case RenderFormat::BC1_UNORM:
        case RenderFormat::BC1_UNORM_SRGB:
        case RenderFormat::BC1_TYPELESS:
        case RenderFormat::BC4_UNORM:
        case RenderFormat::BC4_SNORM:
        case RenderFormat::BC4_TYPELESS:
            return 8;
        case RenderFormat::BC2_UNORM:
        case RenderFormat::BC2_UNORM_SRGB:
        case RenderFormat::BC2_TYPELESS:
        case RenderFormat::BC3_UNORM:
        case RenderFormat::BC3_UNORM_SRGB:
        case RenderFormat::BC3_TYPELESS:
        case RenderFormat::BC5_UNORM:
        case RenderFormat::BC5_SNORM:
        case RenderFormat::BC6H_UF16:
        case RenderFormat::BC6H_SF16:
        case RenderFormat::BC7_UNORM:
        case RenderFormat::BC7_UNORM_SRGB:
            return 16;
        default:
            assert(false && "Unknown format.");
            return 1;
        }
    }

    constexpr uint32_t RenderFormatBlockWidth(RenderFormat format) {
        switch (format) {
        case RenderFormat::R32G32B32A32_TYPELESS:
        case RenderFormat::R32G32B32A32_FLOAT:
        case RenderFormat::R32G32B32A32_UINT:
        case RenderFormat::R32G32B32A32_SINT:
        case RenderFormat::R32G32B32_TYPELESS:
        case RenderFormat::R32G32B32_FLOAT:
        case RenderFormat::R32G32B32_UINT:
        case RenderFormat::R32G32B32_SINT:
        case RenderFormat::R16G16B16A16_TYPELESS:
        case RenderFormat::R16G16B16A16_FLOAT:
        case RenderFormat::R16G16B16A16_UNORM:
        case RenderFormat::R16G16B16A16_UINT:
        case RenderFormat::R16G16B16A16_SNORM:
        case RenderFormat::R16G16B16A16_SINT:
        case RenderFormat::R32G32_TYPELESS:
        case RenderFormat::R32G32_FLOAT:
        case RenderFormat::R32G32_UINT:
        case RenderFormat::R32G32_SINT:
        case RenderFormat::R8G8B8A8_TYPELESS:
        case RenderFormat::R8G8B8A8_UNORM:
        case RenderFormat::R8G8B8A8_UINT:
        case RenderFormat::R8G8B8A8_SNORM:
        case RenderFormat::R8G8B8A8_SINT:
        case RenderFormat::B8G8R8A8_UNORM:
        case RenderFormat::R16G16_TYPELESS:
        case RenderFormat::R16G16_FLOAT:
        case RenderFormat::R16G16_UNORM:
        case RenderFormat::R16G16_UINT:
        case RenderFormat::R16G16_SNORM:
        case RenderFormat::R16G16_SINT:
        case RenderFormat::R32_TYPELESS:
        case RenderFormat::D32_FLOAT:
        case RenderFormat::D32_FLOAT_S8_UINT:
        case RenderFormat::R32_FLOAT:
        case RenderFormat::R32_UINT:
        case RenderFormat::R32_SINT:
        case RenderFormat::R8G8_TYPELESS:
        case RenderFormat::R8G8_UNORM:
        case RenderFormat::R8G8_UINT:
        case RenderFormat::R8G8_SNORM:
        case RenderFormat::R8G8_SINT:
        case RenderFormat::R16_TYPELESS:
        case RenderFormat::R16_FLOAT:
        case RenderFormat::D16_UNORM:
        case RenderFormat::R16_UNORM:
        case RenderFormat::R16_UINT:
        case RenderFormat::R16_SNORM:
        case RenderFormat::R16_SINT:
        case RenderFormat::R8_TYPELESS:
        case RenderFormat::R8_UNORM:
        case RenderFormat::R8_UINT:
        case RenderFormat::R8_SNORM:
        case RenderFormat::R8_SINT:
            return 1;
        case RenderFormat::BC1_TYPELESS:
        case RenderFormat::BC1_UNORM:
        case RenderFormat::BC1_UNORM_SRGB:
        case RenderFormat::BC2_TYPELESS:
        case RenderFormat::BC2_UNORM:
        case RenderFormat::BC2_UNORM_SRGB:
        case RenderFormat::BC3_TYPELESS:
        case RenderFormat::BC3_UNORM:
        case RenderFormat::BC3_UNORM_SRGB:
        case RenderFormat::BC4_TYPELESS:
        case RenderFormat::BC4_UNORM:
        case RenderFormat::BC4_SNORM:
        case RenderFormat::BC5_TYPELESS:
        case RenderFormat::BC5_UNORM:
        case RenderFormat::BC5_SNORM:
        case RenderFormat::BC6H_TYPELESS:
        case RenderFormat::BC6H_UF16:
        case RenderFormat::BC6H_SF16:
        case RenderFormat::BC7_TYPELESS:
        case RenderFormat::BC7_UNORM:
        case RenderFormat::BC7_UNORM_SRGB:
            return 4;
        default:
            assert(false && "Unknown format.");
            return 1;
        }
    };

    constexpr bool RenderFormatIsDepth(RenderFormat format) {
        switch (format) {
        case RenderFormat::D16_UNORM:
        case RenderFormat::D32_FLOAT:
        case RenderFormat::D32_FLOAT_S8_UINT:
            return true;
        default:
            return false;
        }
    }

    constexpr bool RenderFormatIsStencil(RenderFormat format) {
        return format == RenderFormat::D32_FLOAT_S8_UINT;
    }

    constexpr RenderTextureViewDimension RenderTextureDimensionToView(const RenderTextureDimension dimension) {
        switch (dimension) {
            case RenderTextureDimension::UNKNOWN:
                return RenderTextureViewDimension::UNKNOWN;
            case RenderTextureDimension::TEXTURE_1D:
                return RenderTextureViewDimension::TEXTURE_1D;
            case RenderTextureDimension::TEXTURE_2D:
                return RenderTextureViewDimension::TEXTURE_2D;
            case RenderTextureDimension::TEXTURE_3D:
                return RenderTextureViewDimension::TEXTURE_3D;
            default:
                assert(false && "Unknown texture dimension.");
                return RenderTextureViewDimension::UNKNOWN;
        }
    }
#endif

    // Concrete structs.

    struct RenderColor {
        union {
            struct {
                float rgba[4];
            };

            struct {
                float r;
                float g;
                float b;
                float a;
            };
        };

    #ifdef __cplusplus
        RenderColor() {
            r = 0.0f;
            g = 0.0f;
            b = 0.0f;
            a = 1.0f;
        }

        RenderColor(float r, float g, float b, float a = 1.0f) {
            this->r = r;
            this->g = g;
            this->b = b;
            this->a = a;
        }
    #endif
    };

    struct RenderAffineTransform {
        float m[3][4] PLUME_DEFAULT({});

    #ifdef __cplusplus
        RenderAffineTransform() {
            m[0][0] = 1.0f;
            m[1][1] = 1.0f;
            m[2][2] = 1.0f;
        }
    #endif
    };

    struct RenderDepth {
        float depth PLUME_DEFAULT(1.0f);

    #ifdef __cplusplus
        RenderDepth() = default;

        RenderDepth(float depth) {
            this->depth = depth;
        }
    #endif
    };

    struct RenderMultisamplingLocation {
        // Valid range is [-8, 7].
        int8_t x PLUME_DEFAULT(0);
        int8_t y PLUME_DEFAULT(0);

    #ifdef __cplusplus
        bool operator==(const RenderMultisamplingLocation& other) const {
            return x == other.x && y == other.y;
        }

        bool operator!=(const RenderMultisamplingLocation& other) const {
            return !(*this == other);
        }
    #endif
    };

    struct RenderMultisampling {
        RenderSampleCounts sampleCount PLUME_DEFAULT(RenderSampleCount::COUNT_1);
        RenderMultisamplingLocation sampleLocations[16] PLUME_DEFAULT({});
        bool sampleLocationsEnabled PLUME_DEFAULT(false);

    #ifdef __cplusplus
        RenderMultisampling() = default;

        RenderMultisampling(RenderSampleCounts sampleCount) {
            this->sampleCount = sampleCount;
        }
    #endif
    };

    struct RenderBufferReference {
        const RenderBuffer *ref PLUME_DEFAULT(nullptr);
        uint64_t offset PLUME_DEFAULT(0);

    #ifdef __cplusplus
        RenderBufferReference() = default;

        RenderBufferReference(const RenderBuffer *ref) {
            this->ref = ref;
            offset = 0;
        }

        RenderBufferReference(const RenderBuffer *ref, uint64_t offset) {
            this->ref = ref;
            this->offset = offset;
        }

        bool operator==(const RenderBufferReference& rhs) const {
            return ref == rhs.ref && offset == rhs.offset;
        }

        bool operator!=(const RenderBufferReference& rhs) const {
            return !(*this == rhs);
        }
    #endif
    };

    struct RenderBufferBarrier {
        RenderBuffer *buffer PLUME_DEFAULT(nullptr);
        RenderBufferAccessBits accessBits PLUME_DEFAULT(RenderBufferAccess::NONE);

    #ifdef __cplusplus
        RenderBufferBarrier() = default;

        RenderBufferBarrier(RenderBuffer *buffer, RenderBufferAccessBits accessBits) {
            this->buffer = buffer;
            this->accessBits = accessBits;
        }
    #endif
    };

    struct RenderBufferStructuredView {
        uint32_t structureByteStride PLUME_DEFAULT(0);
        uint32_t firstElement PLUME_DEFAULT(0);

    #ifdef __cplusplus
        RenderBufferStructuredView() = default;

        RenderBufferStructuredView(uint32_t structureByteStride, uint32_t firstElement = 0) {
            this->structureByteStride = structureByteStride;
            this->firstElement = firstElement;
        }
    #endif
    };

    struct RenderTextureBarrier {
        RenderTexture *texture PLUME_DEFAULT(nullptr);
        RenderTextureLayout layout PLUME_DEFAULT(RenderTextureLayout::UNKNOWN);

    #ifdef __cplusplus
        RenderTextureBarrier() = default;

        RenderTextureBarrier(RenderTexture *texture, RenderTextureLayout layout) {
            this->texture = texture;
            this->layout = layout;
        }
    #endif
    };

    struct RenderClearValue {
        RenderFormat format PLUME_DEFAULT(RenderFormat::UNKNOWN);
        union {
            RenderColor color;
            RenderDepth depth;
        };

    #ifdef __cplusplus
        RenderClearValue() : color{} {}

        static RenderClearValue Color(RenderColor color, RenderFormat format) {
            RenderClearValue clear = {};
            clear.format = format;
            clear.color = color;
            return clear;
        }

        static RenderClearValue Depth(RenderDepth depth, RenderFormat format) {
            RenderClearValue clear = {};
            clear.format = format;
            clear.depth = depth;
            return clear;
        }
    #endif
    };

    struct RenderBufferDesc {
        uint64_t size PLUME_DEFAULT(0);
        RenderHeapType heapType PLUME_DEFAULT(RenderHeapType::UNKNOWN);
        RenderBufferFlags flags PLUME_DEFAULT(RenderBufferFlag::NONE);
        bool committed PLUME_DEFAULT(false);

    #ifdef __cplusplus
        RenderBufferDesc() = default;

        static RenderBufferDesc DefaultBuffer(uint64_t size, RenderBufferFlags flags = RenderBufferFlag::NONE) {
            RenderBufferDesc desc;
            desc.size = size;
            desc.heapType = RenderHeapType::DEFAULT;
            desc.flags = flags;
            return desc;
        }

        static RenderBufferDesc UploadBuffer(uint64_t size, RenderBufferFlags flags = RenderBufferFlag::NONE) {
            RenderBufferDesc desc;
            desc.heapType = RenderHeapType::UPLOAD;
            desc.size = size;
            desc.flags = flags;
            return desc;
        }

        static RenderBufferDesc ReadbackBuffer(uint64_t size, RenderBufferFlags flags = RenderBufferFlag::NONE) {
            RenderBufferDesc desc;
            desc.heapType = RenderHeapType::READBACK;
            desc.size = size;
            desc.flags = flags;
            return desc;
        }

        static RenderBufferDesc VertexBuffer(uint64_t size, RenderHeapType heapType, RenderBufferFlags flags = RenderBufferFlag::NONE) {
            RenderBufferDesc desc;
            desc.size = size;
            desc.heapType = heapType;
            desc.flags = flags | RenderBufferFlag::VERTEX;
            return desc;
        }

        static RenderBufferDesc IndexBuffer(uint64_t size, RenderHeapType heapType, RenderBufferFlags flags = RenderBufferFlag::NONE) {
            RenderBufferDesc desc;
            desc.size = size;
            desc.heapType = heapType;
            desc.flags = flags | RenderBufferFlag::INDEX;
            return desc;
        }

        static RenderBufferDesc AccelerationStructureBuffer(uint64_t size) {
            RenderBufferDesc desc;
            desc.size = size;
            desc.heapType = RenderHeapType::DEFAULT;
            desc.flags = RenderBufferFlag::ACCELERATION_STRUCTURE;
            return desc;
        }
    #endif
    };

    struct RenderTextureDesc {
        RenderTextureDimension dimension PLUME_DEFAULT(RenderTextureDimension::UNKNOWN);
        uint32_t width PLUME_DEFAULT(0);
        uint32_t height PLUME_DEFAULT(0);
        uint32_t depth PLUME_DEFAULT(0);
        uint32_t mipLevels PLUME_DEFAULT(0);
        uint32_t arraySize PLUME_DEFAULT(0);
        RenderMultisampling multisampling;
        RenderFormat format PLUME_DEFAULT(RenderFormat::UNKNOWN);
        RenderTextureArrangement textureArrangement PLUME_DEFAULT(RenderTextureArrangement::UNKNOWN);
        const RenderClearValue *optimizedClearValue PLUME_DEFAULT(nullptr);
        RenderTextureFlags flags PLUME_DEFAULT(RenderTextureFlag::NONE);
        bool committed PLUME_DEFAULT(false);

    #ifdef __cplusplus
        RenderTextureDesc() = default;

        static RenderTextureDesc Texture(RenderTextureDimension dimension, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels, uint32_t arraySize, RenderFormat format, RenderTextureFlags flags = RenderTextureFlag::NONE) {
            RenderTextureDesc desc;
            desc.dimension = dimension;
            desc.width = width;
            desc.height = height;
            desc.depth = depth;
            desc.mipLevels = mipLevels;
            desc.arraySize = arraySize;
            desc.format = format;
            desc.flags = flags;
            return desc;
        }

        static RenderTextureDesc Texture1D(uint32_t width, uint32_t mipLevels, RenderFormat format, RenderTextureFlags flags = RenderTextureFlag::NONE) {
            return Texture(RenderTextureDimension::TEXTURE_1D, width, 1, 1, mipLevels, 1, format, flags);
        }

        static RenderTextureDesc Texture2D(uint32_t width, uint32_t height, uint32_t mipLevels, RenderFormat format, RenderTextureFlags flags = RenderTextureFlag::NONE) {
            return Texture(RenderTextureDimension::TEXTURE_2D, width, height, 1, mipLevels, 1, format, flags);
        }

        static RenderTextureDesc Texture3D(uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels, RenderFormat format, RenderTextureFlags flags = RenderTextureFlag::NONE) {
            return Texture(RenderTextureDimension::TEXTURE_3D, width, height, depth, mipLevels, 1, format, flags);
        }

        static RenderTextureDesc ColorTarget(uint32_t width, uint32_t height, RenderFormat format, RenderMultisampling multisampling = RenderMultisampling(), const RenderClearValue *optimizedClearValue = nullptr, RenderTextureFlags flags = RenderTextureFlag::NONE) {
            RenderTextureDesc desc;
            desc.committed = true;
            desc.dimension = RenderTextureDimension::TEXTURE_2D;
            desc.width = width;
            desc.height = height;
            desc.depth = 1;
            desc.mipLevels = 1;
            desc.arraySize = 1;
            desc.format = format;
            desc.multisampling = multisampling;
            desc.flags = flags | RenderTextureFlag::RENDER_TARGET;
            desc.optimizedClearValue = optimizedClearValue;
            return desc;
        }

        static RenderTextureDesc DepthTarget(uint32_t width, uint32_t height, RenderFormat format, RenderMultisampling multisampling = RenderMultisampling(), const RenderClearValue *optimizedClearValue = nullptr, RenderTextureFlags flags = RenderTextureFlag::NONE) {
            RenderTextureDesc desc;
            desc.committed = true;
            desc.dimension = RenderTextureDimension::TEXTURE_2D;
            desc.width = width;
            desc.height = height;
            desc.depth = 1;
            desc.mipLevels = 1;
            desc.arraySize = 1;
            desc.format = format;
            desc.multisampling = multisampling;
            desc.flags = flags | RenderTextureFlag::DEPTH_TARGET;
            desc.optimizedClearValue = optimizedClearValue;
            return desc;
        }
    #endif
    };

    struct RenderComponentMapping
    {
        RenderSwizzle r PLUME_DEFAULT(RenderSwizzle::IDENTITY);
        RenderSwizzle g PLUME_DEFAULT(RenderSwizzle::IDENTITY);
        RenderSwizzle b PLUME_DEFAULT(RenderSwizzle::IDENTITY);
        RenderSwizzle a PLUME_DEFAULT(RenderSwizzle::IDENTITY);

    #ifdef __cplusplus
        RenderComponentMapping() = default;
        RenderComponentMapping(RenderSwizzle r, RenderSwizzle g, RenderSwizzle b, RenderSwizzle a) : r(r), g(g), b(b), a(a) {}
    #endif
    };

    struct RenderTextureViewDesc {
        RenderFormat format PLUME_DEFAULT(RenderFormat::UNKNOWN);
        RenderTextureViewDimension dimension PLUME_DEFAULT(RenderTextureViewDimension::UNKNOWN);
        uint32_t mipLevels PLUME_DEFAULT(UINT32_MAX);
        uint32_t mipSlice PLUME_DEFAULT(0);
        uint32_t arraySize PLUME_DEFAULT(UINT32_MAX);
        uint32_t arrayIndex PLUME_DEFAULT(0);
        RenderComponentMapping componentMapping;

    #ifdef __cplusplus
        RenderTextureViewDesc() = default;

        static RenderTextureViewDesc Texture1D(RenderFormat format) {
            RenderTextureViewDesc viewDesc;
            viewDesc.format = format;
            viewDesc.dimension = RenderTextureViewDimension::TEXTURE_1D;
            return viewDesc;
        }

        static RenderTextureViewDesc Texture2D(RenderFormat format) {
            RenderTextureViewDesc viewDesc;
            viewDesc.format = format;
            viewDesc.dimension = RenderTextureViewDimension::TEXTURE_2D;
            return viewDesc;
        }

        static RenderTextureViewDesc Texture3D(RenderFormat format) {
            RenderTextureViewDesc viewDesc;
            viewDesc.format = format;
            viewDesc.dimension = RenderTextureViewDimension::TEXTURE_3D;
            return viewDesc;
        }

        static RenderTextureViewDesc TextureCube(RenderFormat format) {
            RenderTextureViewDesc viewDesc;
            viewDesc.format = format;
            viewDesc.dimension = RenderTextureViewDimension::TEXTURE_CUBE;
            return viewDesc;
        }
    #endif
    };

    struct RenderAccelerationStructureDesc {
        RenderAccelerationStructureType type PLUME_DEFAULT(RenderAccelerationStructureType::UNKNOWN);
        RenderBufferReference buffer;
        uint64_t size PLUME_DEFAULT(0);

    #ifdef __cplusplus
        RenderAccelerationStructureDesc() = default;

        RenderAccelerationStructureDesc(RenderAccelerationStructureType type, RenderBufferReference buffer, uint64_t size) {
            this->type = type;
            this->buffer = buffer;
            this->size = size;
        }
    #endif
    };

    struct RenderTextureCopyLocation {
        const RenderTexture *texture PLUME_DEFAULT(nullptr);
        const RenderBuffer *buffer PLUME_DEFAULT(nullptr);
        RenderTextureCopyType type PLUME_DEFAULT(RenderTextureCopyType::UNKNOWN);

        union {
            struct {
                RenderFormat format;
                uint32_t width;
                uint32_t height;
                uint32_t depth;
                uint32_t rowWidth;
                uint64_t offset;
            } placedFootprint;

            struct {
                uint32_t mipLevel;
                uint32_t arrayIndex;
            } subresource;
        };

    #ifdef __cplusplus
        static RenderTextureCopyLocation PlacedFootprint(const RenderBuffer *buffer, RenderFormat format, uint32_t width, uint32_t height, uint32_t depth, uint32_t rowWidth, uint64_t offset = 0) {
            RenderTextureCopyLocation loc;
            loc.buffer = buffer;
            loc.type = RenderTextureCopyType::PLACED_FOOTPRINT;
            loc.placedFootprint.format = format;
            loc.placedFootprint.width = width;
            loc.placedFootprint.height = height;
            loc.placedFootprint.depth = depth;
            loc.placedFootprint.rowWidth = rowWidth;
            loc.placedFootprint.offset = offset;
            return loc;
        }

        static RenderTextureCopyLocation Subresource(const RenderTexture *texture, uint32_t mipLevel = 0, uint32_t arrayIndex = 0) {
            RenderTextureCopyLocation loc;
            loc.texture = texture;
            loc.type = RenderTextureCopyType::SUBRESOURCE;
            loc.subresource.mipLevel = mipLevel;
            loc.subresource.arrayIndex = arrayIndex;
            return loc;
        }
    #endif
    };

    struct RenderPoolDesc {
        RenderHeapType heapType PLUME_DEFAULT(RenderHeapType::UNKNOWN);
        uint32_t minBlockCount PLUME_DEFAULT(0);
        uint32_t maxBlockCount PLUME_DEFAULT(0);
        bool useLinearAlgorithm PLUME_DEFAULT(false);
        bool allowOnlyBuffers PLUME_DEFAULT(false);
    };

    struct RenderSwapChainDesc {
        RenderWindow renderWindow PLUME_DEFAULT({});
        RenderFormat format PLUME_DEFAULT(RenderFormat::UNKNOWN);
        uint32_t textureCount PLUME_DEFAULT(0);

        // The capability for presentWait must be supported by the RenderDevice.
        bool enablePresentWait PLUME_DEFAULT(false);
        uint32_t maxFrameLatency PLUME_DEFAULT(0);

    #ifdef __cplusplus
        RenderSwapChainDesc() = default;

        RenderSwapChainDesc(RenderWindow renderWindow, RenderFormat format, uint32_t textureCount, bool enablePresentWait = false, uint32_t maxFrameLatency = 0) {
            this->renderWindow = renderWindow;
            this->format = format;
            this->textureCount = textureCount;
            this->enablePresentWait = enablePresentWait;
            this->maxFrameLatency = maxFrameLatency;
        }
    #endif
    };

    struct RenderInputSlot {
        uint32_t index PLUME_DEFAULT(0);
        uint32_t stride PLUME_DEFAULT(0);
        RenderInputSlotClassification classification PLUME_DEFAULT(RenderInputSlotClassification::UNKNOWN);

    #ifdef __cplusplus
        RenderInputSlot() = default;

        RenderInputSlot(uint32_t index, uint32_t stride, RenderInputSlotClassification classification = RenderInputSlotClassification::PER_VERTEX_DATA) {
            this->index = index;
            this->stride = stride;
            this->classification = classification;
        }
    #endif
    };

    struct RenderInputElement {
        // Semantic name and index and location must be specified for both backends, but each attribute will only be read by the backend that uses them.
        const char *semanticName PLUME_DEFAULT(nullptr);
        uint32_t semanticIndex PLUME_DEFAULT(0);
        uint32_t location PLUME_DEFAULT(0);
        RenderFormat format PLUME_DEFAULT(RenderFormat::UNKNOWN);
        uint32_t slotIndex PLUME_DEFAULT(0);
        uint32_t alignedByteOffset PLUME_DEFAULT(0);

    #ifdef __cplusplus
        RenderInputElement() = default;

        RenderInputElement(const char *semanticName, uint32_t semanticIndex, uint32_t location, RenderFormat format, uint32_t slotIndex, uint32_t alignedByteOffset) {
            this->semanticName = semanticName;
            this->semanticIndex = semanticIndex;
            this->location = location;
            this->format = format;
            this->slotIndex = slotIndex;
            this->alignedByteOffset = alignedByteOffset;
        }
    #endif
    };

    struct RenderBlendDesc {
        bool blendEnabled PLUME_DEFAULT(false);
        RenderBlend srcBlend PLUME_DEFAULT(RenderBlend::UNKNOWN);
        RenderBlend dstBlend PLUME_DEFAULT(RenderBlend::UNKNOWN);
        RenderBlendOperation blendOp PLUME_DEFAULT(RenderBlendOperation::UNKNOWN);
        RenderBlend srcBlendAlpha PLUME_DEFAULT(RenderBlend::UNKNOWN);
        RenderBlend dstBlendAlpha PLUME_DEFAULT(RenderBlend::UNKNOWN);
        RenderBlendOperation blendOpAlpha PLUME_DEFAULT(RenderBlendOperation::UNKNOWN);
        uint8_t renderTargetWriteMask PLUME_DEFAULT(uint8_t(RenderColorWriteEnable::ALL));

    #ifdef __cplusplus
        static RenderBlendDesc Copy() {
            RenderBlendDesc desc;
            desc.srcBlend = RenderBlend::ONE;
            desc.dstBlend = RenderBlend::ZERO;
            desc.blendOp = RenderBlendOperation::ADD;
            desc.srcBlendAlpha = RenderBlend::ONE;
            desc.dstBlendAlpha = RenderBlend::ZERO;
            desc.blendOpAlpha = RenderBlendOperation::ADD;
            return desc;
        }

        static RenderBlendDesc AlphaBlend() {
            RenderBlendDesc desc;
            desc.blendEnabled = true;
            desc.srcBlend = RenderBlend::SRC_ALPHA;
            desc.dstBlend = RenderBlend::INV_SRC_ALPHA;
            desc.blendOp = RenderBlendOperation::ADD;
            desc.srcBlendAlpha = RenderBlend::ONE;
            desc.dstBlendAlpha = RenderBlend::INV_SRC_ALPHA;
            desc.blendOpAlpha = RenderBlendOperation::ADD;
            return desc;
        }
    #endif
    };

    struct RenderStencilFaceDesc {
        RenderStencilOp passOp PLUME_DEFAULT(RenderStencilOp::KEEP);
        RenderStencilOp failOp PLUME_DEFAULT(RenderStencilOp::KEEP);
        RenderStencilOp depthFailOp PLUME_DEFAULT(RenderStencilOp::KEEP);
        RenderComparisonFunction compareFunction PLUME_DEFAULT(RenderComparisonFunction::ALWAYS);
    };

    struct RenderSpecConstant {
        uint32_t index PLUME_DEFAULT(0);
        uint32_t value PLUME_DEFAULT(0);

    #ifdef __cplusplus
        RenderSpecConstant() = default;

        RenderSpecConstant(uint32_t index, uint32_t value) {
            this->index = index;
            this->value = value;
        }
    #endif
    };

    struct RenderComputePipelineDesc {
        const RenderPipelineLayout *pipelineLayout PLUME_DEFAULT(nullptr);
        const RenderShader *computeShader PLUME_DEFAULT(nullptr);
        const RenderSpecConstant *specConstants PLUME_DEFAULT(nullptr);
        uint32_t specConstantsCount PLUME_DEFAULT(0);
        uint32_t threadGroupSizeX PLUME_DEFAULT(0);
        uint32_t threadGroupSizeY PLUME_DEFAULT(0);
        uint32_t threadGroupSizeZ PLUME_DEFAULT(0);

    #ifdef __cplusplus
        RenderComputePipelineDesc() = default;

        RenderComputePipelineDesc(const RenderPipelineLayout *pipelineLayout, const RenderShader *computeShader, uint32_t threadGroupSizeX, uint32_t threadGroupSizeY, uint32_t threadGroupSizeZ) {
            this->pipelineLayout = pipelineLayout;
            this->computeShader = computeShader;
            this->threadGroupSizeX = threadGroupSizeX;
            this->threadGroupSizeY = threadGroupSizeY;
            this->threadGroupSizeZ = threadGroupSizeZ;
        }
    #endif
    };

    #define MAX_RENDER_TARGETS 8

    struct RenderGraphicsPipelineDesc {
        const RenderPipelineLayout *pipelineLayout PLUME_DEFAULT(nullptr);
        const RenderShader *vertexShader PLUME_DEFAULT(nullptr);
        const RenderShader *geometryShader PLUME_DEFAULT(nullptr);
        const RenderShader *pixelShader PLUME_DEFAULT(nullptr);
        RenderComparisonFunction depthFunction PLUME_DEFAULT(RenderComparisonFunction::NEVER);
        bool depthClipEnabled PLUME_DEFAULT(false);
        int32_t depthBias PLUME_DEFAULT(0);
        float depthBiasClamp PLUME_DEFAULT(0.0f);
        float slopeScaledDepthBias PLUME_DEFAULT(0.0f);
        bool dynamicDepthBiasEnabled PLUME_DEFAULT(false);
        bool depthEnabled PLUME_DEFAULT(false);
        bool depthWriteEnabled PLUME_DEFAULT(false);
        bool stencilEnabled PLUME_DEFAULT(false);
        uint32_t stencilReadMask PLUME_DEFAULT(0xFFFFFFFF);
        uint32_t stencilWriteMask PLUME_DEFAULT(0xFFFFFFFF);
        uint32_t stencilReference PLUME_DEFAULT(0);
        RenderStencilFaceDesc stencilFrontFace;
        RenderStencilFaceDesc stencilBackFace;
        RenderMultisampling multisampling;
        bool alphaToCoverageEnabled PLUME_DEFAULT(false);
        RenderPrimitiveTopology primitiveTopology PLUME_DEFAULT(RenderPrimitiveTopology::TRIANGLE_LIST);
        RenderCullMode cullMode PLUME_DEFAULT(RenderCullMode::NONE);
        RenderFrontFace frontFace PLUME_DEFAULT(RenderFrontFace::CLOCKWISE);
        RenderFormat renderTargetFormat[MAX_RENDER_TARGETS] PLUME_DEFAULT({});
        RenderBlendDesc renderTargetBlend[MAX_RENDER_TARGETS] PLUME_DEFAULT({});
        uint32_t renderTargetCount PLUME_DEFAULT(0);
        bool logicOpEnabled PLUME_DEFAULT(false);
        RenderLogicOperation logicOp PLUME_DEFAULT(RenderLogicOperation::NOOP);
        RenderFormat depthTargetFormat PLUME_DEFAULT(RenderFormat::UNKNOWN);
        const RenderInputSlot *inputSlots PLUME_DEFAULT(nullptr);
        uint32_t inputSlotsCount PLUME_DEFAULT(0);
        const RenderInputElement *inputElements PLUME_DEFAULT(nullptr);
        uint32_t inputElementsCount PLUME_DEFAULT(0);
        const RenderSpecConstant *specConstants PLUME_DEFAULT(nullptr);
        uint32_t specConstantsCount PLUME_DEFAULT(0);
    };

    struct RenderRaytracingPipelineLibrarySymbol {
        const char *importName PLUME_DEFAULT(nullptr);
        RenderRaytracingPipelineLibrarySymbolType type PLUME_DEFAULT(RenderRaytracingPipelineLibrarySymbolType::UNKNOWN);
        const char *exportName PLUME_DEFAULT(nullptr);
        const RenderSpecConstant *specConstants PLUME_DEFAULT(nullptr);
        uint32_t specConstantsCount PLUME_DEFAULT(0);

    #ifdef __cplusplus
        RenderRaytracingPipelineLibrarySymbol() = default;

        RenderRaytracingPipelineLibrarySymbol(const char *importName, RenderRaytracingPipelineLibrarySymbolType type, const char *exportName = nullptr, const RenderSpecConstant *specConstants = nullptr, uint32_t specConstantsCount = 0) {
            this->importName = importName;
            this->type = type;
            this->specConstants = specConstants;
            this->exportName = exportName;
            this->specConstantsCount = specConstantsCount;
        }
    #endif
    };

    struct RenderRaytracingPipelineLibrary {
        const RenderShader *shader PLUME_DEFAULT(nullptr);
        const RenderRaytracingPipelineLibrarySymbol *symbols PLUME_DEFAULT(nullptr);
        uint32_t symbolsCount PLUME_DEFAULT(0);

    #ifdef __cplusplus
        RenderRaytracingPipelineLibrary() = default;

        RenderRaytracingPipelineLibrary(const RenderShader *shader, const RenderRaytracingPipelineLibrarySymbol *symbols, uint32_t symbolsCount) {
            this->shader = shader;
            this->symbols = symbols;
            this->symbolsCount = symbolsCount;
        }
    #endif
    };

    struct RenderRaytracingPipelineHitGroup {
        const char *hitGroupName PLUME_DEFAULT(nullptr);
        const char *closestHitName PLUME_DEFAULT(nullptr);
        const char *anyHitName PLUME_DEFAULT(nullptr);
        const char *intersectionName PLUME_DEFAULT(nullptr);

    #ifdef __cplusplus
        RenderRaytracingPipelineHitGroup() = default;

        RenderRaytracingPipelineHitGroup(const char *hitGroupName, const char *closestHitName = nullptr, const char *anyHitName = nullptr, const char *intersectionName = nullptr) {
            this->hitGroupName = hitGroupName;
            this->closestHitName = closestHitName;
            this->anyHitName = anyHitName;
            this->intersectionName = intersectionName;
        }
    #endif
    };

    struct RenderRaytracingPipelineDesc {
        const RenderRaytracingPipelineLibrary *libraries PLUME_DEFAULT(nullptr);
        uint32_t librariesCount PLUME_DEFAULT(0);
        const RenderRaytracingPipelineHitGroup *hitGroups PLUME_DEFAULT(nullptr);
        uint32_t hitGroupsCount PLUME_DEFAULT(0);
        const RenderPipelineLayout *pipelineLayout PLUME_DEFAULT(nullptr);
        uint32_t maxPayloadSize PLUME_DEFAULT(0);
        uint32_t maxAttributeSize PLUME_DEFAULT(2 * sizeof(float));
        uint32_t maxRecursionDepth PLUME_DEFAULT(1);

        // IMPORTANT: State update support must be true for this option to work. The pipeline creation will not work if this option
        // is enabled and the device doesn't support it. This option is only supported by Raytracing Tier 1.1 devices.
        bool stateUpdateEnabled PLUME_DEFAULT(false);
    };

    struct RenderPipelineProgram {
        uint32_t programIndex PLUME_DEFAULT(0);

    #ifdef __cplusplus
        RenderPipelineProgram() = default;

        RenderPipelineProgram(uint32_t programIndex) {
            this->programIndex = programIndex;
        }
    #endif
    };

    struct RenderSamplerDesc {
        RenderFilter minFilter PLUME_DEFAULT(RenderFilter::LINEAR);
        RenderFilter magFilter PLUME_DEFAULT(RenderFilter::LINEAR);
        RenderMipmapMode mipmapMode PLUME_DEFAULT(RenderMipmapMode::LINEAR);
        RenderTextureAddressMode addressU PLUME_DEFAULT(RenderTextureAddressMode::WRAP);
        RenderTextureAddressMode addressV PLUME_DEFAULT(RenderTextureAddressMode::WRAP);
        RenderTextureAddressMode addressW PLUME_DEFAULT(RenderTextureAddressMode::WRAP);
        float mipLODBias PLUME_DEFAULT(0.0f);
        uint32_t maxAnisotropy PLUME_DEFAULT(16);
        bool anisotropyEnabled PLUME_DEFAULT(false);
        RenderComparisonFunction comparisonFunc PLUME_DEFAULT(RenderComparisonFunction::NEVER);
        bool comparisonEnabled PLUME_DEFAULT(false);
        RenderBorderColor borderColor PLUME_DEFAULT(RenderBorderColor::OPAQUE_BLACK);
        float minLOD PLUME_DEFAULT(0.0f);
        float maxLOD PLUME_DEFAULT(FLT_MAX);
        RenderShaderVisibility shaderVisibility PLUME_DEFAULT(RenderShaderVisibility::ALL);

    #ifdef __cplusplus
        RenderSamplerDesc() = default;
    #endif
    };

    struct RenderDescriptorRange {
        // The type of descriptor range. The descriptor can't change this during its lifetime.
        RenderDescriptorRangeType type PLUME_DEFAULT(RenderDescriptorRangeType::UNKNOWN);

        // How many descriptors should be assigned and allocated for this range. When the range
        // is boundless (see RenderDescriptorSetDesc::lastRangeIsBoundless), this indicates the upper
        // bound of the variable sized array (TBD if this implies additional memory consumption).
        uint32_t count PLUME_DEFAULT(0);

        // The shader binding number the descriptor will correspond to.
        uint32_t binding PLUME_DEFAULT(0);

        // An optional immutable sampler to build in statically into the pipeline layout.
        const RenderSampler **immutableSampler PLUME_DEFAULT(nullptr);

    #ifdef __cplusplus
        RenderDescriptorRange() = default;

        RenderDescriptorRange(RenderDescriptorRangeType type, uint32_t binding, uint32_t count, const RenderSampler **immutableSampler = nullptr) {
            this->type = type;
            this->binding = binding;
            this->count = count;
            this->immutableSampler = immutableSampler;
        }
    #endif
    };

    struct RenderDescriptorSetDesc {
        const RenderDescriptorRange *descriptorRanges PLUME_DEFAULT(nullptr);
        uint32_t descriptorRangesCount PLUME_DEFAULT(0);
        bool lastRangeIsBoundless PLUME_DEFAULT(false);
        uint32_t boundlessRangeSize PLUME_DEFAULT(0);

    #ifdef __cplusplus
        RenderDescriptorSetDesc() = default;

        RenderDescriptorSetDesc(const RenderDescriptorRange *descriptorRanges, uint32_t descriptorRangesCount, bool lastRangeIsBoundless = false, uint32_t boundlessRangeSize = 0) {
            this->descriptorRanges = descriptorRanges;
            this->descriptorRangesCount = descriptorRangesCount;
            this->lastRangeIsBoundless = lastRangeIsBoundless;
            this->boundlessRangeSize = boundlessRangeSize;
        }
    #endif
    };

    struct RenderPushConstantRange {
        uint32_t binding PLUME_DEFAULT(0);
        uint32_t set PLUME_DEFAULT(0);
        uint32_t offset PLUME_DEFAULT(0); // Must be aligned to 4-bytes for DX12.
        uint32_t size PLUME_DEFAULT(0);
        RenderShaderStageFlags stageFlags PLUME_DEFAULT(RenderShaderStageFlag::NONE);

    #ifdef __cplusplus
        RenderPushConstantRange() = default;

        RenderPushConstantRange(uint32_t binding, uint32_t set, uint32_t offset, uint32_t size, RenderShaderStageFlags stageFlags) {
            this->binding = binding;
            this->set = set;
            this->offset = offset;
            this->size = size;
            this->stageFlags = stageFlags;
        }
    #endif
    };

    // D3D12 only.
    struct RenderRootDescriptorDesc {
        uint32_t shaderRegister PLUME_DEFAULT(0);
        uint32_t registerSpace PLUME_DEFAULT(0);
        RenderRootDescriptorType type PLUME_DEFAULT(RenderRootDescriptorType::UNKNOWN);

    #ifdef __cplusplus
        RenderRootDescriptorDesc() = default;

        RenderRootDescriptorDesc(uint32_t shaderRegister, uint32_t registerSpace, RenderRootDescriptorType type) {
            this->shaderRegister = shaderRegister;
            this->registerSpace = registerSpace;
            this->type = type;
        }
    #endif
    };

    struct RenderPipelineLayoutDesc {
        const RenderPushConstantRange *pushConstantRanges PLUME_DEFAULT(nullptr);
        uint32_t pushConstantRangesCount PLUME_DEFAULT(0);
        const RenderDescriptorSetDesc *descriptorSetDescs PLUME_DEFAULT(nullptr);
        uint32_t descriptorSetDescsCount PLUME_DEFAULT(0);
        const RenderRootDescriptorDesc* rootDescriptorDescs PLUME_DEFAULT(nullptr);
        uint32_t rootDescriptorDescsCount PLUME_DEFAULT(0);
        bool isLocal PLUME_DEFAULT(false);
        bool allowInputLayout PLUME_DEFAULT(false);

    #ifdef __cplusplus
        RenderPipelineLayoutDesc() = default;

        RenderPipelineLayoutDesc(const RenderPushConstantRange *pushConstantRanges, uint32_t pushConstantRangesCount, const RenderDescriptorSetDesc *descriptorSetDescs, uint32_t descriptorSetDescsCount, bool isLocal = false, bool allowInputLayout = false) {
            this->pushConstantRanges = pushConstantRanges;
            this->pushConstantRangesCount = pushConstantRangesCount;
            this->descriptorSetDescs = descriptorSetDescs;
            this->descriptorSetDescsCount = descriptorSetDescsCount;
            this->isLocal = isLocal;
            this->allowInputLayout = allowInputLayout;
        }
    #endif
    };

    struct RenderIndexBufferView {
        RenderBufferReference buffer;
        uint32_t size PLUME_DEFAULT(0);
        RenderFormat format PLUME_DEFAULT(RenderFormat::UNKNOWN);

    #ifdef __cplusplus
        RenderIndexBufferView() = default;

        RenderIndexBufferView(RenderBufferReference buffer, uint32_t size, RenderFormat format) {
            this->buffer = buffer;
            this->size = size;
            this->format = format;
        }
    #endif
    };

    struct RenderVertexBufferView {
        RenderBufferReference buffer;
        uint32_t size PLUME_DEFAULT(0);

    #ifdef __cplusplus
        RenderVertexBufferView() = default;

        RenderVertexBufferView(RenderBufferReference buffer, uint32_t size) {
            this->buffer = buffer;
            this->size = size;
        }
    #endif
    };

    struct RenderViewport {
        float x PLUME_DEFAULT(0.0f);
        float y PLUME_DEFAULT(0.0f);
        float width PLUME_DEFAULT(0.0f);
        float height PLUME_DEFAULT(0.0f);
        float minDepth PLUME_DEFAULT(0.0f);
        float maxDepth PLUME_DEFAULT(1.0f);

    #ifdef __cplusplus
        RenderViewport() = default;

        RenderViewport(float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f) {
            this->x = x;
            this->y = y;
            this->width = width;
            this->height = height;
            this->minDepth = minDepth;
            this->maxDepth = maxDepth;
        }

        bool operator==(const RenderViewport &v) const {
            return (x == v.x) && (y == v.y) && (width == v.width) && (height == v.height) && (minDepth == v.minDepth) && (maxDepth == v.maxDepth);
        }

        bool operator!=(const RenderViewport &v) const {
            return (x != v.x) || (y != v.y) || (width != v.width) || (height != v.height) || (minDepth != v.minDepth) || (maxDepth != v.maxDepth);
        }

        bool isEmpty() const {
            return (width <= 0.0f) || (height <= 0.0f);
        }
    #endif
    };

    struct RenderRect {
        int32_t left PLUME_DEFAULT(0);
        int32_t top PLUME_DEFAULT(0);
        int32_t right PLUME_DEFAULT(0);
        int32_t bottom PLUME_DEFAULT(0);

    #ifdef __cplusplus
        RenderRect() = default;

        RenderRect(int32_t left, int32_t top, int32_t right, int32_t bottom) {
            this->left = left;
            this->top = top;
            this->right = right;
            this->bottom = bottom;
        }

        bool operator==(const RenderRect &v) const {
            return (left == v.left) && (top == v.top) && (right == v.right) && (bottom == v.bottom);
        }

        bool operator!=(const RenderRect &v) const {
            return (left != v.left) || (top != v.top) || (right != v.right) || (bottom != v.bottom);
        }

        bool isEmpty() const {
            return (left >= right) || (top >= bottom);
        }
    #endif
    };

    struct RenderBox {
        int32_t left PLUME_DEFAULT(0);
        int32_t top PLUME_DEFAULT(0);
        int32_t front PLUME_DEFAULT(0);
        int32_t right PLUME_DEFAULT(0);
        int32_t bottom PLUME_DEFAULT(0);
        int32_t back PLUME_DEFAULT(0);

    #ifdef __cplusplus
        RenderBox() = default;

        RenderBox(int32_t left, int32_t top, int32_t right, int32_t bottom, int32_t front = 0, int32_t back = 1) {
            this->left = left;
            this->top = top;
            this->front = front;
            this->right = right;
            this->bottom = bottom;
            this->back = back;
        }
    #endif
    };

    struct RenderRange {
        uint64_t begin PLUME_DEFAULT(0);
        uint64_t end PLUME_DEFAULT(0);

    #ifdef __cplusplus
        RenderRange() = default;

        RenderRange(uint64_t begin, uint64_t end) {
            this->begin = begin;
            this->end = end;
        }
    #endif
    };

    struct RenderFramebufferDesc {
        const RenderTexture **colorAttachments PLUME_DEFAULT(nullptr);
        const RenderTextureView **colorAttachmentViews PLUME_DEFAULT(nullptr);
        uint32_t colorAttachmentsCount PLUME_DEFAULT(0);
        const RenderTexture *depthAttachment PLUME_DEFAULT(nullptr);
        const RenderTextureView *depthAttachmentView PLUME_DEFAULT(nullptr);
        bool depthAttachmentReadOnly PLUME_DEFAULT(false);

    #ifdef __cplusplus
        RenderFramebufferDesc() = default;

        RenderFramebufferDesc(const RenderTexture **colorAttachments, uint32_t colorAttachmentsCount, const RenderTexture *depthAttachment = nullptr, bool depthAttachmentReadOnly = false) {
            this->colorAttachments = colorAttachments;
            this->colorAttachmentsCount = colorAttachmentsCount;
            this->depthAttachment = depthAttachment;
            this->depthAttachmentReadOnly = depthAttachmentReadOnly;
        }
    #endif
    };

    struct RenderBottomLevelASMesh {
        RenderBufferReference indexBuffer;
        RenderBufferReference vertexBuffer;
        RenderFormat indexFormat PLUME_DEFAULT(RenderFormat::UNKNOWN);
        RenderFormat vertexFormat PLUME_DEFAULT(RenderFormat::UNKNOWN);
        uint32_t indexCount PLUME_DEFAULT(0);
        uint32_t vertexCount PLUME_DEFAULT(0);
        uint32_t vertexStride PLUME_DEFAULT(0);
        bool isOpaque PLUME_DEFAULT(false);

    #ifdef __cplusplus
        RenderBottomLevelASMesh() = default;

        RenderBottomLevelASMesh(RenderBufferReference indexBuffer, RenderBufferReference vertexBuffer, RenderFormat indexFormat, RenderFormat vertexFormat, uint32_t indexCount, uint32_t vertexCount, uint32_t vertexStride, bool isOpaque) {
            this->indexBuffer = indexBuffer;
            this->vertexBuffer = vertexBuffer;
            this->indexFormat = indexFormat;
            this->vertexFormat = vertexFormat;
            this->indexCount = indexCount;
            this->vertexCount = vertexCount;
            this->vertexStride = vertexStride;
            this->isOpaque = isOpaque;
        }
    #endif
    };

    struct RenderBottomLevelASBuildInfo {
        uint32_t meshCount PLUME_DEFAULT(0);
        uint32_t primitiveCount PLUME_DEFAULT(0);
        bool preferFastBuild PLUME_DEFAULT(false);
        bool preferFastTrace PLUME_DEFAULT(false);
        uint64_t scratchSize PLUME_DEFAULT(0);
        uint64_t accelerationStructureSize PLUME_DEFAULT(0);

    #ifdef __cplusplus
        // Private backend data. Can go unused.
        std::vector<uint8_t> buildData;
    #endif
    };

    struct RenderTopLevelASInstance {
        RenderBufferReference bottomLevelAS;
        uint32_t instanceID PLUME_DEFAULT(0);
        uint32_t instanceMask PLUME_DEFAULT(0);
        uint32_t instanceContributionToHitGroupIndex PLUME_DEFAULT(0);
        bool cullDisable PLUME_DEFAULT(false);
        RenderAffineTransform transform;

    #ifdef __cplusplus
        RenderTopLevelASInstance() = default;

        RenderTopLevelASInstance(RenderBufferReference bottomLevelAS, uint32_t instanceID, uint32_t instanceMask, uint32_t instanceContributionToHitGroupIndex, bool cullDisable, RenderAffineTransform transform) {
            this->bottomLevelAS = bottomLevelAS;
            this->instanceID = instanceID;
            this->instanceMask = instanceMask;
            this->instanceContributionToHitGroupIndex = instanceContributionToHitGroupIndex;
            this->cullDisable = cullDisable;
            this->transform = transform;
        }
    #endif
    };

#ifdef __cplusplus
    struct RenderTopLevelASBuildInfo {
        // The instances buffer data must be uploaded to the GPU by the API user.
        std::vector<uint8_t> instancesBufferData;
        uint32_t instanceCount PLUME_DEFAULT(0);
        bool preferFastBuild PLUME_DEFAULT(false);
        bool preferFastTrace PLUME_DEFAULT(false);
        uint64_t scratchSize PLUME_DEFAULT(0);
        uint64_t accelerationStructureSize PLUME_DEFAULT(0);

        // Private backend data. Can go unused.
        std::vector<uint8_t> buildData;
    };
#endif

    struct RenderShaderBindingGroup {
        const RenderPipelineProgram *pipelinePrograms PLUME_DEFAULT(nullptr);
        uint32_t pipelineProgramsCount PLUME_DEFAULT(0);

    #ifdef __cplusplus
        RenderShaderBindingGroup() = default;

        RenderShaderBindingGroup(const RenderPipelineProgram *pipelinePrograms, uint32_t pipelineProgramsCount) {
            this->pipelinePrograms = pipelinePrograms;
            this->pipelineProgramsCount = pipelineProgramsCount;
        }
    #endif
    };

    struct RenderShaderBindingGroups {
        RenderShaderBindingGroup rayGen;
        RenderShaderBindingGroup miss;
        RenderShaderBindingGroup hitGroup;
        RenderShaderBindingGroup callable;

    #ifdef __cplusplus
        RenderShaderBindingGroups() = default;

        RenderShaderBindingGroups(RenderShaderBindingGroup rayGen, RenderShaderBindingGroup miss, RenderShaderBindingGroup hitGroup, RenderShaderBindingGroup callable = RenderShaderBindingGroup()) {
            this->rayGen = rayGen;
            this->miss = miss;
            this->hitGroup = hitGroup;
            this->callable = callable;
        }
    #endif
    };

    struct RenderShaderBindingGroupInfo {
        uint64_t offset PLUME_DEFAULT(0);
        uint64_t size PLUME_DEFAULT(0);
        uint32_t stride PLUME_DEFAULT(0);

        // Convenience index for selecting a different binding in the table. offset must add startIndex * stride.
        uint32_t startIndex PLUME_DEFAULT(0);
    };

    struct RenderShaderBindingGroupsInfo {
        RenderShaderBindingGroupInfo rayGen;
        RenderShaderBindingGroupInfo miss;
        RenderShaderBindingGroupInfo hitGroup;
        RenderShaderBindingGroupInfo callable;
    };

#ifdef __cplusplus
    struct RenderShaderBindingTableInfo {
        // The table buffer data must be uploaded to the GPU by the API user and submitted to dispatchRays().
        std::vector<uint8_t> tableBufferData;

        // This info will be requested by dispatchRays().
        RenderShaderBindingGroupsInfo groups;
    };

    struct RenderDeviceDescription {
        std::string name PLUME_DEFAULT("Unknown");
        RenderDeviceType type PLUME_DEFAULT(RenderDeviceType::UNKNOWN);
        RenderDeviceVendor vendor PLUME_DEFAULT(RenderDeviceVendor::UNKNOWN);
        uint64_t driverVersion PLUME_DEFAULT(0);
        uint64_t dedicatedVideoMemory PLUME_DEFAULT(0);
    };
#endif

    struct RenderDeviceCapabilities {
        // Geometry shaders.
        bool geometryShader PLUME_DEFAULT(false);

        // Raytracing.
        bool raytracing PLUME_DEFAULT(false);
        bool raytracingStateUpdate PLUME_DEFAULT(false);

        // MSAA.
        bool sampleLocations PLUME_DEFAULT(false);

        // Resolve.
        bool resolveRegion PLUME_DEFAULT(false);
        bool resolveModes PLUME_DEFAULT(false);

        // Bindless resources.
        bool descriptorIndexing PLUME_DEFAULT(false);
        bool scalarBlockLayout PLUME_DEFAULT(false);

        // Buffers.
        bool bufferDeviceAddress PLUME_DEFAULT(false);

        // Samplers.
        bool samplerMirrorClampToEdge PLUME_DEFAULT(false);

        // Present.
        bool presentWait PLUME_DEFAULT(false);
        bool displayTiming PLUME_DEFAULT(false);

        // Framebuffers.
        uint64_t maxTextureSize PLUME_DEFAULT(0);

        // HDR.
        bool preferHDR PLUME_DEFAULT(false);

        // Draw.
        bool triangleFan PLUME_DEFAULT(false);
        bool dynamicDepthBias PLUME_DEFAULT(false);

        // UMA.
        bool uma PLUME_DEFAULT(false);

        // GPU Upload heap.
        bool gpuUploadHeap PLUME_DEFAULT(false);

        // Query Pools.
        bool queryPools PLUME_DEFAULT(false);
    };

    struct RenderInterfaceCapabilities {
        RenderShaderFormat shaderFormat PLUME_DEFAULT(RenderShaderFormat::UNKNOWN);
    };

#ifdef __cplusplus
};
#endif
