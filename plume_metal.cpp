//
// plume
//
// Copyright (c) 2024 renderbag and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE file for details.
//

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include <CoreFoundation/CoreFoundation.h>

#include <algorithm>
#include <mutex>

#include "plume_metal.h"

namespace plume {
    // MARK: - Constants

    static constexpr size_t MAX_DRAWABLES = 3;
    static constexpr size_t DESCRIPTOR_SETS_BINDING_INDEX = 0;
    static constexpr size_t PUSH_CONSTANTS_BINDING_INDEX = DESCRIPTOR_SETS_BINDING_INDEX + MAX_DESCRIPTOR_SET_BINDINGS;
    static constexpr size_t VERTEX_BUFFERS_BINDING_INDEX = PUSH_CONSTANTS_BINDING_INDEX + MAX_PUSH_CONSTANT_BINDINGS;

    // MARK: - Prototypes

    MTL::PixelFormat mapPixelFormat(RenderFormat format);
    RenderFormat mapRenderFormat(MTL::PixelFormat format);

    // MARK: - Helpers

    constexpr uint64_t alignUp(uint64_t n, uint64_t alignment = 16) {
        return (n + alignment - 1) & ~(alignment - 1);
    }

    uint64_t createClearPipelineKey(MTL::RenderPipelineDescriptor *pipelineDesc, bool depthWriteEnabled, bool stencilWriteEnabled) {
        auto colorFormat = [&](uint32_t index) {
            if (auto colorAttachment = pipelineDesc->colorAttachments()->object(index)) {
                return static_cast<uint64_t>(mapRenderFormat(colorAttachment->pixelFormat()));
            }
            return 0llu;
        };

        ClearPipelineKey key;
        key.value = 0;
        key.depthClear = depthWriteEnabled ? 1 : 0;
        key.stencilClear = stencilWriteEnabled ? 1 : 0;
        key.msaaCount = pipelineDesc->sampleCount();
        key.colorFormat0 = colorFormat(0);
        key.colorFormat1 = colorFormat(1);
        key.colorFormat2 = colorFormat(2);
        key.colorFormat3 = colorFormat(3);
        key.colorFormat4 = colorFormat(4);
        key.colorFormat5 = colorFormat(5);
        key.colorFormat6 = colorFormat(6);
        key.depthFormat = static_cast<uint64_t>(mapRenderFormat(pipelineDesc->depthAttachmentPixelFormat()));

        return key.value;
    }

    NS::UInteger alignmentForRenderFormat(MTL::Device *device, RenderFormat format) {
        const auto deviceAlignment = device->minimumLinearTextureAlignmentForPixelFormat(mapPixelFormat(format));

        NS::UInteger minTexelBufferOffsetAlignment = 0;
    #if TARGET_OS_TV
        minTexelBufferOffsetAlignment = 64;
    #elif TARGET_OS_IPHONE
        minTexelBufferOffsetAlignment = 64;
        if (device->supportsFamily(MTL::GPUFamilyApple3)) {
            minTexelBufferOffsetAlignment = 16;
        }
    #elif TARGET_OS_MAC
        minTexelBufferOffsetAlignment = 256;
        if (device->supportsFamily(MTL::GPUFamilyApple3)) {
            minTexelBufferOffsetAlignment = 16;
        }
    #endif

        return deviceAlignment ? deviceAlignment : minTexelBufferOffsetAlignment;
    }

    MTL::ScissorRect clampScissorRectIfNecessary(const RenderRect& rect, const MetalFramebuffer* targetFramebuffer) {
        // Always clamp the scissor rect to the render target dimensions.
        // RenderRect is signed, but Metal's rect is not. Use a signed max function, then cast to unsigned.
        NS::UInteger left = static_cast<NS::UInteger>(std::max(0, rect.left));
        NS::UInteger top = static_cast<NS::UInteger>(std::max(0, rect.top));
        NS::UInteger right = static_cast<NS::UInteger>(std::max(0, rect.right));
        NS::UInteger bottom = static_cast<NS::UInteger>(std::max(0, rect.bottom));

        if (left >= right || top >= bottom) {
            return MTL::ScissorRect({0u, 0u, 0u, 0u});
        }

        MTL::ScissorRect clampedRect = {
            left,
            top,
            right - left,
            bottom - top
        };

        if (!targetFramebuffer || targetFramebuffer->colorAttachments.empty()) {
            // No need to clamp
            return clampedRect;
        }

        // Always clamp to the attachment dimensions, to avoid Metal API error
        uint32_t maxWidth = INT_MAX;
        uint32_t maxHeight = INT_MAX;
        bool hasAttachments = false;

        for (const auto& attachment : targetFramebuffer->colorAttachments) {
            if (const auto texture = attachment.getTexture()) {
                maxWidth = std::min(static_cast<NS::UInteger>(maxWidth), texture->width());
                maxHeight = std::min(static_cast<NS::UInteger>(maxHeight), texture->height());
                hasAttachments = true;
            }
        }

        // If no valid attachments found, return original rect
        if (!hasAttachments) {
            return clampedRect;
        }

        // Clamp width and height to fit within the render target
        if (clampedRect.x + clampedRect.width > maxWidth) {
            clampedRect.width = maxWidth > clampedRect.x ? maxWidth - clampedRect.x : 0;
        }

        if (clampedRect.y + clampedRect.height > maxHeight) {
            clampedRect.height = maxHeight > clampedRect.y ? maxHeight - clampedRect.y : 0;
        }

        return clampedRect;
    }

    // MARK: - Mapping RHI <> Metal

    MTL::DataType mapDataType(RenderDescriptorRangeType type) {
        switch (type) {
            case RenderDescriptorRangeType::TEXTURE:
            case RenderDescriptorRangeType::READ_WRITE_TEXTURE:
            case RenderDescriptorRangeType::FORMATTED_BUFFER:
            case RenderDescriptorRangeType::READ_WRITE_FORMATTED_BUFFER:
                return MTL::DataTypeTexture;

            case RenderDescriptorRangeType::ACCELERATION_STRUCTURE:
                return MTL::DataTypePrimitiveAccelerationStructure;

            case RenderDescriptorRangeType::STRUCTURED_BUFFER:
            case RenderDescriptorRangeType::BYTE_ADDRESS_BUFFER:
            case RenderDescriptorRangeType::READ_WRITE_STRUCTURED_BUFFER:
            case RenderDescriptorRangeType::READ_WRITE_BYTE_ADDRESS_BUFFER:
            case RenderDescriptorRangeType::CONSTANT_BUFFER:
                return MTL::DataTypePointer;

            case RenderDescriptorRangeType::SAMPLER:
                return MTL::DataTypeSampler;

            default:
                assert(false && "Unknown descriptor range type.");
                return MTL::DataTypeNone;
        }
    }

    RenderFormat mapRenderFormat(MTL::PixelFormat format) {
            switch (format) {
                case MTL::PixelFormatInvalid:
                    return RenderFormat::UNKNOWN;
                case MTL::PixelFormatRGBA32Float:
                    return RenderFormat::R32G32B32A32_FLOAT;
                case MTL::PixelFormatRGBA32Uint:
                    return RenderFormat::R32G32B32A32_UINT;
                case MTL::PixelFormatRGBA32Sint:
                    return RenderFormat::R32G32B32A32_SINT;
                case MTL::PixelFormatRGBA16Float:
                    return RenderFormat::R16G16B16A16_FLOAT;
                case MTL::PixelFormatRGBA16Unorm:
                    return RenderFormat::R16G16B16A16_UNORM;
                case MTL::PixelFormatRGBA16Uint:
                    return RenderFormat::R16G16B16A16_UINT;
                case MTL::PixelFormatRGBA16Snorm:
                    return RenderFormat::R16G16B16A16_SNORM;
                case MTL::PixelFormatRGBA16Sint:
                    return RenderFormat::R16G16B16A16_SINT;
                case MTL::PixelFormatRG32Float:
                    return RenderFormat::R32G32_FLOAT;
                case MTL::PixelFormatRG32Uint:
                    return RenderFormat::R32G32_UINT;
                case MTL::PixelFormatRG32Sint:
                    return RenderFormat::R32G32_SINT;
                case MTL::PixelFormatRGBA8Unorm:
                    return RenderFormat::R8G8B8A8_UNORM;
                case MTL::PixelFormatRGBA8Uint:
                    return RenderFormat::R8G8B8A8_UINT;
                case MTL::PixelFormatRGBA8Snorm:
                    return RenderFormat::R8G8B8A8_SNORM;
                case MTL::PixelFormatRGBA8Sint:
                    return RenderFormat::R8G8B8A8_SINT;
                case MTL::PixelFormatBGRA8Unorm:
                    return RenderFormat::B8G8R8A8_UNORM;
                case MTL::PixelFormatRG16Float:
                    return RenderFormat::R16G16_FLOAT;
                case MTL::PixelFormatRG16Unorm:
                    return RenderFormat::R16G16_UNORM;
                case MTL::PixelFormatRG16Uint:
                    return RenderFormat::R16G16_UINT;
                case MTL::PixelFormatRG16Snorm:
                    return RenderFormat::R16G16_SNORM;
                case MTL::PixelFormatRG16Sint:
                    return RenderFormat::R16G16_SINT;
                case MTL::PixelFormatDepth32Float:
                    return RenderFormat::D32_FLOAT;
                case MTL::PixelFormatDepth32Float_Stencil8:
                    return RenderFormat::D32_FLOAT_S8_UINT;
                case MTL::PixelFormatR32Float:
                    return RenderFormat::R32_FLOAT;
                case MTL::PixelFormatR32Uint:
                    return RenderFormat::R32_UINT;
                case MTL::PixelFormatR32Sint:
                    return RenderFormat::R32_SINT;
                case MTL::PixelFormatRG8Unorm:
                    return RenderFormat::R8G8_UNORM;
                case MTL::PixelFormatRG8Uint:
                    return RenderFormat::R8G8_UINT;
                case MTL::PixelFormatRG8Snorm:
                    return RenderFormat::R8G8_SNORM;
                case MTL::PixelFormatRG8Sint:
                    return RenderFormat::R8G8_SINT;
                case MTL::PixelFormatR16Float:
                    return RenderFormat::R16_FLOAT;
                case MTL::PixelFormatDepth16Unorm:
                    return RenderFormat::D16_UNORM;
                case MTL::PixelFormatR16Unorm:
                    return RenderFormat::R16_UNORM;
                case MTL::PixelFormatR16Uint:
                    return RenderFormat::R16_UINT;
                case MTL::PixelFormatR16Snorm:
                    return RenderFormat::R16_SNORM;
                case MTL::PixelFormatR16Sint:
                    return RenderFormat::R16_SINT;
                case MTL::PixelFormatR8Unorm:
                    return RenderFormat::R8_UNORM;
                case MTL::PixelFormatR8Uint:
                    return RenderFormat::R8_UINT;
                case MTL::PixelFormatR8Snorm:
                    return RenderFormat::R8_SNORM;
                case MTL::PixelFormatR8Sint:
                    return RenderFormat::R8_SINT;
                // Block compressed formats
                case MTL::PixelFormatBC1_RGBA:
                    return RenderFormat::BC1_UNORM;
                case MTL::PixelFormatBC1_RGBA_sRGB:
                    return RenderFormat::BC1_UNORM_SRGB;
                case MTL::PixelFormatBC2_RGBA:
                    return RenderFormat::BC2_UNORM;
                case MTL::PixelFormatBC2_RGBA_sRGB:
                    return RenderFormat::BC2_UNORM_SRGB;
                case MTL::PixelFormatBC3_RGBA:
                    return RenderFormat::BC3_UNORM;
                case MTL::PixelFormatBC3_RGBA_sRGB:
                    return RenderFormat::BC3_UNORM_SRGB;
                case MTL::PixelFormatBC4_RUnorm:
                    return RenderFormat::BC4_UNORM;
                case MTL::PixelFormatBC4_RSnorm:
                    return RenderFormat::BC4_SNORM;
                case MTL::PixelFormatBC5_RGUnorm:
                    return RenderFormat::BC5_UNORM;
                case MTL::PixelFormatBC5_RGSnorm:
                    return RenderFormat::BC5_SNORM;
                case MTL::PixelFormatBC6H_RGBFloat:
                    return RenderFormat::BC6H_SF16;
                case MTL::PixelFormatBC6H_RGBUfloat:
                    return RenderFormat::BC6H_UF16;
                case MTL::PixelFormatBC7_RGBAUnorm:
                    return RenderFormat::BC7_UNORM;
                case MTL::PixelFormatBC7_RGBAUnorm_sRGB:
                    return RenderFormat::BC7_UNORM_SRGB;
                default:
                    assert(false && "Unknown Metal format.");
                    return RenderFormat::UNKNOWN;
            }
        }

    MTL::PixelFormat mapPixelFormat(RenderFormat format) {
        switch (format) {
            case RenderFormat::UNKNOWN:
                return MTL::PixelFormatInvalid;
            case RenderFormat::R32G32B32A32_TYPELESS:
                return MTL::PixelFormatRGBA32Float;
            case RenderFormat::R32G32B32A32_FLOAT:
                return MTL::PixelFormatRGBA32Float;
            case RenderFormat::R32G32B32A32_UINT:
                return MTL::PixelFormatRGBA32Uint;
            case RenderFormat::R32G32B32A32_SINT:
                return MTL::PixelFormatRGBA32Sint;
            case RenderFormat::R32G32B32_TYPELESS:
                return MTL::PixelFormatRGBA32Float;
            case RenderFormat::R32G32B32_FLOAT:
                return MTL::PixelFormatRGBA32Float;
            case RenderFormat::R32G32B32_UINT:
                return MTL::PixelFormatRGBA32Uint;
            case RenderFormat::R32G32B32_SINT:
                return MTL::PixelFormatRGBA32Sint;
            case RenderFormat::R16G16B16A16_TYPELESS:
                return MTL::PixelFormatRGBA16Float;
            case RenderFormat::R16G16B16A16_FLOAT:
                return MTL::PixelFormatRGBA16Float;
            case RenderFormat::R16G16B16A16_UNORM:
                return MTL::PixelFormatRGBA16Unorm;
            case RenderFormat::R16G16B16A16_UINT:
                return MTL::PixelFormatRGBA16Uint;
            case RenderFormat::R16G16B16A16_SNORM:
                return MTL::PixelFormatRGBA16Snorm;
            case RenderFormat::R16G16B16A16_SINT:
                return MTL::PixelFormatRGBA16Sint;
            case RenderFormat::R32G32_TYPELESS:
                return MTL::PixelFormatRG32Float;
            case RenderFormat::R32G32_FLOAT:
                return MTL::PixelFormatRG32Float;
            case RenderFormat::R32G32_UINT:
                return MTL::PixelFormatRG32Uint;
            case RenderFormat::R32G32_SINT:
                return MTL::PixelFormatRG32Sint;
            case RenderFormat::R8G8B8A8_TYPELESS:
                return MTL::PixelFormatRGBA8Unorm;
            case RenderFormat::R8G8B8A8_UNORM:
                return MTL::PixelFormatRGBA8Unorm;
            case RenderFormat::R8G8B8A8_UINT:
                return MTL::PixelFormatRGBA8Uint;
            case RenderFormat::R8G8B8A8_SNORM:
                return MTL::PixelFormatRGBA8Snorm;
            case RenderFormat::R8G8B8A8_SINT:
                return MTL::PixelFormatRGBA8Sint;
            case RenderFormat::B8G8R8A8_UNORM:
                return MTL::PixelFormatBGRA8Unorm;
            case RenderFormat::R16G16_TYPELESS:
                return MTL::PixelFormatRG16Float;
            case RenderFormat::R16G16_FLOAT:
                return MTL::PixelFormatRG16Float;
            case RenderFormat::R16G16_UNORM:
                return MTL::PixelFormatRG16Unorm;
            case RenderFormat::R16G16_UINT:
                return MTL::PixelFormatRG16Uint;
            case RenderFormat::R16G16_SNORM:
                return MTL::PixelFormatRG16Snorm;
            case RenderFormat::R16G16_SINT:
                return MTL::PixelFormatRG16Sint;
            case RenderFormat::R32_TYPELESS:
                return MTL::PixelFormatR32Float;
            case RenderFormat::D32_FLOAT:
                return MTL::PixelFormatDepth32Float;
            case RenderFormat::D32_FLOAT_S8_UINT:
                return MTL::PixelFormatDepth32Float_Stencil8;
            case RenderFormat::R32_FLOAT:
                return MTL::PixelFormatR32Float;
            case RenderFormat::R32_UINT:
                return MTL::PixelFormatR32Uint;
            case RenderFormat::R32_SINT:
                return MTL::PixelFormatR32Sint;
            case RenderFormat::R8G8_TYPELESS:
                return MTL::PixelFormatRG8Unorm;
            case RenderFormat::R8G8_UNORM:
                return MTL::PixelFormatRG8Unorm;
            case RenderFormat::R8G8_UINT:
                return MTL::PixelFormatRG8Uint;
            case RenderFormat::R8G8_SNORM:
                return MTL::PixelFormatRG8Snorm;
            case RenderFormat::R8G8_SINT:
                return MTL::PixelFormatRG8Sint;
            case RenderFormat::R16_TYPELESS:
                return MTL::PixelFormatR16Float;
            case RenderFormat::R16_FLOAT:
                return MTL::PixelFormatR16Float;
            case RenderFormat::D16_UNORM:
                return MTL::PixelFormatDepth16Unorm;
            case RenderFormat::R16_UNORM:
                return MTL::PixelFormatR16Unorm;
            case RenderFormat::R16_UINT:
                return MTL::PixelFormatR16Uint;
            case RenderFormat::R16_SNORM:
                return MTL::PixelFormatR16Snorm;
            case RenderFormat::R16_SINT:
                return MTL::PixelFormatR16Sint;
            case RenderFormat::R8_TYPELESS:
                return MTL::PixelFormatR8Unorm;
            case RenderFormat::R8_UNORM:
                return MTL::PixelFormatR8Unorm;
            case RenderFormat::R8_UINT:
                return MTL::PixelFormatR8Uint;
            case RenderFormat::R8_SNORM:
                return MTL::PixelFormatR8Snorm;
            case RenderFormat::R8_SINT:
                return MTL::PixelFormatR8Sint;
            // Block compressed formats
           case RenderFormat::BC1_TYPELESS:
               return MTL::PixelFormatBC1_RGBA;
           case RenderFormat::BC1_UNORM:
               return MTL::PixelFormatBC1_RGBA;
           case RenderFormat::BC1_UNORM_SRGB:
               return MTL::PixelFormatBC1_RGBA_sRGB;
           case RenderFormat::BC2_TYPELESS:
               return MTL::PixelFormatBC2_RGBA;
           case RenderFormat::BC2_UNORM:
               return MTL::PixelFormatBC2_RGBA;
           case RenderFormat::BC2_UNORM_SRGB:
               return MTL::PixelFormatBC2_RGBA_sRGB;
           case RenderFormat::BC3_TYPELESS:
               return MTL::PixelFormatBC3_RGBA;
           case RenderFormat::BC3_UNORM:
               return MTL::PixelFormatBC3_RGBA;
           case RenderFormat::BC3_UNORM_SRGB:
               return MTL::PixelFormatBC3_RGBA_sRGB;
           case RenderFormat::BC4_TYPELESS:
               return MTL::PixelFormatBC4_RUnorm;
           case RenderFormat::BC4_UNORM:
               return MTL::PixelFormatBC4_RUnorm;
           case RenderFormat::BC4_SNORM:
               return MTL::PixelFormatBC4_RSnorm;
           case RenderFormat::BC5_TYPELESS:
               return MTL::PixelFormatBC5_RGUnorm;
           case RenderFormat::BC5_UNORM:
               return MTL::PixelFormatBC5_RGUnorm;
           case RenderFormat::BC5_SNORM:
               return MTL::PixelFormatBC5_RGSnorm;
           case RenderFormat::BC6H_TYPELESS:
               return MTL::PixelFormatBC6H_RGBFloat;
           case RenderFormat::BC6H_UF16:
               return MTL::PixelFormatBC6H_RGBUfloat;
           case RenderFormat::BC6H_SF16:
               return MTL::PixelFormatBC6H_RGBFloat;
           case RenderFormat::BC7_TYPELESS:
               return MTL::PixelFormatBC7_RGBAUnorm;
           case RenderFormat::BC7_UNORM:
               return MTL::PixelFormatBC7_RGBAUnorm;
           case RenderFormat::BC7_UNORM_SRGB:
               return MTL::PixelFormatBC7_RGBAUnorm_sRGB;
            default:
                assert(false && "Unknown format.");
                return MTL::PixelFormatInvalid;
        }
    }

    MTL::VertexFormat mapVertexFormat(RenderFormat format) {
        switch (format) {
            case RenderFormat::UNKNOWN:
                return MTL::VertexFormatInvalid;
            case RenderFormat::R32G32B32A32_FLOAT:
                return MTL::VertexFormatFloat4;
            case RenderFormat::R32G32B32A32_UINT:
                return MTL::VertexFormatUInt4;
            case RenderFormat::R32G32B32A32_SINT:
                return MTL::VertexFormatInt4;
            case RenderFormat::R32G32B32_FLOAT:
                return MTL::VertexFormatFloat3;
            case RenderFormat::R32G32B32_UINT:
                return MTL::VertexFormatUInt3;
            case RenderFormat::R32G32B32_SINT:
                return MTL::VertexFormatInt3;
            case RenderFormat::R16G16B16A16_FLOAT:
                return MTL::VertexFormatHalf4;
            case RenderFormat::R16G16B16A16_UNORM:
                return MTL::VertexFormatUShort4Normalized;
            case RenderFormat::R16G16B16A16_UINT:
                return MTL::VertexFormatUShort4;
            case RenderFormat::R16G16B16A16_SNORM:
                return MTL::VertexFormatShort4Normalized;
            case RenderFormat::R16G16B16A16_SINT:
                return MTL::VertexFormatShort4;
            case RenderFormat::R32G32_FLOAT:
                return MTL::VertexFormatFloat2;
            case RenderFormat::R32G32_UINT:
                return MTL::VertexFormatUInt2;
            case RenderFormat::R32G32_SINT:
                return MTL::VertexFormatInt2;
            case RenderFormat::R8G8B8A8_UNORM:
                return MTL::VertexFormatUChar4Normalized;
            case RenderFormat::B8G8R8A8_UNORM:
                return MTL::VertexFormatUChar4Normalized_BGRA;
            case RenderFormat::R8G8B8A8_UINT:
                return MTL::VertexFormatUChar4;
            case RenderFormat::R8G8B8A8_SNORM:
                return MTL::VertexFormatChar4Normalized;
            case RenderFormat::R8G8B8A8_SINT:
                return MTL::VertexFormatChar4;
            case RenderFormat::R16G16_FLOAT:
                return MTL::VertexFormatHalf2;
            case RenderFormat::R16G16_UNORM:
                return MTL::VertexFormatUShort2Normalized;
            case RenderFormat::R16G16_UINT:
                return MTL::VertexFormatUShort2;
            case RenderFormat::R16G16_SNORM:
                return MTL::VertexFormatShort2Normalized;
            case RenderFormat::R16G16_SINT:
                return MTL::VertexFormatShort2;
            case RenderFormat::R32_FLOAT:
                return MTL::VertexFormatFloat;
            case RenderFormat::R32_UINT:
                return MTL::VertexFormatUInt;
            case RenderFormat::R32_SINT:
                return MTL::VertexFormatInt;
            case RenderFormat::R8G8_UNORM:
                return MTL::VertexFormatUChar2Normalized;
            case RenderFormat::R8G8_UINT:
                return MTL::VertexFormatUChar2;
            case RenderFormat::R8G8_SNORM:
                return MTL::VertexFormatChar2Normalized;
            case RenderFormat::R8G8_SINT:
                return MTL::VertexFormatChar2;
            case RenderFormat::R16_FLOAT:
                return MTL::VertexFormatHalf;
            case RenderFormat::R16_UNORM:
                return MTL::VertexFormatUShortNormalized;
            case RenderFormat::R16_UINT:
                return MTL::VertexFormatUShort;
            case RenderFormat::R16_SNORM:
                return MTL::VertexFormatShortNormalized;
            case RenderFormat::R16_SINT:
                return MTL::VertexFormatShort;
            case RenderFormat::R8_UNORM:
                return MTL::VertexFormatUCharNormalized;
            case RenderFormat::R8_UINT:
                return MTL::VertexFormatUChar;
            case RenderFormat::R8_SNORM:
                return MTL::VertexFormatCharNormalized;
            case RenderFormat::R8_SINT:
                return MTL::VertexFormatChar;
            default:
                assert(false && "Unsupported vertex format.");
                return MTL::VertexFormatInvalid;
        }
    }

    MTL::IndexType mapIndexFormat(RenderFormat format) {
        switch (format) {
            case RenderFormat::R16_UINT:
                return MTL::IndexTypeUInt16;
            case RenderFormat::R32_UINT:
                return MTL::IndexTypeUInt32;
            default:
                assert(false && "Format is not supported as an index type.");
                return MTL::IndexTypeUInt16;
        }
    }

    MTL::TextureType mapTextureType(RenderTextureDimension dimension, RenderSampleCounts sampleCount, uint32_t arraySize) {
        switch (dimension) {
            case RenderTextureDimension::TEXTURE_1D:
                assert(sampleCount <= 1 && "Multisampling not supported for 1D textures");
                return (arraySize > 1) ? MTL::TextureType1DArray : MTL::TextureType1D;
            case RenderTextureDimension::TEXTURE_2D:
                if (arraySize > 1) {
                    return (sampleCount > 1) ? MTL::TextureType2DMultisampleArray : MTL::TextureType2DArray;
                } else {
                    return (sampleCount > 1) ? MTL::TextureType2DMultisample : MTL::TextureType2D;
                }
            case RenderTextureDimension::TEXTURE_3D:
                assert(sampleCount <= 1 && "Multisampling not supported for 3D textures");
                return MTL::TextureType3D;
            default:
                assert(false && "Unknown resource dimension.");
                return MTL::TextureType2D;
        }
    }

    MTL::TextureType mapTextureViewType(RenderTextureViewDimension dimension, RenderSampleCounts sampleCount, uint32_t arraySize) {
        switch (dimension) {
            case RenderTextureViewDimension::TEXTURE_1D:
                assert(sampleCount <= 1 && "Multisampling not supported for 1D textures");
                return (arraySize > 1) ? MTL::TextureType1DArray : MTL::TextureType1D;
            case RenderTextureViewDimension::TEXTURE_2D:
                if (arraySize > 1) {
                    return (sampleCount > 1) ? MTL::TextureType2DMultisampleArray : MTL::TextureType2DArray;
                } else {
                    return (sampleCount > 1) ? MTL::TextureType2DMultisample : MTL::TextureType2D;
                }
            case RenderTextureViewDimension::TEXTURE_3D:
                assert(sampleCount <= 1 && "Multisampling not supported for 3D textures");
                return MTL::TextureType3D;
            case RenderTextureViewDimension::TEXTURE_CUBE:
                return MTL::TextureTypeCube;
            default:
                assert(false && "Unknown resource dimension.");
                return MTL::TextureType2D;
        }
    }

    MTL::CullMode mapCullMode(RenderCullMode cullMode) {
        switch (cullMode) {
            case RenderCullMode::NONE:
                return MTL::CullModeNone;
            case RenderCullMode::FRONT:
                return MTL::CullModeFront;
            case RenderCullMode::BACK:
                return MTL::CullModeBack;
            default:
                assert(false && "Unknown cull mode.");
                return MTL::CullModeNone;
        }
    }

    MTL::Winding mapFrontFace(RenderFrontFace frontFace) {
        switch (frontFace) {
            case RenderFrontFace::CLOCKWISE:
                return MTL::WindingClockwise;
            case RenderFrontFace::COUNTER_CLOCKWISE:
                return MTL::WindingCounterClockwise;
            default:
                assert(false && "Unknown front face.");
                return MTL::WindingClockwise;
        }
    }

    MTL::PrimitiveTopologyClass mapPrimitiveTopologyClass(RenderPrimitiveTopology topology) {
        switch (topology) {
            case RenderPrimitiveTopology::POINT_LIST:
                return MTL::PrimitiveTopologyClassPoint;
            case RenderPrimitiveTopology::LINE_LIST:
            case RenderPrimitiveTopology::LINE_STRIP:
                return MTL::PrimitiveTopologyClassLine;
            case RenderPrimitiveTopology::TRIANGLE_LIST:
            case RenderPrimitiveTopology::TRIANGLE_STRIP:
                return MTL::PrimitiveTopologyClassTriangle;
            default:
                assert(false && "Unknown primitive topology type.");
                return MTL::PrimitiveTopologyClassPoint;
        }
    }

    MTL::PrimitiveType mapPrimitiveType(const RenderPrimitiveTopology topology) {
        switch (topology) {
            case RenderPrimitiveTopology::POINT_LIST:
                return MTL::PrimitiveTypePoint;
            case RenderPrimitiveTopology::LINE_LIST:
                return MTL::PrimitiveTypeLine;
            case RenderPrimitiveTopology::LINE_STRIP:
                return MTL::PrimitiveTypeLineStrip;
            case RenderPrimitiveTopology::TRIANGLE_LIST:
                return MTL::PrimitiveTypeTriangle;
            case RenderPrimitiveTopology::TRIANGLE_STRIP:
                return MTL::PrimitiveTypeTriangleStrip;
            case RenderPrimitiveTopology::TRIANGLE_FAN:
                assert(false && "Triangle fan is not supported by Metal.");
            default:
                assert(false && "Unknown primitive topology.");
                return MTL::PrimitiveTypePoint;
        }
    }

    MTL::VertexStepFunction mapVertexStepFunction(RenderInputSlotClassification classification) {
        switch (classification) {
            case RenderInputSlotClassification::PER_VERTEX_DATA:
                return MTL::VertexStepFunctionPerVertex;
            case RenderInputSlotClassification::PER_INSTANCE_DATA:
                return MTL::VertexStepFunctionPerInstance;
            default:
                assert(false && "Unknown input classification.");
                return MTL::VertexStepFunctionPerVertex;
        }
    }

    MTL::BlendFactor mapBlendFactor(RenderBlend blend) {
        switch (blend) {
            case RenderBlend::ZERO:
                return MTL::BlendFactorZero;
            case RenderBlend::ONE:
                return MTL::BlendFactorOne;
            case RenderBlend::SRC_COLOR:
                return MTL::BlendFactorSourceColor;
            case RenderBlend::INV_SRC_COLOR:
                return MTL::BlendFactorOneMinusSourceColor;
            case RenderBlend::SRC_ALPHA:
                return MTL::BlendFactorSourceAlpha;
            case RenderBlend::INV_SRC_ALPHA:
                return MTL::BlendFactorOneMinusSourceAlpha;
            case RenderBlend::DEST_ALPHA:
                return MTL::BlendFactorDestinationAlpha;
            case RenderBlend::INV_DEST_ALPHA:
                return MTL::BlendFactorOneMinusDestinationAlpha;
            case RenderBlend::DEST_COLOR:
                return MTL::BlendFactorDestinationColor;
            case RenderBlend::INV_DEST_COLOR:
                return MTL::BlendFactorOneMinusDestinationColor;
            case RenderBlend::SRC_ALPHA_SAT:
                return MTL::BlendFactorSourceAlphaSaturated;
            case RenderBlend::BLEND_FACTOR:
                return MTL::BlendFactorBlendColor;
            case RenderBlend::INV_BLEND_FACTOR:
                return MTL::BlendFactorOneMinusBlendColor;
            case RenderBlend::SRC1_COLOR:
                return MTL::BlendFactorSource1Color;
            case RenderBlend::INV_SRC1_COLOR:
                return MTL::BlendFactorOneMinusSource1Color;
            case RenderBlend::SRC1_ALPHA:
                return MTL::BlendFactorSource1Alpha;
            case RenderBlend::INV_SRC1_ALPHA:
                return MTL::BlendFactorOneMinusSource1Alpha;
            default:
                assert(false && "Unknown blend factor.");
                return MTL::BlendFactorZero;
        }
    }

    MTL::BlendOperation mapBlendOperation(RenderBlendOperation operation) {
        switch (operation) {
            case RenderBlendOperation::ADD:
                return MTL::BlendOperationAdd;
            case RenderBlendOperation::SUBTRACT:
                return MTL::BlendOperationSubtract;
            case RenderBlendOperation::REV_SUBTRACT:
                return MTL::BlendOperationReverseSubtract;
            case RenderBlendOperation::MIN:
                return MTL::BlendOperationMin;
            case RenderBlendOperation::MAX:
                return MTL::BlendOperationMax;
            default:
                assert(false && "Unknown blend operation.");
                return MTL::BlendOperationAdd;
        }
    }

    // Metal does not support Logic Operations in the public API.

    MTL::CompareFunction mapCompareFunction(RenderComparisonFunction function) {
        switch (function) {
            case RenderComparisonFunction::NEVER:
                return MTL::CompareFunctionNever;
            case RenderComparisonFunction::LESS:
                return MTL::CompareFunctionLess;
            case RenderComparisonFunction::EQUAL:
                return MTL::CompareFunctionEqual;
            case RenderComparisonFunction::LESS_EQUAL:
                return MTL::CompareFunctionLessEqual;
            case RenderComparisonFunction::GREATER:
                return MTL::CompareFunctionGreater;
            case RenderComparisonFunction::NOT_EQUAL:
                return MTL::CompareFunctionNotEqual;
            case RenderComparisonFunction::GREATER_EQUAL:
                return MTL::CompareFunctionGreaterEqual;
            case RenderComparisonFunction::ALWAYS:
                return MTL::CompareFunctionAlways;
            default:
                assert(false && "Unknown comparison function.");
                return MTL::CompareFunctionNever;
        }
    }

    MTL::StencilOperation mapStencilOperation(RenderStencilOp stencilOp) {
        switch (stencilOp) {
            case RenderStencilOp::KEEP:
                return MTL::StencilOperationKeep;
            case RenderStencilOp::ZERO:
                return MTL::StencilOperationZero;
            case RenderStencilOp::REPLACE:
                return MTL::StencilOperationReplace;
            case RenderStencilOp::INCREMENT_AND_CLAMP:
                return MTL::StencilOperationIncrementClamp;
            case RenderStencilOp::DECREMENT_AND_CLAMP:
                return MTL::StencilOperationDecrementClamp;
            case RenderStencilOp::INVERT:
                return MTL::StencilOperationInvert;
            case RenderStencilOp::INCREMENT_AND_WRAP:
                return MTL::StencilOperationIncrementWrap;
            case RenderStencilOp::DECREMENT_AND_WRAP:
                return MTL::StencilOperationDecrementWrap;
            default:
                assert(false && "Unknown stencil operation.");
                return MTL::StencilOperationKeep;
        }
    }

    MTL::SamplerMinMagFilter mapSamplerMinMagFilter(RenderFilter filter) {
        switch (filter) {
            case RenderFilter::NEAREST:
                return MTL::SamplerMinMagFilterNearest;
            case RenderFilter::LINEAR:
                return MTL::SamplerMinMagFilterLinear;
            default:
                assert(false && "Unknown filter.");
                return MTL::SamplerMinMagFilterNearest;
        }
    }

    MTL::SamplerMipFilter mapSamplerMipFilter(RenderMipmapMode mode) {
        switch (mode) {
            case RenderMipmapMode::NEAREST:
                return MTL::SamplerMipFilterNearest;
            case RenderMipmapMode::LINEAR:
                return MTL::SamplerMipFilterLinear;
            default:
                assert(false && "Unknown mipmap mode.");
                return MTL::SamplerMipFilterNearest;
        }
    }

    MTL::SamplerAddressMode mapSamplerAddressMode(RenderTextureAddressMode mode) {
        switch (mode) {
            case RenderTextureAddressMode::WRAP:
                return MTL::SamplerAddressModeRepeat;
            case RenderTextureAddressMode::MIRROR:
                return MTL::SamplerAddressModeMirrorRepeat;
            case RenderTextureAddressMode::CLAMP:
                return MTL::SamplerAddressModeClampToEdge;
            case RenderTextureAddressMode::BORDER:
                return MTL::SamplerAddressModeClampToBorderColor;
            case RenderTextureAddressMode::MIRROR_ONCE:
                return MTL::SamplerAddressModeMirrorClampToEdge;
            default:
                assert(false && "Unknown texture address mode.");
                return MTL::SamplerAddressModeRepeat;
        }
    }

    MTL::SamplerBorderColor mapSamplerBorderColor(RenderBorderColor color) {
        switch (color) {
            case RenderBorderColor::TRANSPARENT_BLACK:
                return MTL::SamplerBorderColorTransparentBlack;
            case RenderBorderColor::OPAQUE_BLACK:
                return MTL::SamplerBorderColorOpaqueBlack;
            case RenderBorderColor::OPAQUE_WHITE:
                return MTL::SamplerBorderColorOpaqueWhite;
            default:
                assert(false && "Unknown border color.");
                return MTL::SamplerBorderColorTransparentBlack;
        }
    }

    MTL::ResourceOptions mapResourceOption(RenderHeapType heapType) {
        switch (heapType) {
            case RenderHeapType::DEFAULT:
                return MTL::ResourceStorageModePrivate;
            case RenderHeapType::UPLOAD:
            case RenderHeapType::READBACK:
            case RenderHeapType::GPU_UPLOAD:
                return MTL::ResourceStorageModeShared;
            default:
                assert(false && "Unknown heap type.");
                return MTL::ResourceStorageModePrivate;
        }
    }

    MTL::StorageMode mapStorageMode(RenderHeapType heapType) {
        switch (heapType) {
            case RenderHeapType::DEFAULT:
                return MTL::StorageModePrivate;
            case RenderHeapType::UPLOAD:
                return MTL::StorageModeShared;
            case RenderHeapType::READBACK:
                return MTL::StorageModeShared;
            default:
                assert(false && "Unknown heap type.");
                return MTL::StorageModePrivate;
        }
    }

    MTL::ClearColor mapClearColor(RenderColor color) {
        return MTL::ClearColor(color.r, color.g, color.b, color.a);
    }

    MTL::ResourceUsage mapResourceUsage(RenderDescriptorRangeType type) {
        switch (type) {
            case RenderDescriptorRangeType::TEXTURE:
            case RenderDescriptorRangeType::FORMATTED_BUFFER:
            case RenderDescriptorRangeType::ACCELERATION_STRUCTURE:
            case RenderDescriptorRangeType::STRUCTURED_BUFFER:
            case RenderDescriptorRangeType::BYTE_ADDRESS_BUFFER:
            case RenderDescriptorRangeType::CONSTANT_BUFFER:
            case RenderDescriptorRangeType::SAMPLER:
                return MTL::ResourceUsageRead;

            case RenderDescriptorRangeType::READ_WRITE_FORMATTED_BUFFER:
            case RenderDescriptorRangeType::READ_WRITE_STRUCTURED_BUFFER:
            case RenderDescriptorRangeType::READ_WRITE_BYTE_ADDRESS_BUFFER:
            case RenderDescriptorRangeType::READ_WRITE_TEXTURE:
                return MTL::ResourceUsageRead | MTL::ResourceUsageWrite;
            default:
                assert(false && "Unknown descriptor range type.");
                return MTL::DataTypeNone;
        }
    }

    MTL::TextureUsage mapTextureUsageFromBufferFlags(RenderBufferFlags flags) {
        MTL::TextureUsage usage = MTL::TextureUsageShaderRead;
        usage |= (flags & RenderBufferFlag::UNORDERED_ACCESS) ? MTL::TextureUsageShaderWrite : MTL::TextureUsageUnknown;

        return usage;
    }

    MTL::TextureUsage mapTextureUsage(RenderTextureFlags flags) {
        MTL::TextureUsage usage = MTL::TextureUsageShaderRead;

        if (flags & RenderTextureFlag::RENDER_TARGET)
            usage |= MTL::TextureUsageRenderTarget;

        if (flags & RenderTextureFlag::DEPTH_TARGET)
            usage |= MTL::TextureUsageRenderTarget;

        if (flags & RenderTextureFlag::UNORDERED_ACCESS)
            usage |= MTL::TextureUsageShaderWrite;

        return usage;
    }

    MTL::TextureSwizzle mapTextureSwizzle(RenderSwizzle swizzle) {
        switch (swizzle) {
        case RenderSwizzle::ZERO:
            return MTL::TextureSwizzleZero;
        case RenderSwizzle::ONE:
            return MTL::TextureSwizzleOne;
        case RenderSwizzle::R:
            return MTL::TextureSwizzleRed;
        case RenderSwizzle::G:
            return MTL::TextureSwizzleGreen;
        case RenderSwizzle::B:
            return MTL::TextureSwizzleBlue;
        case RenderSwizzle::A:
            return MTL::TextureSwizzleAlpha;
        default:
            assert(false && "Unknown swizzle type.");
            return MTL::TextureSwizzleRed;
        }
    }

    MTL::TextureSwizzleChannels mapTextureSwizzleChannels(RenderComponentMapping mapping) {
    #define convert(v, d) \
        v == RenderSwizzle::IDENTITY ? MTL::TextureSwizzle##d : mapTextureSwizzle(v)
        return MTL::TextureSwizzleChannels(convert(mapping.r, Red), convert(mapping.g, Green), convert(mapping.b, Blue), convert(mapping.a, Alpha));
    #undef convert
    }

    MTL::ColorWriteMask mapColorWriteMask(uint8_t mask) {
        MTL::ColorWriteMask metalMask = MTL::ColorWriteMaskNone;

        if (mask & static_cast<uint8_t>(RenderColorWriteEnable::RED))
            metalMask |= MTL::ColorWriteMaskRed;
        if (mask & static_cast<uint8_t>(RenderColorWriteEnable::GREEN))
            metalMask |= MTL::ColorWriteMaskGreen;
        if (mask & static_cast<uint8_t>(RenderColorWriteEnable::BLUE))
            metalMask |= MTL::ColorWriteMaskBlue;
        if (mask & static_cast<uint8_t>(RenderColorWriteEnable::ALPHA))
            metalMask |= MTL::ColorWriteMaskAlpha;

        return metalMask;
    }

    RenderDeviceType mapDeviceType(MTL::DeviceLocation location) {
        switch (location) {
            case MTL::DeviceLocationBuiltIn:
                return RenderDeviceType::INTEGRATED;
            case MTL::DeviceLocationExternal:
            case MTL::DeviceLocationSlot:
                return RenderDeviceType::DISCRETE;
            default:
                assert(false && "Unknown device location.");
                return RenderDeviceType::UNKNOWN;
        }
    }

    // MARK: - Helper Structures

    MetalDescriptorSetLayout::MetalDescriptorSetLayout(MetalDevice *device, const RenderDescriptorSetDesc &desc) {
        assert(device != nullptr);
        this->device = device;

        NS::AutoreleasePool *releasePool = NS::AutoreleasePool::alloc()->init();

        // Initialize binding vector with -1 (invalid index)
        bindingToIndex.resize(MAX_BINDING_NUMBER, -1);

        // Pre-allocate vectors with known size
        const uint32_t totalDescriptors = desc.descriptorRangesCount + (desc.lastRangeIsBoundless ? desc.boundlessRangeSize : 0);
        descriptorIndexBases.reserve(totalDescriptors);
        descriptorBindingIndices.reserve(totalDescriptors);
        setBindings.reserve(desc.descriptorRangesCount);
        argumentDescriptors.reserve(totalDescriptors);

        // First pass: Calculate descriptor bases and bindings
        for (uint32_t i = 0; i < desc.descriptorRangesCount; i++) {
            const RenderDescriptorRange &range = desc.descriptorRanges[i];
            uint32_t indexBase = uint32_t(descriptorIndexBases.size());

            descriptorIndexBases.resize(descriptorIndexBases.size() + range.count, indexBase);
            descriptorBindingIndices.resize(descriptorBindingIndices.size() + range.count, range.binding);
        }

        // Sort ranges by binding due to how spirv-cross orders them
        std::vector<RenderDescriptorRange> sortedRanges(desc.descriptorRanges, desc.descriptorRanges + desc.descriptorRangesCount);
        std::sort(sortedRanges.begin(), sortedRanges.end(), [](const RenderDescriptorRange &a, const RenderDescriptorRange &b) {
            return a.binding < b.binding;
        });

        // Second pass: Create argument descriptors and set bindings
        const uint32_t rangeCount = desc.lastRangeIsBoundless ? desc.descriptorRangesCount - 1 : desc.descriptorRangesCount;

        auto createBinding = [](const RenderDescriptorRange &range) {
            // The binding exceeds our fixed binding vec limit, increase MAX_BINDING_NUMBER if necessary
            assert(range.binding < MAX_BINDING_NUMBER);

            DescriptorSetLayoutBinding binding = {
                    .binding = range.binding,
                    .descriptorCount = range.count,
                    .descriptorType = range.type,
            };

            if (range.immutableSampler != nullptr) {
                binding.immutableSamplers.resize(range.count);
                for (uint32_t j = 0; j < range.count; j++) {
                    const MetalSampler *sampler = static_cast<const MetalSampler *>(range.immutableSampler[j]);
                    binding.immutableSamplers[j] = sampler->state;
                }
            }

            return binding;
        };

        uint32_t curBinding = 0;

        for (uint32_t i = 0; i < rangeCount; i++) {
            const RenderDescriptorRange &range = sortedRanges[i];

            descriptorTypeMaxIndex += range.count;

            bindingToIndex[range.binding] = setBindings.size();
            setBindings.push_back(createBinding(range));

            while (curBinding < range.binding) {
                // exhaust curBinding with padding till we reach the actual current binding
                MTL::ArgumentDescriptor *argumentDesc = MTL::ArgumentDescriptor::alloc()->init();
                argumentDesc->setDataType(MTL::DataTypePointer);
                argumentDesc->setIndex(curBinding);
                argumentDesc->setArrayLength(0);

                argumentDescriptors.push_back(argumentDesc);
                curBinding++;
            }

            // Include the current binding
            curBinding++;

            // Create argument descriptor
            MTL::ArgumentDescriptor *argumentDesc = MTL::ArgumentDescriptor::alloc()->init();
            argumentDesc->setDataType(mapDataType(range.type));
            argumentDesc->setIndex(range.binding);
            argumentDesc->setArrayLength(range.count > 1 ? range.count : 0);

            if (range.type == RenderDescriptorRangeType::TEXTURE) {
                argumentDesc->setTextureType(MTL::TextureType2D);
            } else if (range.type == RenderDescriptorRangeType::READ_WRITE_FORMATTED_BUFFER || range.type == RenderDescriptorRangeType::FORMATTED_BUFFER) {
                argumentDesc->setTextureType(MTL::TextureTypeTextureBuffer);
            }

            argumentDescriptors.push_back(argumentDesc);
        }

        // Handle boundless range if present
        if (desc.lastRangeIsBoundless) {
            const RenderDescriptorRange &lastRange = sortedRanges[desc.descriptorRangesCount - 1];

            descriptorTypeMaxIndex++;

            bindingToIndex[lastRange.binding] = setBindings.size();
            setBindings.push_back(createBinding(lastRange));

            MTL::ArgumentDescriptor *argumentDesc = MTL::ArgumentDescriptor::alloc()->init();
            argumentDesc->setDataType(mapDataType(lastRange.type));
            argumentDesc->setIndex(lastRange.binding);
            argumentDesc->setArrayLength(device->capabilities.maxTextureSize);

            if (lastRange.type == RenderDescriptorRangeType::TEXTURE) {
                argumentDesc->setTextureType(MTL::TextureType2D);
            } else if (lastRange.type == RenderDescriptorRangeType::READ_WRITE_FORMATTED_BUFFER || lastRange.type == RenderDescriptorRangeType::FORMATTED_BUFFER) {
                argumentDesc->setTextureType(MTL::TextureTypeTextureBuffer);
            }

            argumentDescriptors.push_back(argumentDesc);
        }

        assert(argumentDescriptors.size() > 0);

        // Create and initialize argument encoder
        NS::Array *pArray = (NS::Array*)CFArrayCreate(kCFAllocatorDefault, (const void **)argumentDescriptors.data(), argumentDescriptors.size(), &kCFTypeArrayCallBacks);
        argumentEncoder = device->mtl->newArgumentEncoder(pArray);

        // Release resources
        pArray->release();
        releasePool->release();
    }

    MetalDescriptorSetLayout::DescriptorSetLayoutBinding* MetalDescriptorSetLayout::getBinding(const uint32_t binding, const uint32_t bindingIndexOffset) {
        if (const uint32_t bindingIndex = bindingToIndex[binding] + bindingIndexOffset; bindingIndex < setBindings.size()) {
            return &setBindings[bindingIndex];
        }

        return nullptr;
    }

    MetalDescriptorSetLayout::~MetalDescriptorSetLayout() {
        argumentEncoder->release();
        for (MTL::ArgumentDescriptor *argumentDesc: argumentDescriptors) {
            argumentDesc->release();
        }
    }

    // MetalBuffer

    MetalBuffer::MetalBuffer(MetalDevice *device, MetalPool *pool, const RenderBufferDesc &desc) {
        assert(device != nullptr);

        this->pool = pool;
        this->desc = desc;
        this->device = device;

        this->mtl = device->mtl->newBuffer(desc.size, mapResourceOption(desc.heapType));

        if (desc.flags & RenderBufferFlag::DEVICE_ADDRESSABLE) {
            // If the buffer may be used by device address, we need to make sure it will be resident.
            std::lock_guard lock(device->gpuAddressableResourcesMutex);
            if (device->gpuAddressableResidencySet != nullptr) {
                device->gpuAddressableResidencySet->addAllocation(mtl);
                device->gpuAddressableResidencySet->commit();
            } else {
                device->gpuAddressableResources.push_back(mtl);
            }
        }
    }

    MetalBuffer::~MetalBuffer() {
        if (desc.flags & RenderBufferFlag::DEVICE_ADDRESSABLE) {
            std::lock_guard lock(device->gpuAddressableResourcesMutex);
            if (device->gpuAddressableResidencySet != nullptr) {
                device->gpuAddressableResidencySet->removeAllocation(mtl);
                device->gpuAddressableResidencySet->commit();
            } else {
                const auto it = std::find(device->gpuAddressableResources.begin(), device->gpuAddressableResources.end(), mtl);
                if (it != device->gpuAddressableResources.end()) {
                    device->gpuAddressableResources.erase(it);
                }
            }
        }

        mtl->release();
    }

    void* MetalBuffer::map(uint32_t subresource, const RenderRange* readRange) {
        return mtl->contents();
    }

    void MetalBuffer::unmap(uint32_t subresource, const RenderRange* writtenRange) {
        if (mtl->storageMode() == MTL::StorageModeManaged) {
            if (writtenRange == nullptr) {
                mtl->didModifyRange(NS::Range(0, desc.size));
            } else {
                mtl->didModifyRange(NS::Range(writtenRange->begin, writtenRange->end - writtenRange->begin));
            }
        }
    }

    std::unique_ptr<RenderBufferFormattedView> MetalBuffer::createBufferFormattedView(RenderFormat format) {
        return std::make_unique<MetalBufferFormattedView>(this, format);
    }

    void MetalBuffer::setName(const std::string &name) {
        const NS::String *label = NS::String::string(name.c_str(), NS::UTF8StringEncoding);
        mtl->setLabel(label);
    }

    uint64_t MetalBuffer::getDeviceAddress() const {
        assert(device->mtl->supportsFamily(MTL::GPUFamilyMetal3) && "Device address is only supported on Metal3 devices.");
        assert((desc.flags & RenderBufferFlag::DEVICE_ADDRESSABLE) != 0 && "Buffer must have been created with GPU_ADDRESSABLE flag.");
        return mtl->gpuAddress();
    }

    // MetalBufferFormattedView

    MetalBufferFormattedView::MetalBufferFormattedView(MetalBuffer *buffer, RenderFormat format) {
        assert(buffer != nullptr);
        assert((buffer->desc.flags & RenderBufferFlag::FORMATTED) && "Buffer must allow formatted views.");

        this->buffer = buffer;

        // Calculate texture properties
        const uint32_t width = buffer->desc.size / RenderFormatSize(format);
        const size_t rowAlignment = alignmentForRenderFormat(buffer->device->mtl, format);
        const uint64_t bytesPerRow = alignUp(buffer->desc.size, rowAlignment);

        // Configure texture properties
        const MTL::PixelFormat pixelFormat = mapPixelFormat(format);
        const MTL::TextureUsage usage = mapTextureUsageFromBufferFlags(buffer->desc.flags);
        const MTL::ResourceOptions options = mapResourceOption(buffer->desc.heapType);

        // Create texture with configured descriptor and alignment
        MTL::TextureDescriptor *descriptor = MTL::TextureDescriptor::textureBufferDescriptor(pixelFormat, width, options, usage);
        this->texture = buffer->mtl->newTexture(descriptor, 0, bytesPerRow);

        descriptor->release();
    }

    MetalBufferFormattedView::~MetalBufferFormattedView() {
        texture->release();
    }

    // MetalTexture

    MetalTexture::MetalTexture(const MetalDevice *device, MetalPool *pool, const RenderTextureDesc &desc) {
        assert(device != nullptr);

        this->pool = pool;
        this->desc = desc;

        MTL::TextureDescriptor *descriptor = MTL::TextureDescriptor::alloc()->init();
        const MTL::TextureType textureType = mapTextureType(desc.dimension, desc.multisampling.sampleCount, desc.arraySize);

        descriptor->setTextureType(textureType);
        descriptor->setStorageMode(MTL::StorageModePrivate);
        descriptor->setPixelFormat(mapPixelFormat(desc.format));
        descriptor->setWidth(desc.width);
        descriptor->setHeight(desc.height);
        descriptor->setDepth(desc.depth);
        descriptor->setMipmapLevelCount(desc.mipLevels);
        descriptor->setArrayLength(desc.arraySize);
        descriptor->setSampleCount(desc.multisampling.sampleCount);

        MTL::TextureUsage usage = mapTextureUsage(desc.flags);
        // Add shader write usage if this texture might be used as a resolve target
        if (desc.multisampling.sampleCount == 1 && (usage & MTL::TextureUsageRenderTarget)) {
            usage |= MTL::TextureUsageShaderWrite;
        }
        descriptor->setUsage(usage);

        this->mtl = device->mtl->newTexture(descriptor);

        // Release resources
        descriptor->release();
    }

    MetalTexture::~MetalTexture() {
        mtl->release();
    }

    std::unique_ptr<RenderTextureView> MetalTexture::createTextureView(const RenderTextureViewDesc &desc) const {
        return std::make_unique<MetalTextureView>(this, desc);
    }

    void MetalTexture::setName(const std::string &name) {
        mtl->setLabel(NS::String::string(name.c_str(), NS::UTF8StringEncoding));
    }

    // MetalTextureView

    bool operator==(const RenderTextureDimension lhs, RenderTextureViewDimension rhs) {
        return lhs == static_cast<RenderTextureDimension>(rhs);
    }

    MetalTextureView::MetalTextureView(const MetalTexture *texture, const RenderTextureViewDesc &desc) {
        assert(texture != nullptr);

        this->parentTexture = texture;
        this->desc = desc;

        const uint32_t mipLevels = std::min(desc.mipLevels, texture->desc.mipLevels - desc.mipSlice);
        const uint32_t arraySize = std::min(desc.arraySize, texture->desc.arraySize - desc.arrayIndex);

        this->texture = texture->mtl->newTextureView(
            mapPixelFormat(desc.format),
            mapTextureViewType(desc.dimension, texture->desc.multisampling.sampleCount, arraySize),
            { desc.mipSlice, mipLevels },
            { desc.arrayIndex, arraySize },
            mapTextureSwizzleChannels(desc.componentMapping)
        );
    }

    MetalTextureView::~MetalTextureView() {
        texture->release();
    }

    // MetalAccelerationStructure

    MetalAccelerationStructure::MetalAccelerationStructure(MetalDevice *device, const RenderAccelerationStructureDesc &desc) {
        assert(device != nullptr);
        assert(desc.buffer.ref != nullptr);

        this->device = device;
        this->type = desc.type;
    }

    MetalAccelerationStructure::~MetalAccelerationStructure() { }

    // MetalPool

    MetalPool::MetalPool(MetalDevice *device, const RenderPoolDesc &desc) {
        assert(device != nullptr);
        this->device = device;

        // TODO: Implement MetalPool
        fprintf(stderr, "RenderPool in Metal is not implemented currently. Resources are created directly on device.\n");
    }

    MetalPool::~MetalPool() { }

    std::unique_ptr<RenderBuffer> MetalPool::createBuffer(const RenderBufferDesc &desc) {
        return std::make_unique<MetalBuffer>(device, this, desc);
    }

    std::unique_ptr<RenderTexture> MetalPool::createTexture(const RenderTextureDesc &desc) {
        return std::make_unique<MetalTexture>(device, this, desc);
    }

    // MetalShader

    MetalShader::MetalShader(const MetalDevice *device, const void *data, uint64_t size, const char *entryPointName, const RenderShaderFormat format) {
        assert(device != nullptr);
        assert(data != nullptr);
        assert(size > 0);
        assert(format == RenderShaderFormat::METAL);

        this->format = format;
        this->functionName = (entryPointName != nullptr) ? NS::String::string(entryPointName, NS::UTF8StringEncoding) : MTLSTR("");

        NS::Error *error = nullptr;
        const dispatch_data_t dispatchData = dispatch_data_create(data, size, dispatch_get_main_queue(), ^{});
        library = device->mtl->newLibrary(dispatchData, &error);

        if (error != nullptr) {
            fprintf(stderr, "MTLDevice newLibraryWithSource: failed with error %s.\n", error->localizedDescription()->utf8String());
            return;
        }
    }

    MetalShader::~MetalShader() {
        functionName->release();
        library->release();
        if (debugName) {
            debugName->release();
        }
    }

    void MetalShader::setName(const std::string &name) {
        if (debugName) {
            debugName->release();
        }
        debugName = NS::String::string(name.c_str(), NS::UTF8StringEncoding);
        library->setLabel(debugName);
    }

    MTL::Function* MetalShader::createFunction(const RenderSpecConstant *specConstants, const uint32_t specConstantsCount) const {
        MTL::FunctionConstantValues *values = MTL::FunctionConstantValues::alloc()->init();
        if (specConstants != nullptr) {
            for (uint32_t i = 0; i < specConstantsCount; i++) {
                const RenderSpecConstant &specConstant = specConstants[i];
                values->setConstantValue(&specConstant.value, MTL::DataTypeUInt, specConstant.index);
            }
        }
        NS::Error *error = nullptr;
        MTL::Function *function = library->newFunction(functionName, values, &error);
        values->release();

        if (error != nullptr) {
            fprintf(stderr, "MTLLibrary newFunction: failed with error: %s.\n", error->localizedDescription()->utf8String());
            return nullptr;
        }

        if (debugName) {
            function->setLabel(debugName);
        }

        return function;
    }

    // MetalSampler

    MetalSampler::MetalSampler(const MetalDevice *device, const RenderSamplerDesc &desc) {
        assert(device != nullptr);

        MTL::SamplerDescriptor *descriptor = MTL::SamplerDescriptor::alloc()->init();
        descriptor->setSupportArgumentBuffers(true);
        descriptor->setMinFilter(mapSamplerMinMagFilter(desc.minFilter));
        descriptor->setMagFilter(mapSamplerMinMagFilter(desc.magFilter));
        descriptor->setMipFilter(mapSamplerMipFilter(desc.mipmapMode));
        descriptor->setSAddressMode(mapSamplerAddressMode(desc.addressU));
        descriptor->setTAddressMode(mapSamplerAddressMode(desc.addressV));
        descriptor->setRAddressMode(mapSamplerAddressMode(desc.addressW));
        descriptor->setMaxAnisotropy(desc.maxAnisotropy);
        descriptor->setCompareFunction(mapCompareFunction(desc.comparisonFunc));
        descriptor->setLodMinClamp(desc.minLOD);
        descriptor->setLodMaxClamp(desc.maxLOD);
        descriptor->setBorderColor(mapSamplerBorderColor(desc.borderColor));

        this->state = device->mtl->newSamplerState(descriptor);

        // Release resources
        descriptor->release();
    }

    MetalSampler::~MetalSampler() {
        state->release();
    }

    // MetalPipeline

    MetalPipeline::MetalPipeline(const MetalDevice *device, const Type type) {
        assert(device != nullptr);
        assert(type != Type::Unknown);

        this->type = type;
    }

    MetalPipeline::~MetalPipeline() { }

    // MetalComputePipeline

    MetalComputePipeline::MetalComputePipeline(const MetalDevice *device, const RenderComputePipelineDesc &desc) : MetalPipeline(device, Type::Compute) {
        assert(desc.computeShader != nullptr);
        assert(desc.pipelineLayout != nullptr);
        assert((desc.threadGroupSizeX > 0) && (desc.threadGroupSizeY > 0) && (desc.threadGroupSizeZ > 0));

        const MetalShader *computeShader = static_cast<const MetalShader *>(desc.computeShader);

        MTL::ComputePipelineDescriptor *descriptor = MTL::ComputePipelineDescriptor::alloc()->init();
        MTL::Function *function = computeShader->createFunction(desc.specConstants, desc.specConstantsCount);
        descriptor->setComputeFunction(function);
        descriptor->setLabel(computeShader->functionName);

        // State variables, initialized here to be reused in encoder re-binding
        NS::Error *error = nullptr;
        state.pipelineState = device->mtl->newComputePipelineState(descriptor, MTL::PipelineOptionNone, nullptr, &error);
        state.threadGroupSizeX = desc.threadGroupSizeX;
        state.threadGroupSizeY = desc.threadGroupSizeY;
        state.threadGroupSizeZ = desc.threadGroupSizeZ;

        if (error != nullptr) {
            fprintf(stderr, "MTLDevice newComputePipelineStateWithDescriptor: failed with error %s.\n", error->localizedDescription()->utf8String());
            return;
        }

        // Release resources
        descriptor->release();
        function->release();
    }

    MetalComputePipeline::~MetalComputePipeline() {
        if (state.pipelineState) state.pipelineState->release();
    }

    void MetalComputePipeline::setName(const std::string &name) {
        // TODO: New - setting name happens at descriptor level - this would have to be reworked
    }

    RenderPipelineProgram MetalComputePipeline::getProgram(const std::string &name) const {
        assert(false && "Compute pipelines can't retrieve shader programs.");
        return RenderPipelineProgram();
    }

    // MetalGraphicsPipeline

    MetalGraphicsPipeline::MetalGraphicsPipeline(const MetalDevice *device, const RenderGraphicsPipelineDesc &desc) : MetalPipeline(device, Type::Graphics) {
        assert(desc.pipelineLayout != nullptr);
        NS::AutoreleasePool *releasePool = NS::AutoreleasePool::alloc()->init();

        MTL::RenderPipelineDescriptor *descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
        descriptor->setInputPrimitiveTopology(mapPrimitiveTopologyClass(desc.primitiveTopology));
        descriptor->setRasterSampleCount(desc.multisampling.sampleCount);
        descriptor->setAlphaToCoverageEnabled(desc.alphaToCoverageEnabled);
        descriptor->setDepthAttachmentPixelFormat(mapPixelFormat(desc.depthTargetFormat));
        if (RenderFormatIsStencil(desc.depthTargetFormat)) {
            descriptor->setStencilAttachmentPixelFormat(descriptor->depthAttachmentPixelFormat());
        }
        descriptor->setRasterSampleCount(desc.multisampling.sampleCount);

        assert(desc.vertexShader != nullptr && "Cannot create a valid MTLRenderPipelineState without a vertex shader!");
        const MetalShader *metalShader = static_cast<const MetalShader *>(desc.vertexShader);

        MTL::Function *vertexFunction = metalShader->createFunction(desc.specConstants, desc.specConstantsCount);
        descriptor->setVertexFunction(vertexFunction);

        MTL::VertexDescriptor *vertexDescriptor = MTL::VertexDescriptor::alloc()->init();

        for (uint32_t i = 0; i < desc.inputSlotsCount; i++) {
            const RenderInputSlot &inputSlot = desc.inputSlots[i];
            assert(inputSlot.index < MAX_VERTEX_BUFFER_BINDINGS && "Vertex binding slot index out of range.");

            const uint32_t vertexBufferIndex = VERTEX_BUFFERS_BINDING_INDEX + inputSlot.index;
            MTL::VertexBufferLayoutDescriptor *layout = vertexDescriptor->layouts()->object(vertexBufferIndex);
            if (inputSlot.stride == 0) {
                // Metal does not support stride 0, we must provide a
                // substitute "null" buffer to match behaviour of other robust APIs.
                layout->setStride(1);
                layout->setStepFunction(MTL::VertexStepFunctionConstant);
                layout->setStepRate(0);
            } else {
                layout->setStride(inputSlot.stride);
                layout->setStepFunction(mapVertexStepFunction(inputSlot.classification));
                layout->setStepRate(1);
            }
        }

        for (uint32_t i = 0; i < desc.inputElementsCount; i++) {
            const RenderInputElement &inputElement = desc.inputElements[i];
            assert(inputElement.slotIndex < MAX_VERTEX_BUFFER_BINDINGS && "Vertex attribute slot index out of range.");

            MTL::VertexAttributeDescriptor *attributeDescriptor = vertexDescriptor->attributes()->object(inputElement.location);
            attributeDescriptor->setOffset(inputElement.alignedByteOffset);

            const uint32_t vertexBufferIndex = VERTEX_BUFFERS_BINDING_INDEX + inputElement.slotIndex;
            attributeDescriptor->setBufferIndex(vertexBufferIndex);
            attributeDescriptor->setFormat(mapVertexFormat(inputElement.format));
        }

        descriptor->setVertexDescriptor(vertexDescriptor);

        assert(desc.geometryShader == nullptr && "Metal does not support geometry shaders!");

        if (desc.pixelShader != nullptr) {
            const MetalShader *pixelShader = static_cast<const MetalShader *>(desc.pixelShader);
            MTL::Function *fragmentFunction = pixelShader->createFunction(desc.specConstants, desc.specConstantsCount);
            descriptor->setFragmentFunction(fragmentFunction);
            fragmentFunction->release();
        }

        for (uint32_t i = 0; i < desc.renderTargetCount; i++) {
            const RenderBlendDesc &blendDesc = desc.renderTargetBlend[i];

            MTL::RenderPipelineColorAttachmentDescriptor *blendDescriptor = descriptor->colorAttachments()->object(i);
            blendDescriptor->setBlendingEnabled(blendDesc.blendEnabled);
            blendDescriptor->setSourceRGBBlendFactor(mapBlendFactor(blendDesc.srcBlend));
            blendDescriptor->setDestinationRGBBlendFactor(mapBlendFactor(blendDesc.dstBlend));
            blendDescriptor->setRgbBlendOperation(mapBlendOperation(blendDesc.blendOp));
            blendDescriptor->setSourceAlphaBlendFactor(mapBlendFactor(blendDesc.srcBlendAlpha));
            blendDescriptor->setDestinationAlphaBlendFactor(mapBlendFactor(blendDesc.dstBlendAlpha));
            blendDescriptor->setAlphaBlendOperation(mapBlendOperation(blendDesc.blendOpAlpha));
            blendDescriptor->setWriteMask(mapColorWriteMask(blendDesc.renderTargetWriteMask));
            blendDescriptor->setPixelFormat(mapPixelFormat(desc.renderTargetFormat[i]));
        }

        // State variables, initialized here to be reused in encoder re-binding
        MTL::DepthStencilDescriptor *depthStencilDescriptor = MTL::DepthStencilDescriptor::alloc()->init();
        MTL::StencilDescriptor *frontFaceStencilDescriptor = nullptr;
        MTL::StencilDescriptor *backFaceStencilDescriptor = nullptr;

        if (desc.depthTargetFormat != RenderFormat::UNKNOWN) {
            depthStencilDescriptor->setDepthWriteEnabled(desc.depthWriteEnabled);
            depthStencilDescriptor->setDepthCompareFunction(desc.depthEnabled ? mapCompareFunction(desc.depthFunction) : MTL::CompareFunctionAlways);

            if (desc.stencilEnabled) {
                frontFaceStencilDescriptor = MTL::StencilDescriptor::alloc()->init();
                frontFaceStencilDescriptor->setStencilFailureOperation(mapStencilOperation(desc.stencilFrontFace.failOp));
                frontFaceStencilDescriptor->setDepthFailureOperation(mapStencilOperation(desc.stencilFrontFace.depthFailOp));
                frontFaceStencilDescriptor->setDepthStencilPassOperation(mapStencilOperation(desc.stencilFrontFace.passOp));
                frontFaceStencilDescriptor->setStencilCompareFunction(mapCompareFunction(desc.stencilFrontFace.compareFunction));
                frontFaceStencilDescriptor->setReadMask(desc.stencilReadMask);
                frontFaceStencilDescriptor->setWriteMask(desc.stencilWriteMask);

                backFaceStencilDescriptor = MTL::StencilDescriptor::alloc()->init();
                backFaceStencilDescriptor->setStencilFailureOperation(mapStencilOperation(desc.stencilBackFace.failOp));
                backFaceStencilDescriptor->setDepthFailureOperation(mapStencilOperation(desc.stencilBackFace.depthFailOp));
                backFaceStencilDescriptor->setDepthStencilPassOperation(mapStencilOperation(desc.stencilBackFace.passOp));
                backFaceStencilDescriptor->setStencilCompareFunction(mapCompareFunction(desc.stencilBackFace.compareFunction));
                backFaceStencilDescriptor->setReadMask(desc.stencilReadMask);
                backFaceStencilDescriptor->setWriteMask(desc.stencilWriteMask);

                depthStencilDescriptor->setFrontFaceStencil(frontFaceStencilDescriptor);
                depthStencilDescriptor->setBackFaceStencil(backFaceStencilDescriptor);
            }
        }

        NS::Error *error = nullptr;
        state.depthStencilState = device->mtl->newDepthStencilState(depthStencilDescriptor);
        state.cullMode = mapCullMode(desc.cullMode);
        state.depthClipMode = (desc.depthClipEnabled) ? MTL::DepthClipModeClip : MTL::DepthClipModeClamp;
        state.winding = mapFrontFace(desc.frontFace);
        state.renderPipelineState = device->mtl->newRenderPipelineState(descriptor, &error);
        state.primitiveType = mapPrimitiveType(desc.primitiveTopology);
        state.stencilReference = desc.stencilEnabled ? desc.stencilReference : 0;

        if (desc.dynamicDepthBiasEnabled) {
            state.dynamicDepthBiasEnabled = true;
        } else if (desc.depthBias != 0 || desc.slopeScaledDepthBias != 0.0f) {
            state.dynamicDepthBiasEnabled = false;
            state.depthBiasConstantFactor = static_cast<float>(desc.depthBias);
            state.depthBiasClamp = desc.depthBiasClamp;
            state.depthBiasSlopeFactor = desc.slopeScaledDepthBias;
        }

        if (error != nullptr) {
            fprintf(stderr, "MTLDevice newRenderPipelineState: failed with error %s.\n", error->localizedDescription()->utf8String());
            return;
        }

        // Release resources
        vertexDescriptor->release();
        vertexFunction->release();
        descriptor->release();
        depthStencilDescriptor->release();
        if (frontFaceStencilDescriptor) {
            frontFaceStencilDescriptor->release();
        }
        if (backFaceStencilDescriptor) {
            backFaceStencilDescriptor->release();
        }
        releasePool->release();
    }

    MetalGraphicsPipeline::~MetalGraphicsPipeline() {
        if (state.renderPipelineState) state.renderPipelineState->release();
        if (state.depthStencilState) state.depthStencilState->release();
    }

    void MetalGraphicsPipeline::setName(const std::string &name) {
        // TODO: New - setting name happens at descriptor level - this would have to be reworked
    }

    RenderPipelineProgram MetalGraphicsPipeline::getProgram(const std::string &name) const {
        assert(false && "Graphics pipelines can't retrieve shader programs.");
        return RenderPipelineProgram();
    }

    // MetalDescriptorSet

    MetalDescriptorSet::MetalDescriptorSet(MetalDevice *device, const RenderDescriptorSetDesc &desc) {
        assert(device != nullptr);

        this->device = device;

        thread_local std::unordered_map<RenderDescriptorRangeType, uint32_t> typeCounts;
        typeCounts.clear();

        // Figure out the total amount of entries that will be required.
        uint32_t rangeCount = desc.descriptorRangesCount;
        if (desc.lastRangeIsBoundless) {
            assert((desc.descriptorRangesCount > 0) && "There must be at least one descriptor set to define the last range as boundless.");

            // Ensure at least one entry is created for boundless ranges.
            const uint32_t boundlessRangeSize = std::max(desc.boundlessRangeSize, 1U);

            const RenderDescriptorRange &lastDescriptorRange = desc.descriptorRanges[desc.descriptorRangesCount - 1];
            typeCounts[lastDescriptorRange.type] += boundlessRangeSize;
            rangeCount--;
        }

        for (uint32_t i = 0; i < desc.descriptorRangesCount; i++) {
            const RenderDescriptorRange &descriptorRange = desc.descriptorRanges[i];
            typeCounts[descriptorRange.type] += descriptorRange.count;
        }

        setLayout = std::make_unique<MetalDescriptorSetLayout>(device, desc);

        const uint32_t maxResources = setLayout->descriptorBindingIndices.size();
        // When using more than 128 resources, use residency sets for greater efficiency.
        if (maxResources > 128 && device->supportsResidencySets) {
            MTL::ResidencySetDescriptor* descriptor = MTL::ResidencySetDescriptor::alloc()->init();
            descriptor->setInitialCapacity(maxResources);

            residencySet = device->mtl->newResidencySet(descriptor, nullptr);

            descriptor->release();
        }

        uint64_t requiredSize = setLayout->argumentEncoder->encodedLength();
        requiredSize = alignUp(requiredSize, 256);

        argumentBuffer = {
            .mtl = device->mtl->newBuffer(requiredSize, MTL::ResourceStorageModeShared),
            .argumentEncoder = setLayout->argumentEncoder,
            .offset = 0,
        };

        argumentBuffer.argumentEncoder->setArgumentBuffer(argumentBuffer.mtl, argumentBuffer.offset);
        bindImmutableSamplers();

        resourceEntries.resize(maxResources);
    }

    MetalDescriptorSet::~MetalDescriptorSet() {
        if (residencySet != nullptr) {
            residencySet->endResidency();
            residencySet->release();
        }

        for (const auto &entry : resourceEntries) {
            if (entry.resource != nullptr) {
                entry.resource->release();
            }
        }

        if (argumentBuffer.mtl != nullptr) {
            argumentBuffer.mtl->release();
        }
    }

    void MetalDescriptorSet::bindImmutableSamplers() const {
        for (auto &binding: setLayout->setBindings) {
            for (uint32_t i = 0; i < binding.immutableSamplers.size(); i++) {
                argumentBuffer.argumentEncoder->setSamplerState(binding.immutableSamplers[i], binding.binding + i);
            }
        }
    }

    void MetalDescriptorSet::commit() {
        if (needsCommit) {
            std::lock_guard lock(residencySetWriteMutex);
            residencySet->commit();
            needsCommit = false;
        }
    }

    void MetalDescriptorSet::setBuffer(const uint32_t descriptorIndex, const RenderBuffer *buffer, uint64_t bufferSize, const RenderBufferStructuredView *bufferStructuredView, const RenderBufferFormattedView *bufferFormattedView) {
        if (buffer == nullptr) {
            setDescriptor(descriptorIndex, nullptr);
            return;
        }

        const MetalBuffer *interfaceBuffer = static_cast<const MetalBuffer *>(buffer);

        if (bufferFormattedView != nullptr) {
            assert((bufferStructuredView == nullptr) && "Can't use structured views and formatted views at the same time.");

            const MetalBufferFormattedView *interfaceBufferFormattedView = static_cast<const MetalBufferFormattedView *>(bufferFormattedView);
            const TextureDescriptor descriptor = { .texture = interfaceBufferFormattedView->texture };
            setDescriptor(descriptorIndex, &descriptor);
        } else {
            uint32_t offset = 0;

            if (bufferStructuredView != nullptr) {
                assert((bufferFormattedView == nullptr) && "Can't use structured views and formatted views at the same time.");
                assert(bufferStructuredView->structureByteStride > 0);

                offset = bufferStructuredView->firstElement * bufferStructuredView->structureByteStride;
            }

            const BufferDescriptor descriptor = { .buffer = interfaceBuffer->mtl, .offset = offset };
            setDescriptor(descriptorIndex, &descriptor);
        }
    }

    void MetalDescriptorSet::setTexture(const uint32_t descriptorIndex, const RenderTexture *texture, RenderTextureLayout textureLayout, const RenderTextureView *textureView) {
        if (texture == nullptr) {
            setDescriptor(descriptorIndex, nullptr);
            return;
        }

        const MetalTexture *interfaceTexture = static_cast<const MetalTexture *>(texture);

        if (textureView != nullptr) {
            const MetalTextureView *interfaceTextureView = static_cast<const MetalTextureView *>(textureView);

            const TextureDescriptor descriptor = { .texture = interfaceTextureView->texture };
            setDescriptor(descriptorIndex, &descriptor);
        }
        else {
            const TextureDescriptor descriptor = { .texture = interfaceTexture->mtl };
            setDescriptor(descriptorIndex, &descriptor);
        }
    }

    void MetalDescriptorSet::setSampler(const uint32_t descriptorIndex, const RenderSampler *sampler) {
        if (sampler == nullptr) {
            setDescriptor(descriptorIndex, nullptr);
            return;
        }

        const MetalSampler *interfaceSampler = static_cast<const MetalSampler *>(sampler);
        const SamplerDescriptor descriptor = { .state = interfaceSampler->state };
        setDescriptor(descriptorIndex, &descriptor);
    }

    void MetalDescriptorSet::setAccelerationStructure(uint32_t descriptorIndex, const RenderAccelerationStructure *accelerationStructure) {
        // TODO: Unimplemented.
    }

    void MetalDescriptorSet::setDescriptor(const uint32_t descriptorIndex, const Descriptor *descriptor) {
        assert(descriptorIndex < setLayout->descriptorBindingIndices.size());

        const uint32_t indexBase = setLayout->descriptorIndexBases[descriptorIndex];
        const uint32_t bindingIndex = setLayout->descriptorBindingIndices[descriptorIndex];
        const auto &setLayoutBinding = setLayout->setBindings[indexBase];
        const MTL::DataType dtype = mapDataType(setLayoutBinding.descriptorType);
        MTL::Resource *nativeResource = nullptr;
        RenderDescriptorRangeType descriptorType = getDescriptorType(bindingIndex);

        if (dtype != MTL::DataTypeSampler) {
            if (resourceEntries[descriptorIndex].resource != nullptr) {
                if (residencySet != nullptr) {
                    std::lock_guard lock(residencySetWriteMutex);
                    residencySet->removeAllocation(resourceEntries[descriptorIndex].resource);
                    needsCommit = true;
                }
                resourceEntries[descriptorIndex].resource->release();
                resourceEntries[descriptorIndex].resource = nullptr;
            }
        }

        if (descriptor != nullptr) {
            switch (dtype) {
                case MTL::DataTypeTexture: {
                    const TextureDescriptor *textureDescriptor = static_cast<const TextureDescriptor *>(descriptor);
                    nativeResource = textureDescriptor->texture;
                    MTL::Texture *nativeTexture = static_cast<MTL::Texture *>(nativeResource);
                    if (residencySet != nullptr) {
                        std::lock_guard lock(residencySetWriteMutex);
                        residencySet->addAllocation(nativeTexture);
                        needsCommit = true;
                    }
                    argumentBuffer.argumentEncoder->setTexture(nativeTexture, descriptorIndex - indexBase + bindingIndex);
                    nativeTexture->retain();
                    break;
                }
                case MTL::DataTypePointer: {
                    const BufferDescriptor *bufferDescriptor = static_cast<const BufferDescriptor *>(descriptor);
                    nativeResource = bufferDescriptor->buffer;
                    MTL::Buffer *nativeBuffer = static_cast<MTL::Buffer *>(nativeResource);
                    if (residencySet != nullptr) {
                        std::lock_guard lock(residencySetWriteMutex);
                        residencySet->addAllocation(nativeBuffer);
                        needsCommit = true;
                    }
                    argumentBuffer.argumentEncoder->setBuffer(nativeBuffer, bufferDescriptor->offset, descriptorIndex - indexBase + bindingIndex);
                    nativeBuffer->retain();
                    break;
                }
                case MTL::DataTypeSampler: {
                    const SamplerDescriptor *samplerDescriptor = static_cast<const SamplerDescriptor *>(descriptor);
                    argumentBuffer.argumentEncoder->setSamplerState(samplerDescriptor->state, descriptorIndex - indexBase + bindingIndex);
                    break;
                }

                default:
                    assert(false && "Unsupported descriptor type.");
            }
        }

        if (argumentBuffer.mtl->storageMode() == MTL::StorageModeManaged) {
            argumentBuffer.mtl->didModifyRange(NS::Range(argumentBuffer.offset, argumentBuffer.mtl->length() - argumentBuffer.offset));
        }

        resourceEntries[descriptorIndex].resource = nativeResource;
        resourceEntries[descriptorIndex].type = descriptorType;
    }

    RenderDescriptorRangeType MetalDescriptorSet::getDescriptorType(const uint32_t binding) const {
        return setLayout->getBinding(binding)->descriptorType;
    }

    // MetalDrawable

    MetalDrawable::MetalDrawable(MetalDevice* device, MetalPool* pool, const RenderTextureDesc& desc) {
        assert(false && "MetalDrawable should not be constructed directly from device - use fromDrawable() instead");
    }

    MetalDrawable::~MetalDrawable() {
        mtl->release();
    }

    std::unique_ptr<RenderTextureView> MetalDrawable::createTextureView(const RenderTextureViewDesc& desc) const {
        assert(false && "Drawables don't support texture views");
        return nullptr;
    }

    void MetalDrawable::setName(const std::string &name) {
        mtl->texture()->setLabel(NS::String::string(name.c_str(), NS::UTF8StringEncoding));
    }

    // MetalSwapChain

    MetalSwapChain::MetalSwapChain(MetalCommandQueue *commandQueue, const RenderWindow renderWindow, uint32_t textureCount, const RenderFormat format, uint32_t maxFrameLatency) {
        this->layer = static_cast<CA::MetalLayer*>(renderWindow.view);
        layer->setDevice(commandQueue->device->mtl);
        layer->setPixelFormat(mapPixelFormat(format));

        this->commandQueue = commandQueue;
        this->maxFrameLatency = maxFrameLatency;

        // Metal supports a maximum of 3 drawables.
        this->drawables.resize(MAX_DRAWABLES);

        this->renderWindow = renderWindow;
        this->windowWrapper = std::make_unique<CocoaWindow>(renderWindow.window);
        getWindowSize(width, height);

        // Set the layer's drawable size to match the window size
        if (width > 0 && height > 0) {
            layer->setDrawableSize(CGSizeMake(width, height));
        }

        // set each of the drawable to have desc.flags = RenderTextureFlag::RENDER_TARGET;
        for (uint32_t i = 0; i < MAX_DRAWABLES; i++) {
            MetalDrawable &drawable = this->drawables[i];
            drawable.desc.width = width;
            drawable.desc.height = height;
            drawable.desc.format = format;
            drawable.desc.flags = RenderTextureFlag::RENDER_TARGET;
        }
    }

    MetalSwapChain::~MetalSwapChain() {
    }

    bool MetalSwapChain::present(const uint32_t textureIndex, RenderCommandSemaphore **waitSemaphores, const uint32_t waitSemaphoreCount) {
        assert(layer != nullptr && "Cannot present without a valid layer.");
        NS::AutoreleasePool *releasePool = NS::AutoreleasePool::alloc()->init();

        const MetalDrawable &drawable = drawables[textureIndex];
        assert(drawable.mtl != nullptr && "Cannot present without a valid drawable.");

        // Create a new command buffer just for presenting
        MTL::CommandBuffer *presentBuffer = commandQueue->mtl->commandBufferWithUnretainedReferences();
        presentBuffer->setLabel(MTLSTR("Present Command Buffer"));
        presentBuffer->enqueue();

        for (uint32_t i = 0; i < waitSemaphoreCount; i++) {
            MetalCommandSemaphore *interfaceSemaphore = static_cast<MetalCommandSemaphore *>(waitSemaphores[i]);
            presentBuffer->encodeWait(interfaceSemaphore->mtl, interfaceSemaphore->mtlEventValue++);
        }

        const uint64_t presentId = ++currentPresentId;

        // According to Apple, presenting via scheduled handler is more performant than using the presentDrawable method.
        // We grab the underlying drawable because we might've acquired a new one by now and the old one would have been released.
        CA::MetalDrawable *drawableMtl = drawable.mtl->retain();
        presentBuffer->addScheduledHandler([drawableMtl](MTL::CommandBuffer* cmdBuffer) {
            drawableMtl->present();
        });

        presentBuffer->addCompletedHandler([drawableMtl, presentId, this](MTL::CommandBuffer* cmdBuffer) {
            currentAvailableDrawableIndex = (currentAvailableDrawableIndex + 1) % MAX_DRAWABLES;
            drawableMtl->release();

            {
                std::lock_guard lock(lastPresentedIdMutex);
                lastPresentedId = std::max(lastPresentedId, presentId);
            }
            lastPresentedIdCondVar.notify_all();
        });

        presentBuffer->commit();

        releasePool->release();

        return true;
    }

    void MetalSwapChain::wait() {
        if (currentPresentId >= maxFrameLatency) {
            std::unique_lock lock(lastPresentedIdMutex);
            lastPresentedIdCondVar.wait_for(lock, std::chrono::seconds(1), [this] {
                return lastPresentedId >= currentPresentId - (maxFrameLatency - 1);
            });
        }
    }

    bool MetalSwapChain::resize() {
        getWindowSize(width, height);

        if (width == 0 || height == 0) {
            return false;
        }

        const CGSize drawableSize = CGSizeMake(width, height);
        if (const CGSize current = layer->drawableSize(); !CGSizeEqualToSize(current, drawableSize)) {
            layer->setDrawableSize(drawableSize);

            for (uint32_t i = 0; i < MAX_DRAWABLES; i++) {
                MetalDrawable &drawable = drawables[i];
                drawable.desc.width = width;
                drawable.desc.height = height;
            }
        }

        return true;
    }

    bool MetalSwapChain::needsResize() const {
        uint32_t windowWidth, windowHeight;
        getWindowSize(windowWidth, windowHeight);
        return (layer == nullptr) || (width != windowWidth) || (height != windowHeight);
    }

    void MetalSwapChain::setVsyncEnabled(const bool vsyncEnabled) {
        layer->setDisplaySyncEnabled(vsyncEnabled);
    }

    bool MetalSwapChain::isVsyncEnabled() const {
        return layer->displaySyncEnabled();
    }

    uint32_t MetalSwapChain::getWidth() const {
        return width;
    }

    uint32_t MetalSwapChain::getHeight() const {
        return height;
    }

    RenderTexture *MetalSwapChain::getTexture(const uint32_t textureIndex) {
        return &drawables[textureIndex];
    }

    bool MetalSwapChain::acquireTexture(RenderCommandSemaphore *signalSemaphore, uint32_t *textureIndex) {
        assert(signalSemaphore != nullptr);
        assert(textureIndex != nullptr);
        assert(*textureIndex < MAX_DRAWABLES);

        NS::AutoreleasePool *releasePool = NS::AutoreleasePool::alloc()->init();

        // Create a command buffer just to encode the signal
        MTL::CommandBuffer *acquireBuffer = commandQueue->mtl->commandBufferWithUnretainedReferences();
        acquireBuffer->setLabel(MTLSTR("Acquire Drawable Command Buffer"));
        const MetalCommandSemaphore *interfaceSemaphore = static_cast<MetalCommandSemaphore *>(signalSemaphore);
        acquireBuffer->enqueue();
        acquireBuffer->encodeSignalEvent(interfaceSemaphore->mtl, interfaceSemaphore->mtlEventValue);
        acquireBuffer->commit();

        CA::MetalDrawable *nextDrawable = layer->nextDrawable();
        if (nextDrawable == nullptr) {
            fprintf(stderr, "No more drawables available for rendering.\n");
            return false;
        }

        // Set the texture index and drawable data
        *textureIndex = currentAvailableDrawableIndex;
        MetalDrawable &drawable = drawables[currentAvailableDrawableIndex];
        drawable.desc.width = width;
        drawable.desc.height = height;
        drawable.desc.flags = RenderTextureFlag::RENDER_TARGET;
        drawable.desc.format = mapRenderFormat(nextDrawable->texture()->pixelFormat());
        if (drawable.mtl) {
            // Release our reference before replacing.
            drawable.mtl->release();
        }
        drawable.mtl = nextDrawable;

        drawable.mtl->retain();
        releasePool->release();

        return true;
    }

    uint32_t MetalSwapChain::getTextureCount() const {
        return MAX_DRAWABLES;
    }

    RenderWindow MetalSwapChain::getWindow() const {
        return renderWindow;
    }

    bool MetalSwapChain::isEmpty() const {
        return layer == nullptr || width == 0 || height == 0;
    }

    uint32_t MetalSwapChain::getRefreshRate() const {
        return windowWrapper->getRefreshRate();
    }

    void MetalSwapChain::getWindowSize(uint32_t &dstWidth, uint32_t &dstHeight) const {
        CocoaWindowAttributes attributes;
        windowWrapper->getWindowAttributes(&attributes);
        dstWidth = attributes.width;
        dstHeight = attributes.height;
    }

    // MetalAttachment

    MTL::Texture* MetalAttachment::getTexture() const {
        return textureView ? textureView->texture : texture->getTexture();
    }

    // MetalFramebuffer

    MetalFramebuffer::MetalFramebuffer(const MetalDevice *device, const RenderFramebufferDesc &desc) {
        assert(device != nullptr);
        NS::AutoreleasePool *releasePool = NS::AutoreleasePool::alloc()->init();

        colorAttachments.reserve(desc.colorAttachmentsCount);
        depthAttachmentReadOnly = desc.depthAttachmentReadOnly;

        const MetalTexture *firstTexture = nullptr;

        for (uint32_t i = 0; i < desc.colorAttachmentsCount; i++) {
            const MetalTextureView *colorAttachmentView = desc.colorAttachmentViews && desc.colorAttachmentViews[i] ? static_cast<const MetalTextureView *>(desc.colorAttachmentViews[i]) : nullptr;
            const MetalTexture *colorAttachment = colorAttachmentView ? colorAttachmentView->parentTexture : static_cast<const MetalTexture *>(desc.colorAttachments[i]);
            assert((colorAttachment->desc.flags & RenderTextureFlag::RENDER_TARGET) && "Color attachment must be a render target.");

            MetalAttachment attachment;
            attachment.texture = colorAttachment;
            attachment.textureView = colorAttachmentView;
            attachment.format = colorAttachmentView ? colorAttachmentView->desc.format : colorAttachment->desc.format;
            attachment.width = colorAttachment->desc.width;
            attachment.height = colorAttachment->desc.height;
            attachment.depth = colorAttachment->desc.depth;
            attachment.sampleCount = colorAttachment->desc.multisampling.sampleCount;
            colorAttachments.emplace_back(attachment);

            if (i == 0) {
                firstTexture = colorAttachment;
            }
        }

        if (desc.depthAttachment != nullptr || desc.depthAttachmentView != nullptr) {
            const MetalTextureView *depthAttachmentView = desc.depthAttachmentView ? static_cast<const MetalTextureView *>(desc.depthAttachmentView) : nullptr;
            const MetalTexture *interfaceDepthAttachment = depthAttachmentView ? depthAttachmentView->parentTexture : static_cast<const MetalTexture *>(desc.depthAttachment);
            assert((interfaceDepthAttachment->desc.flags & RenderTextureFlag::DEPTH_TARGET) && "Depth attachment must be a depth target.");

            depthAttachment.texture = interfaceDepthAttachment;
            depthAttachment.textureView = depthAttachmentView;
            depthAttachment.format = depthAttachmentView ? depthAttachmentView->desc.format : interfaceDepthAttachment->desc.format;
            depthAttachment.width = interfaceDepthAttachment->desc.width;
            depthAttachment.height = interfaceDepthAttachment->desc.height;
            depthAttachment.depth = interfaceDepthAttachment->desc.depth;
            depthAttachment.sampleCount = interfaceDepthAttachment->desc.multisampling.sampleCount;

            if (desc.colorAttachmentsCount == 0) {
                firstTexture = interfaceDepthAttachment;
            }
        }

        if (firstTexture) {
            width = firstTexture->desc.width;
            height = firstTexture->desc.height;

            sampleCount = firstTexture->desc.multisampling.sampleCount;
            if (firstTexture->desc.multisampling.sampleCount > 1) {
                for (uint32_t i = 0; i < firstTexture->desc.multisampling.sampleCount; i++) {
                    // Normalize from [-8, 7] to [0,1) range
                    float normalizedX = firstTexture->desc.multisampling.sampleLocations[i].x / 16.0f + 0.5f;
                    float normalizedY = firstTexture->desc.multisampling.sampleLocations[i].y / 16.0f + 0.5f;
                    samplePositions[i] = { normalizedX, normalizedY };
                }
            }
        }

        releasePool->release();
    }

    MetalFramebuffer::~MetalFramebuffer() {
        colorAttachments.clear();
    }

    uint32_t MetalFramebuffer::getWidth() const {
        return width;
    }

    uint32_t MetalFramebuffer::getHeight() const {
        return height;
    }

    // MetalQueryPool

    MetalQueryPool::MetalQueryPool(MetalDevice *device, uint32_t queryCount) {
        assert(device != nullptr);
        assert(queryCount > 0);

        this->device = device;

        MTL::CounterSampleBufferDescriptor *sampleBufferDesc = MTL::CounterSampleBufferDescriptor::alloc()->init();
        sampleBufferDesc->setCounterSet(device->timestampCounterSet);
        sampleBufferDesc->setStorageMode(MTL::StorageModeShared);
        sampleBufferDesc->setSampleCount(queryCount);
        sampleBuffer = device->mtl->newCounterSampleBuffer(sampleBufferDesc, nullptr);
        sampleBufferDesc->release();

        results.resize(queryCount, 0);
    }

    MetalQueryPool::~MetalQueryPool() {
        sampleBuffer->release();
    }

    void MetalQueryPool::queryResults() {
        NS::AutoreleasePool *releasePool = NS::AutoreleasePool::alloc()->init();

        const NS::Data* data = sampleBuffer->resolveCounterRange(NS::Range(0, results.size()));
        std::memcpy(results.data(), data->mutableBytes(), results.size() * sizeof(uint64_t));

        releasePool->release();
    }

    const uint64_t *MetalQueryPool::getResults() const {
        return results.data();
    }

    uint32_t MetalQueryPool::getCount() const {
        return uint32_t(results.size());
    }

    // MetalCommandList

    MetalCommandList::MetalCommandList(const MetalCommandQueue *queue) {
        this->device = queue->device;
        this->queue = queue;

        NS::AutoreleasePool *releasePool = NS::AutoreleasePool::alloc()->init();

        timestampQueryFence = device->mtl->newFence();
        timestampQueryFence->setLabel(MTLSTR("Timestamp Query Fence"));

        releasePool->release();
    }

    MetalCommandList::~MetalCommandList() {
        mtl->release();

        for (auto& fenceSet : fences) {
            for (auto* fence : fenceSet) {
                fence->release();
            }
        }

        timestampQueryFence->release();
    }

    void MetalCommandList::begin() {
        assert(mtl == nullptr);
        startedEncoding = false;
        mtl = queue->mtl->commandBufferWithUnretainedReferences();
        mtl->setLabel(MTLSTR("RT64 Command List"));

        // Reset fence waits and updates for new command list.
        fenceSlots.updateDirtyBits = ~0;
        for (uint32_t dstStage = 0; dstStage < MetalBarrierStage::COUNT; dstStage++) {
            for (uint32_t srcStage = 0; srcStage < MetalBarrierStage::COUNT; srcStage++) {
                fenceSlots.wait[dstStage][srcStage] = -1;
            }
            fenceSlots.update[dstStage] = -1;
        }
    }

    void MetalCommandList::end() {
        endActiveRenderEncoder();
        handlePendingClears();

        endActiveResolveTextureComputeEncoder();
        endActiveBlitEncoder();
        endActiveComputeEncoder();

        targetFramebuffer = nullptr;

        for (uint32_t i = 0; i < MAX_VERTEX_BUFFER_BINDINGS; i++) {
            vertexBuffers[i] = nullptr;
            vertexBufferOffsets[i] = 0;
        }
    }

    void MetalCommandList::commit() {
        mtl->commit();
        mtl->release();
        mtl = nullptr;
    }

    void MetalCommandList::barrierWait(MetalBarrierStage stage, MTL::RenderCommandEncoder* encoder) {
        constexpr uint32_t beforeStages = MTL::RenderStageVertex | MTL::RenderStageFragment;
        for (int i = 0; i < MetalBarrierStage::COUNT; ++i) {
            int &fenceIndex = fenceSlots.wait[stage][i];
            if (fenceIndex != -1) {
                const MTL::Fence* fence = fences[i][fenceIndex];
                encoder->waitForFence(fence, beforeStages);
                fenceIndex = -1;
            }
        }
    }

    void MetalCommandList::barrierWait(MetalBarrierStage stage, MTL::ComputeCommandEncoder* encoder) {
        for (int i = 0; i < MetalBarrierStage::COUNT; ++i) {
            int &fenceIndex = fenceSlots.wait[stage][i];
            if (fenceIndex != -1) {
                const MTL::Fence* fence = fences[i][fenceIndex];
                encoder->waitForFence(fence);
                fenceIndex = -1;
            }
        }
    }

    void MetalCommandList::barrierWait(MetalBarrierStage stage, MTL::BlitCommandEncoder* encoder) {
        for (int i = 0; i < MetalBarrierStage::COUNT; ++i) {
            int &fenceIndex = fenceSlots.wait[stage][i];
            if (fenceIndex != -1) {
                const MTL::Fence* fence = fences[i][fenceIndex];
                encoder->waitForFence(fence);
                fenceIndex = -1;
            }
        }
    }

    void MetalCommandList::barrierUpdate(MetalBarrierStage stage, MTL::RenderCommandEncoder* encoder) {
        constexpr uint32_t beforeStages = MTL::RenderStageVertex | MTL::RenderStageFragment;
        const MTL::Fence* fence = getBarrierStageFence(stage);
        if (fence != nullptr) {
            encoder->updateFence(fence, beforeStages);
        }
    }

    void MetalCommandList::barrierUpdate(MetalBarrierStage stage, MTL::ComputeCommandEncoder* encoder) {
        const MTL::Fence* fence = getBarrierStageFence(stage);
        if (fence != nullptr) {
            encoder->updateFence(fence);
        }
    }

    void MetalCommandList::barrierUpdate(MetalBarrierStage stage, MTL::BlitCommandEncoder* encoder) {
        const MTL::Fence* fence = getBarrierStageFence(stage);
        if (fence != nullptr) {
            encoder->updateFence(fence);
        }
    }

    MTL::Fence *MetalCommandList::getBarrierStageFence(MetalBarrierStage stage) {
        const uint32_t dirtyMask = 1 << stage;
        if ((fenceSlots.updateDirtyBits & dirtyMask) == dirtyMask) {
            fenceSlots.updateDirtyBits &= ~dirtyMask;
            ++fenceSlots.update[stage];

            if (fenceSlots.update[stage] >= fences[stage].size()) {
                MTL::Fence* fence = fences[stage].emplace_back(device->mtl->newFence());

                char label[100];
                snprintf(label, sizeof(label), "%s Fence %d", MetalBarrierStageName(stage).c_str(), fenceSlots.update[stage]);
                fence->setLabel(NS::String::string(label, NS::UTF8StringEncoding));
            }
        }

        if (fenceSlots.update[stage] < 0) {
            return nullptr;
        }

        return fences[stage][fenceSlots.update[stage]];
    }

    void MetalCommandList::setBarrier(uint64_t sourceStageMask, uint64_t destStageMask) {
        for (int i = 0; i < MetalBarrierStage::COUNT; ++i) {
            if ((sourceStageMask & (1 << i)) == 0) {
                continue;
            }

            for (int j = 0; j < MetalBarrierStage::COUNT; ++j) {
                if ((destStageMask & (1 << j)) == 0) {
                    continue;
                }

                fenceSlots.wait[j][i] = fenceSlots.update[i];
            }

            fenceSlots.wait[i][i] = fenceSlots.update[i];
            fenceSlots.updateDirtyBits |= 1 << i;
        }
    }

    void MetalCommandList::barriers(RenderBarrierStages stages, const RenderBufferBarrier *bufferBarriers, const uint32_t bufferBarriersCount, const RenderTextureBarrier *textureBarriers, const uint32_t textureBarriersCount) {
        assert(bufferBarriersCount == 0 || bufferBarriers != nullptr);
        assert(textureBarriersCount == 0 || textureBarriers != nullptr);

        if (bufferBarriersCount == 0 && textureBarriersCount == 0) {
            return;
        }

        uint64_t srcStageMask = 0;
        const uint64_t destStageMask = toStageMask(stages);

        for (int i = 0; i < bufferBarriersCount; i++) {
            const RenderBufferBarrier &bufferBarrier = bufferBarriers[i];
            MetalBuffer *interfaceBuffer = static_cast<MetalBuffer *>(bufferBarrier.buffer);

            srcStageMask |= toStageMask(interfaceBuffer->barrierStages);
            interfaceBuffer->barrierStages = stages;
        }

        for (int i = 0; i < textureBarriersCount; i++) {
            const RenderTextureBarrier &textureBarrier = textureBarriers[i];
            MetalTexture *interfaceTexture = static_cast<MetalTexture *>(textureBarrier.texture);

            srcStageMask |= toStageMask(interfaceTexture->barrierStages);
            interfaceTexture->barrierStages = stages;
            // Ignore layout transitions on Metal
            // interfaceTexture->layout = textureBarrier.layout;
        }

        // End passes on all barriers to finish work and update the appropriate barrier fences.
        endOtherEncoders(EncoderType::None);
        handlePendingClears();

        setBarrier(srcStageMask, destStageMask);
    }

    static uint64_t toStageMask(RenderBarrierStages stages) {
        uint64_t mask = 0;

        if (stages & RenderBarrierStage::GRAPHICS) {
            mask |= 1 << MetalBarrierStage::GRAPHICS;
        }

        if (stages & RenderBarrierStage::COMPUTE) {
            mask |= 1 << MetalBarrierStage::COMPUTE;
        }

        if (stages & RenderBarrierStage::COPY) {
            mask |= 1 << MetalBarrierStage::COPY;
        }

        return mask;
    }

    void MetalCommandList::dispatch(const uint32_t threadGroupCountX, const uint32_t threadGroupCountY, const uint32_t threadGroupCountZ) {
        checkActiveComputeEncoder();
        assert(activeComputeEncoder != nullptr && "Cannot encode dispatch on nullptr MTLComputeCommandEncoder!");
        assert(activeComputePipelineLayout != nullptr);

        const MTL::Size threadGroupCount = { threadGroupCountX, threadGroupCountY, threadGroupCountZ };
        const MTL::Size threadGroupSize = { activeComputeState->threadGroupSizeX, activeComputeState->threadGroupSizeY, activeComputeState->threadGroupSizeZ };
        activeComputeEncoder->dispatchThreadgroups(threadGroupCount, threadGroupSize);
    }

    void MetalCommandList::traceRays(uint32_t width, uint32_t height, uint32_t depth, RenderBufferReference shaderBindingTable, const RenderShaderBindingGroupsInfo &shaderBindingGroupsInfo) {
        // TODO: Support Metal RT
    }

    void MetalCommandList::prepareClearVertices(const RenderRect& rect, simd::float2* outVertices) {
        const float attWidth = static_cast<float>(targetFramebuffer->width);
        const float attHeight = static_cast<float>(targetFramebuffer->height);

        // Convert rect coordinates to normalized space (0 to 1)
        float leftPos = static_cast<float>(rect.left) / attWidth;
        float rightPos = static_cast<float>(rect.right) / attWidth;
        float topPos = static_cast<float>(rect.top) / attHeight;
        float bottomPos = static_cast<float>(rect.bottom) / attHeight;

        // Transform to clip space (-1 to 1)
        leftPos = (leftPos * 2.0f) - 1.0f;
        rightPos = (rightPos * 2.0f) - 1.0f;
        // Flip Y coordinates for Metal's coordinate system
        topPos = -(topPos * 2.0f - 1.0f);
        bottomPos = -(bottomPos * 2.0f - 1.0f);

        // Write vertices directly to the output array
        outVertices[0] = (simd::float2){ leftPos,  topPos };     // Top left
        outVertices[1] = (simd::float2){ leftPos,  bottomPos };  // Bottom left
        outVertices[2] = (simd::float2){ rightPos, bottomPos };  // Bottom right
        outVertices[3] = (simd::float2){ rightPos, bottomPos };  // Bottom right (repeated)
        outVertices[4] = (simd::float2){ rightPos, topPos };     // Top right
        outVertices[5] = (simd::float2){ leftPos,  topPos };     // Top left (repeated)
    }

    void MetalCommandList::drawInstanced(const uint32_t vertexCountPerInstance, const uint32_t instanceCount, const uint32_t startVertexLocation, const uint32_t startInstanceLocation) {
        assert(activeGraphicsPipelineLayout != nullptr);
        checkActiveRenderEncoder();
        checkForUpdatesInGraphicsState();

        activeRenderEncoder->drawPrimitives(activeRenderState->primitiveType, startVertexLocation, vertexCountPerInstance, instanceCount, startInstanceLocation);
    }

    void MetalCommandList::drawIndexedInstanced(const uint32_t indexCountPerInstance, const uint32_t instanceCount, const uint32_t startIndexLocation, const int32_t baseVertexLocation, const uint32_t startInstanceLocation) {
        assert(activeGraphicsPipelineLayout != nullptr);
        checkActiveRenderEncoder();
        checkForUpdatesInGraphicsState();

        activeRenderEncoder->drawIndexedPrimitives(currentPrimitiveType, indexCountPerInstance, currentIndexType, indexBuffer, indexBufferOffset + (startIndexLocation * indexTypeSize), instanceCount, baseVertexLocation, startInstanceLocation);
    }

    void MetalCommandList::setPipeline(const RenderPipeline *pipeline) {
        assert(pipeline != nullptr);

        const MetalPipeline *interfacePipeline = static_cast<const MetalPipeline *>(pipeline);
        switch (interfacePipeline->type) {
            case MetalPipeline::Type::Compute: {
                const MetalComputePipeline *computePipeline = static_cast<const MetalComputePipeline *>(interfacePipeline);
                if (activeComputeState != &computePipeline->state) {
                    activeComputeState = &computePipeline->state;
                    dirtyComputeState.pipelineState = 1;
                }
                break;
            }
            case MetalPipeline::Type::Graphics: {
                const MetalGraphicsPipeline *graphicsPipeline = static_cast<const MetalGraphicsPipeline *>(interfacePipeline);
                currentPrimitiveType = graphicsPipeline->state.primitiveType;
                if (activeRenderState != &graphicsPipeline->state) {
                    activeRenderState = &graphicsPipeline->state;
                    dirtyGraphicsState.pipelineState = 1;
                }
                break;
            }
            default:
                assert(false && "Unknown pipeline type.");
                break;
        }
    }

    void MetalCommandList::setComputePipelineLayout(const RenderPipelineLayout *pipelineLayout) {
        assert(pipelineLayout != nullptr);

        const MetalPipelineLayout *oldLayout = activeComputePipelineLayout;
        activeComputePipelineLayout = static_cast<const MetalPipelineLayout *>(pipelineLayout);

        if (oldLayout != activeComputePipelineLayout) {
            // Clear descriptor set bindings since they're no longer valid with the new layout
            for (uint32_t i = 0; i < MAX_DESCRIPTOR_SET_BINDINGS; i++) {
                computeDescriptorSets[i] = nullptr;
            }

            // Clear push constants since they might have different layouts/ranges
            pushConstants.clear();
            stateCache.lastPushConstants.clear();

            // Mark compute states as dirty that need to be rebound
            dirtyComputeState.descriptorSets = 1;
            dirtyComputeState.pushConstants = 1;
            dirtyComputeState.descriptorSetDirtyIndex = 0;
        }
    }

    void MetalCommandList::setComputePushConstants(uint32_t rangeIndex, const void *data, const uint32_t offset, const uint32_t size) {
        assert(activeComputePipelineLayout != nullptr);
        assert(rangeIndex < activeComputePipelineLayout->pushConstantRanges.size());

        const RenderPushConstantRange &range = activeComputePipelineLayout->pushConstantRanges[rangeIndex];
        assert(range.binding < MAX_PUSH_CONSTANT_BINDINGS && "Push constants out of range");

        pushConstants.resize(activeComputePipelineLayout->pushConstantRanges.size());
        pushConstants[rangeIndex].data.resize(range.size);
        memcpy(pushConstants[rangeIndex].data.data() + offset, data, size == 0 ? range.size : size);
        pushConstants[rangeIndex].binding = range.binding;
        pushConstants[rangeIndex].set = range.set;
        pushConstants[rangeIndex].offset = range.offset;
        pushConstants[rangeIndex].size = alignUp(range.size);
        pushConstants[rangeIndex].stageFlags = range.stageFlags;

        dirtyComputeState.pushConstants = 1;
    }

    void MetalCommandList::setComputeDescriptorSet(RenderDescriptorSet *descriptorSet, uint32_t setIndex) {
        assert(activeComputePipelineLayout != nullptr);
        assert(setIndex < MAX_DESCRIPTOR_SET_BINDINGS && "Descriptor set index out of range");

        MetalDescriptorSet *interfaceDescriptorSet = static_cast<MetalDescriptorSet*>(descriptorSet);
        if (computeDescriptorSets[setIndex] != interfaceDescriptorSet) {
            computeDescriptorSets[setIndex] = interfaceDescriptorSet;
            dirtyComputeState.descriptorSets = 1;
            dirtyComputeState.descriptorSetDirtyIndex = std::min(dirtyComputeState.descriptorSetDirtyIndex, setIndex);
        }
    }

    void MetalCommandList::setGraphicsPipelineLayout(const RenderPipelineLayout *pipelineLayout) {
        assert(pipelineLayout != nullptr);

        const MetalPipelineLayout *oldLayout = activeGraphicsPipelineLayout;
        activeGraphicsPipelineLayout = static_cast<const MetalPipelineLayout *>(pipelineLayout);

        if (oldLayout != activeGraphicsPipelineLayout) {
            // Clear descriptor set bindings since they're no longer valid with the new layout
            for (uint32_t i = 0; i < MAX_DESCRIPTOR_SET_BINDINGS; i++) {
                renderDescriptorSets[i] = nullptr;
            }

            // Clear push constants since they might have different layouts/ranges
            pushConstants.clear();
            stateCache.lastPushConstants.clear();

            // Mark graphics states as dirty that need to be rebound
            dirtyGraphicsState.descriptorSets = 1;
            dirtyGraphicsState.pushConstants = 1;
            dirtyGraphicsState.descriptorSetDirtyIndex = 0;
        }
    }

    void MetalCommandList::setGraphicsPushConstants(const uint32_t rangeIndex, const void *data, const uint32_t offset, const uint32_t size) {
        assert(activeGraphicsPipelineLayout != nullptr);
        assert(rangeIndex < activeGraphicsPipelineLayout->pushConstantRanges.size());

        const RenderPushConstantRange &range = activeGraphicsPipelineLayout->pushConstantRanges[rangeIndex];
        assert(range.binding < MAX_PUSH_CONSTANT_BINDINGS && "Push constants out of range");

        pushConstants.resize(activeGraphicsPipelineLayout->pushConstantRanges.size());
        pushConstants[rangeIndex].data.resize(range.size);
        memcpy(pushConstants[rangeIndex].data.data() + offset, data, size == 0 ? range.size : size);
        pushConstants[rangeIndex].binding = range.binding;
        pushConstants[rangeIndex].set = range.set;
        pushConstants[rangeIndex].offset = range.offset;
        pushConstants[rangeIndex].size = alignUp(range.size);
        pushConstants[rangeIndex].stageFlags = range.stageFlags;

        dirtyGraphicsState.pushConstants = 1;
    }

    void MetalCommandList::setGraphicsDescriptorSet(RenderDescriptorSet *descriptorSet, uint32_t setIndex) {
        assert(activeGraphicsPipelineLayout != nullptr);
        assert(setIndex < MAX_DESCRIPTOR_SET_BINDINGS && "Descriptor set index out of range");

        MetalDescriptorSet *interfaceDescriptorSet = static_cast<MetalDescriptorSet*>(descriptorSet);
        if (renderDescriptorSets[setIndex] != interfaceDescriptorSet) {
            renderDescriptorSets[setIndex] = interfaceDescriptorSet;
            dirtyGraphicsState.descriptorSets = 1;
            dirtyGraphicsState.descriptorSetDirtyIndex = std::min(dirtyGraphicsState.descriptorSetDirtyIndex, setIndex);
        }
    }

    void MetalCommandList::setGraphicsRootDescriptor(RenderBufferReference bufferReference, uint32_t rootDescriptorIndex) {
        assert(false && "Root descriptors are not supported in Metal.");
    }

    void MetalCommandList::setRaytracingPipelineLayout(const RenderPipelineLayout *pipelineLayout) {
        // TODO: Metal RT
    }

    void MetalCommandList::setRaytracingPushConstants(uint32_t rangeIndex, const void *data, uint32_t offset, uint32_t size) {
        // TODO: Metal RT
    }

    void MetalCommandList::setRaytracingDescriptorSet(RenderDescriptorSet *descriptorSet, uint32_t setIndex) {
        // TODO: Metal RT
    }

    void MetalCommandList::setIndexBuffer(const RenderIndexBufferView *view) {
        if (view != nullptr) {
            const MetalBuffer *interfaceBuffer = static_cast<const MetalBuffer *>(view->buffer.ref);
            indexBuffer = interfaceBuffer->mtl;
            indexBufferOffset = view->buffer.offset;
            currentIndexType = mapIndexFormat(view->format);
            indexTypeSize = currentIndexType == MTL::IndexTypeUInt32 ? sizeof(uint32_t) : sizeof(uint16_t);
        }
    }

    void MetalCommandList::setVertexBuffers(const uint32_t startSlot, const RenderVertexBufferView *views, const uint32_t viewCount, const RenderInputSlot *inputSlots) {
        if ((views != nullptr) && (viewCount > 0)) {
            assert(inputSlots != nullptr);
            assert(startSlot + viewCount <= MAX_VERTEX_BUFFER_BINDINGS && "Vertex buffer out of range");

            // Check for changes in bindings
            for (uint32_t i = 0; i < viewCount; i++) {
                const MetalBuffer* interfaceBuffer = static_cast<const MetalBuffer*>(views[i].buffer.ref);
                uint64_t newOffset = views[i].buffer.offset;

                if (interfaceBuffer == nullptr) {
                    interfaceBuffer = static_cast<const MetalBuffer*>(queue->device->nullBuffer.get());
                    newOffset = 0;
                }

                const uint32_t bufferIndex = startSlot + i;
                vertexBuffers[bufferIndex] = interfaceBuffer->mtl;
                vertexBufferOffsets[bufferIndex] = newOffset;
                dirtyGraphicsState.vertexBufferSlots |= 1 << bufferIndex;
            }
        }
    }

    void MetalCommandList::setViewports(const RenderViewport *viewports, const uint32_t count) {
        viewportVector.resize(count);

        for (uint32_t i = 0; i < count; i++) {
            const MTL::Viewport viewport { viewports[i].x, viewports[i].y, viewports[i].width, viewports[i].height, viewports[i].minDepth, viewports[i].maxDepth };
            viewportVector[i] = viewport;
        }

        // Since viewports are set at the encoder level, we mark it as dirty so it'll be updated on next active encoder check
        if (viewportVector != stateCache.lastViewports) {
            dirtyGraphicsState.viewports = 1;
        }
    }

    void MetalCommandList::setScissors(const RenderRect *scissorRects, const uint32_t count) {
        scissorVector.resize(count);

        for (uint32_t i = 0; i < count; i++) {
            scissorVector[i] = clampScissorRectIfNecessary(scissorRects[i], targetFramebuffer);
        }

        // Since scissors are set at the encoder level, we mark it as dirty so it'll be updated on next active encoder check
        if (scissorVector != stateCache.lastScissors) {
            dirtyGraphicsState.scissors = 1;
        }
    }

    void MetalCommandList::setFramebuffer(const RenderFramebuffer *framebuffer) {
        endOtherEncoders(EncoderType::Render);
        endActiveRenderEncoder();
        handlePendingClears();
        activeType = EncoderType::Render;

        if (framebuffer != nullptr) {
            const MetalFramebuffer *interfaceFramebuffer = static_cast<const MetalFramebuffer*>(framebuffer);
            targetFramebuffer = interfaceFramebuffer;
            dirtyGraphicsState.setAll();

            // Initialize pending clears
            pendingClears.initialAction.clear();
            pendingClears.clearValues.clear();
            pendingClears.active = false;

            // Resize for color attachments (1 for depth, 1 for stencil)
            const size_t totalAttachments = interfaceFramebuffer->colorAttachments.size() + 2;
            pendingClears.initialAction.resize(totalAttachments, MTL::LoadActionLoad);
            pendingClears.clearValues.resize(totalAttachments);
        } else {
            targetFramebuffer = nullptr;
        }
    }

    void MetalCommandList::setDepthBias(float depthBias, float depthBiasClamp, float slopeScaledDepthBias) {
        dynamicDepthBias.depthBias = depthBias;
        dynamicDepthBias.depthBiasClamp = depthBiasClamp;
        dynamicDepthBias.slopeScaledDepthBias = slopeScaledDepthBias;
    }

    void MetalCommandList::setCommonClearState() const {
        activeRenderEncoder->setViewport({ 0, 0, static_cast<float>(targetFramebuffer->width), static_cast<float>(targetFramebuffer->height), 0.0f, 1.0f });
        activeRenderEncoder->setScissorRect(clampScissorRectIfNecessary({ 0, 0, static_cast<int32_t>(targetFramebuffer->width), static_cast<int32_t>(targetFramebuffer->height) }, targetFramebuffer));
        activeRenderEncoder->setTriangleFillMode(MTL::TriangleFillModeFill);
        activeRenderEncoder->setCullMode(MTL::CullModeNone);
        activeRenderEncoder->setDepthBias(0.0f, 0.0f, 0.0f);
    }

    void MetalCommandList::handlePendingClears() {
        if (!pendingClears.active) {
            return;
        }
        
        checkActiveRenderEncoder();
        endActiveRenderEncoder();
    }

    void MetalCommandList::clearColor(const uint32_t attachmentIndex, RenderColor colorValue, const RenderRect *clearRects, const uint32_t clearRectsCount) {
        assert(targetFramebuffer != nullptr);
        assert(attachmentIndex < targetFramebuffer->colorAttachments.size());
        assert((!clearRects || clearRectsCount <= MAX_CLEAR_RECTS) && "Too many clear rects");
        
        // For full framebuffer clears, use the more efficient load action clear
        if (clearRectsCount == 0) {
            pendingClears.initialAction[attachmentIndex] = MTL::LoadActionClear;
            pendingClears.clearValues[attachmentIndex].color = colorValue;
            pendingClears.active = true;
            return;
        }

        // For partial clears, do our own quad-based clear
        checkActiveRenderEncoder();

        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

        // Store state cache
        const auto previousCache = stateCache;

        // Process clears
        activeRenderEncoder->pushDebugGroup(MTLSTR("ColorClear"));

        MTL::RenderPipelineDescriptor* pipelineDesc = MTL::RenderPipelineDescriptor::alloc()->init();
        pipelineDesc->setVertexFunction(device->clearVertexFunction);
        pipelineDesc->setFragmentFunction(device->clearColorFunction);
        pipelineDesc->setRasterSampleCount(targetFramebuffer->colorAttachments[attachmentIndex].sampleCount);

        MTL::RenderPipelineColorAttachmentDescriptor *pipelineColorAttachment = pipelineDesc->colorAttachments()->object(attachmentIndex);
        pipelineColorAttachment->setPixelFormat(targetFramebuffer->colorAttachments[attachmentIndex].getTexture()->pixelFormat());
        pipelineColorAttachment->setBlendingEnabled(false);

        // Set pixel format for depth attachment if we have one, with write disabled
        if (targetFramebuffer->depthAttachment.format != RenderFormat::UNKNOWN) {
            pipelineDesc->setDepthAttachmentPixelFormat(targetFramebuffer->depthAttachment.getTexture()->pixelFormat());
            if (RenderFormatIsStencil(targetFramebuffer->depthAttachment.format)) {
                pipelineDesc->setStencilAttachmentPixelFormat(pipelineDesc->depthAttachmentPixelFormat());
            }
            MTL::DepthStencilDescriptor *depthStencilDescriptor = MTL::DepthStencilDescriptor::alloc()->init();
            depthStencilDescriptor->setDepthWriteEnabled(false);
            const MTL::DepthStencilState *depthStencilState = device->mtl->newDepthStencilState(depthStencilDescriptor);
            activeRenderEncoder->setDepthStencilState(depthStencilState);

            depthStencilDescriptor->release();
        }

        const MTL::RenderPipelineState *pipelineState = device->getOrCreateClearRenderPipelineState(pipelineDesc);
        activeRenderEncoder->setRenderPipelineState(pipelineState);

        setCommonClearState();
        pipelineDesc->release();

        // Generate vertices for each rect
        const uint32_t rectCount = clearRectsCount > 0 ? clearRectsCount : 1;
        const size_t totalVertices = 6 * rectCount;  // 6 vertices per rect

        thread_local std::vector<simd::float2> allVertices;
        allVertices.resize(totalVertices);

        if (clearRectsCount > 0) {
            // Process each clear rect
            for (uint32_t j = 0; j < clearRectsCount; j++) {
                prepareClearVertices(clearRects[j], allVertices.data() + (j * 6));
            }
        } else {
            // Full screen clear
            const RenderRect fullRect = { 0, 0, static_cast<int32_t>(targetFramebuffer->width), static_cast<int32_t>(targetFramebuffer->height) };
            prepareClearVertices(fullRect, allVertices.data());
        }

        // Set vertices
        activeRenderEncoder->setVertexBytes(allVertices.data(), allVertices.size() * sizeof(simd::float2), 0);

        // Use stack for clear colors too since we know the max size
        simd::float4 clearColors[MAX_CLEAR_RECTS];
        for (size_t j = 0; j < rectCount; j++) {
            clearColors[j] = (simd::float4){ colorValue.r, colorValue.g, colorValue.b, colorValue.a };
        }
        activeRenderEncoder->setFragmentBytes(clearColors, alignUp(sizeof(simd::float4) * rectCount), 0);

        activeRenderEncoder->drawPrimitives(MTL::PrimitiveTypeTriangle, 0.0, 6 * rectCount);

        activeRenderEncoder->popDebugGroup();

        // Restore previous state if we had one
        stateCache = previousCache;
        dirtyGraphicsState.setAll();

        pool->release();
    }

    void MetalCommandList::clearDepthStencil(const bool clearDepth, const bool clearStencil, const float depthValue, const uint32_t stencilValue, const RenderRect *clearRects, const uint32_t clearRectsCount) {
        assert(targetFramebuffer != nullptr);
        assert(targetFramebuffer->depthAttachment.format != RenderFormat::UNKNOWN);
        assert((!clearRects || clearRectsCount <= MAX_CLEAR_RECTS) && "Too many clear rects");

        if (clearDepth || clearStencil) {
            // For full framebuffer clears, use the more efficient load action clear
            if (clearRectsCount == 0) {
                const size_t depthIndex = targetFramebuffer->colorAttachments.size();
                
                if (clearDepth) {
                    pendingClears.initialAction[depthIndex] = MTL::LoadActionClear;
                    pendingClears.clearValues[depthIndex].depth = depthValue;
                }
                
                if (clearStencil) {
                    pendingClears.initialAction[depthIndex + 1] = MTL::LoadActionClear;
                    pendingClears.clearValues[depthIndex + 1].stencil = stencilValue;
                }
                
                pendingClears.active = true;
                return;
            }

            // For partial clears, do our own quad-based clear
            checkActiveRenderEncoder();

            NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

            // Store state cache
            auto previousCache = stateCache;

            // Process clears
            activeRenderEncoder->pushDebugGroup(MTLSTR("DepthClear"));

            MTL::RenderPipelineDescriptor* pipelineDesc = MTL::RenderPipelineDescriptor::alloc()->init();
            pipelineDesc->setVertexFunction(device->clearVertexFunction);
            pipelineDesc->setFragmentFunction(device->clearDepthFunction);
            pipelineDesc->setDepthAttachmentPixelFormat(targetFramebuffer->depthAttachment.getTexture()->pixelFormat());
            if (RenderFormatIsStencil(targetFramebuffer->depthAttachment.format)) {
                pipelineDesc->setStencilAttachmentPixelFormat(pipelineDesc->depthAttachmentPixelFormat());
            }
            pipelineDesc->setRasterSampleCount(targetFramebuffer->depthAttachment.sampleCount);

            // Set color attachment pixel formats with write disabled
            for (uint32_t j = 0; j < targetFramebuffer->colorAttachments.size(); j++) {
                MTL::RenderPipelineColorAttachmentDescriptor *pipelineColorAttachment = pipelineDesc->colorAttachments()->object(j);
                pipelineColorAttachment->setPixelFormat(targetFramebuffer->colorAttachments[j].getTexture()->pixelFormat());
                pipelineColorAttachment->setWriteMask(MTL::ColorWriteMaskNone);
            }

            const MTL::RenderPipelineState *pipelineState = device->getOrCreateClearRenderPipelineState(pipelineDesc, clearDepth, clearStencil);
            activeRenderEncoder->setRenderPipelineState(pipelineState);
            if (clearDepth && clearStencil) {
                activeRenderEncoder->setDepthStencilState(device->clearDepthStencilState);
            } else if (clearDepth) {
                activeRenderEncoder->setDepthStencilState(device->clearDepthState);
            } else {
                activeRenderEncoder->setDepthStencilState(device->clearStencilState);
            }

            setCommonClearState();
            pipelineDesc->release();

            // Generate vertices for each rect
            const uint32_t rectCount = clearRectsCount > 0 ? clearRectsCount : 1;
            const size_t totalVertices = 6 * rectCount;  // 6 vertices per rect

            thread_local std::vector<simd::float2> allVertices;
            allVertices.resize(totalVertices);

            if (clearRectsCount > 0) {
                // Process each clear rect
                for (uint32_t j = 0; j < clearRectsCount; j++) {
                    prepareClearVertices(clearRects[j], allVertices.data() + (j * 6));
                }
            } else {
                // Full screen clear
                const RenderRect fullRect = { 0, 0, static_cast<int32_t>(targetFramebuffer->width), static_cast<int32_t>(targetFramebuffer->height) };
                prepareClearVertices(fullRect, allVertices.data());
            }

            // Set vertices
            activeRenderEncoder->setVertexBytes(allVertices.data(), allVertices.size() * sizeof(simd::float2), 0);

            float clearDepths[MAX_CLEAR_RECTS];
            for (size_t j = 0; j < rectCount; j++) {
                clearDepths[j] = depthValue;
            }
            activeRenderEncoder->setFragmentBytes(clearDepths, alignUp(sizeof(float) * rectCount), 0);

            activeRenderEncoder->drawPrimitives(MTL::PrimitiveTypeTriangle, 0.0, 6 * rectCount);

            activeRenderEncoder->popDebugGroup();

            // Restore previous state if we had one
            stateCache = previousCache;
            dirtyGraphicsState.setAll();

            pool->release();
        }
    }

    void MetalCommandList::copyBufferRegion(const RenderBufferReference dstBuffer, const RenderBufferReference srcBuffer, const uint64_t size) {
        assert(dstBuffer.ref != nullptr);
        assert(srcBuffer.ref != nullptr);

        endOtherEncoders(EncoderType::Blit);
        checkActiveBlitEncoder();
        activeType = EncoderType::Blit;

        const MetalBuffer *interfaceDstBuffer = static_cast<const MetalBuffer *>(dstBuffer.ref);
        const MetalBuffer *interfaceSrcBuffer = static_cast<const MetalBuffer *>(srcBuffer.ref);

        activeBlitEncoder->copyFromBuffer(interfaceSrcBuffer->mtl, srcBuffer.offset, interfaceDstBuffer->mtl, dstBuffer.offset, size);
    }

    void MetalCommandList::copyTextureRegion(const RenderTextureCopyLocation &dstLocation, const RenderTextureCopyLocation &srcLocation, const uint32_t dstX, const uint32_t dstY, const uint32_t dstZ, const RenderBox *srcBox) {
        assert(dstLocation.type != RenderTextureCopyType::UNKNOWN);
        assert(srcLocation.type != RenderTextureCopyType::UNKNOWN);

        endOtherEncoders(EncoderType::Blit);
        checkActiveBlitEncoder();
        activeType = EncoderType::Blit;

        const MetalTexture *dstTexture = static_cast<const MetalTexture *>(dstLocation.texture);
        const MetalTexture *srcTexture = static_cast<const MetalTexture *>(srcLocation.texture);
        const MetalBuffer *dstBuffer = static_cast<const MetalBuffer *>(dstLocation.buffer);
        const MetalBuffer *srcBuffer = static_cast<const MetalBuffer *>(srcLocation.buffer);

        if (dstLocation.type == RenderTextureCopyType::SUBRESOURCE && srcLocation.type == RenderTextureCopyType::PLACED_FOOTPRINT) {
            assert(dstTexture != nullptr);
            assert(srcBuffer != nullptr);

            // Calculate block size based on destination texture format
            const uint32_t blockWidth = RenderFormatBlockWidth(dstTexture->desc.format);

            // Use actual dimensions for the copy size
            const MTL::Size size = { srcLocation.placedFootprint.width, srcLocation.placedFootprint.height, srcLocation.placedFootprint.depth};

            const uint32_t horizontalBlocks = (srcLocation.placedFootprint.rowWidth + blockWidth - 1) / blockWidth;
            const uint32_t verticalBlocks = (srcLocation.placedFootprint.height + blockWidth - 1) / blockWidth;
            const uint32_t bytesPerRow = horizontalBlocks * RenderFormatSize(dstTexture->desc.format);
            const uint32_t bytesPerImage = bytesPerRow * verticalBlocks;

            const MTL::Origin dstOrigin = { dstX, dstY, dstZ };

            activeBlitEncoder->pushDebugGroup(MTLSTR("CopyTextureRegion"));
            activeBlitEncoder->copyFromBuffer(
                srcBuffer->mtl,
                srcLocation.placedFootprint.offset,
                bytesPerRow,
                bytesPerImage,
                size,
                dstTexture->mtl,
                dstLocation.subresource.arrayIndex,
                dstLocation.subresource.mipLevel,
                dstOrigin
            );
            activeBlitEncoder->popDebugGroup();
        } else {
            assert(dstTexture != nullptr);
            assert(srcTexture != nullptr);

            MTL::Origin srcOrigin;
            MTL::Size size;

            if (srcBox != nullptr) {
                srcOrigin = { NS::UInteger(srcBox->left), NS::UInteger(srcBox->top), NS::UInteger(srcBox->front) };
                size = { NS::UInteger(srcBox->right - srcBox->left), NS::UInteger(srcBox->bottom - srcBox->top), NS::UInteger(srcBox->back - srcBox->front) };
            } else {
                srcOrigin = { 0, 0, 0 };
                size = { srcTexture->desc.width, srcTexture->desc.height, srcTexture->desc.depth };
            }

            const MTL::Origin dstOrigin = { dstX, dstY, dstZ };

            activeBlitEncoder->copyFromTexture(
                srcTexture->mtl,                    // source texture
                srcLocation.subresource.arrayIndex, // source slice (baseArrayLayer)
                srcLocation.subresource.mipLevel,   // source mipmap level
                srcOrigin,                          // source origin
                size,                               // copy size
                dstTexture->mtl,                    // destination texture
                dstLocation.subresource.arrayIndex, // destination slice (baseArrayLayer)
                dstLocation.subresource.mipLevel,   // destination mipmap level
                dstOrigin                           // destination origin
            );
          }
    }

    void MetalCommandList::copyBuffer(const RenderBuffer *dstBuffer, const RenderBuffer *srcBuffer) {
        assert(dstBuffer != nullptr);
        assert(srcBuffer != nullptr);

        endOtherEncoders(EncoderType::Blit);
        checkActiveBlitEncoder();
        activeType = EncoderType::Blit;

        const MetalBuffer *dst = static_cast<const MetalBuffer *>(dstBuffer);
        const MetalBuffer *src = static_cast<const MetalBuffer *>(srcBuffer);

        activeBlitEncoder->pushDebugGroup(MTLSTR("CopyBuffer"));
        activeBlitEncoder->copyFromBuffer(src->mtl, 0, dst->mtl, 0, dst->desc.size);
        activeBlitEncoder->popDebugGroup();
    }

    void MetalCommandList::copyTexture(const RenderTexture *dstTexture, const RenderTexture *srcTexture) {
        assert(dstTexture != nullptr);
        assert(srcTexture != nullptr);

        endOtherEncoders(EncoderType::Blit);
        checkActiveBlitEncoder();
        activeType = EncoderType::Blit;

        const MetalTexture *dst = static_cast<const MetalTexture *>(dstTexture);
        const MetalTexture *src = static_cast<const MetalTexture *>(srcTexture);

        activeBlitEncoder->copyFromTexture(src->mtl, dst->mtl);
    }

    void MetalCommandList::resolveTexture(const RenderTexture *dstTexture, const RenderTexture *srcTexture) {
        assert(dstTexture != nullptr);
        assert(srcTexture != nullptr);

        const MetalTexture *dst = static_cast<const MetalTexture *>(dstTexture);
        const MetalTexture *src = static_cast<const MetalTexture *>(srcTexture);

        // For full texture resolves, use the more efficient render pass resolve
        endOtherEncoders(EncoderType::Render);
        endActiveRenderEncoder();
        handlePendingClears();
        activeType = EncoderType::Render;

        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

        const MTL::RenderPassDescriptor *renderPassDescriptor = MTL::RenderPassDescriptor::renderPassDescriptor();
        MTL::RenderPassColorAttachmentDescriptor *colorAttachment = renderPassDescriptor->colorAttachments()->object(0);

        colorAttachment->setTexture(src->mtl);
        colorAttachment->setResolveTexture(dst->mtl);
        colorAttachment->setLoadAction(MTL::LoadActionLoad);
        colorAttachment->setStoreAction(MTL::StoreActionMultisampleResolve);

        MTL::RenderCommandEncoder *encoder = mtl->renderCommandEncoder(renderPassDescriptor);
        encoder->setLabel(MTLSTR("Resolve Texture Encoder"));
        encoder->endEncoding();

        pool->release();
    }

    void MetalCommandList::resolveTextureRegion(const RenderTexture *dstTexture, const uint32_t dstX, const uint32_t dstY, const RenderTexture *srcTexture, const RenderRect *srcRect, RenderResolveMode resolveMode) {
        assert(dstTexture != nullptr);
        assert(srcTexture != nullptr);
        assert(resolveMode == RenderResolveMode::AVERAGE && "Metal currently only supports AVERAGE resolve mode.");

        const MetalTexture *dst = static_cast<const MetalTexture *>(dstTexture);
        const MetalTexture *src = static_cast<const MetalTexture *>(srcTexture);

        assert(dst->mtl->usage() & MTL::TextureUsageShaderWrite);

        // Check if we can use full texture resolve
        const bool canUseFullResolve =
        (dst->desc.width == src->desc.width) &&
        (dst->desc.height == src->desc.height) &&
        (dstX == 0) && (dstY == 0) &&
        (srcRect == nullptr ||
         (srcRect->left == 0 &&
          srcRect->top == 0 &&
          static_cast<uint32_t>(srcRect->right) == src->desc.width &&
          static_cast<uint32_t>(srcRect->bottom) == src->desc.height));

        if (canUseFullResolve) {
            resolveTexture(dstTexture, srcTexture);
            return;
        }

        endOtherEncoders(EncoderType::Resolve);
        checkActiveResolveTextureComputeEncoder();
        activeType = EncoderType::Resolve;

        // Calculate source region
        uint32_t srcX = 0;
        uint32_t srcY = 0;
        uint32_t width = src->mtl->width();
        uint32_t height = src->mtl->height();

        if (srcRect != nullptr) {
            srcX = srcRect->left;
            srcY = srcRect->top;
            width = srcRect->right - srcRect->left;
            height = srcRect->bottom - srcRect->top;
        }

        // Setup resolve parameters
        const struct ResolveParams {
            uint32_t dstOffsetX;
            uint32_t dstOffsetY;
            uint32_t srcOffsetX;
            uint32_t srcOffsetY;
            uint32_t resolveSizeX;
            uint32_t resolveSizeY;
        } params = {
            dstX, dstY,
            srcX, srcY,
            width, height
        };

        activeResolveComputeEncoder->setTexture(src->mtl, 0);
        activeResolveComputeEncoder->setTexture(dst->mtl, 1);
        activeResolveComputeEncoder->setBytes(&params, sizeof(params), 0);

        const MTL::Size threadGroupSize = { 8, 8, 1 };
        const NS::UInteger groupSizeX = (width + threadGroupSize.width - 1) / threadGroupSize.width;
        const NS::UInteger groupSizeY = (height + threadGroupSize.height - 1) / threadGroupSize.height;
        const MTL::Size gridSize = { groupSizeX, groupSizeY, 1 };
        activeResolveComputeEncoder->dispatchThreadgroups(gridSize, threadGroupSize);
    }

    void MetalCommandList::buildBottomLevelAS(const RenderAccelerationStructure *dstAccelerationStructure, RenderBufferReference scratchBuffer, const RenderBottomLevelASBuildInfo &buildInfo) {
        // TODO: Unimplemented.
    }

    void MetalCommandList::buildTopLevelAS(const RenderAccelerationStructure *dstAccelerationStructure, RenderBufferReference scratchBuffer, RenderBufferReference instancesBuffer, const RenderTopLevelASBuildInfo &buildInfo) {
        // TODO: Unimplemented.
    }

    void MetalCommandList::discardTexture(const RenderTexture* texture) {
        // Not required in Metal.
    }

    void MetalCommandList::resetQueryPool(const RenderQueryPool *queryPool, uint32_t queryFirstIndex, uint32_t queryCount) {
        assert(queryPool != nullptr);
        // No-op
    }

    void MetalCommandList::writeTimestamp(const RenderQueryPool *queryPool, uint32_t queryIndex) {
        assert(queryPool != nullptr);

        const MetalQueryPool *interfaceQueryPool = static_cast<const MetalQueryPool *>(queryPool);

        MTL::ComputeCommandEncoder *computeEncoder = activeComputeEncoder != nullptr ? activeComputeEncoder : activeResolveComputeEncoder;
        if (computeEncoder != nullptr && device->mtl->supportsCounterSampling(MTL::CounterSamplingPointAtDispatchBoundary)) {
            computeEncoder->sampleCountersInBuffer(interfaceQueryPool->sampleBuffer, queryIndex, true);
        } else if (activeRenderEncoder != nullptr && device->mtl->supportsCounterSampling(MTL::CounterSamplingPointAtDrawBoundary)) {
            activeRenderEncoder->sampleCountersInBuffer(interfaceQueryPool->sampleBuffer, queryIndex, true);
        } else if (device->mtl->supportsCounterSampling(MTL::CounterSamplingPointAtBlitBoundary)) {
            // If we are here because dispatch/draw sampling is not supported, end the
            // current encoder and ensure we have a blit encoder for sampling.
            endOtherEncoders(EncoderType::Blit);
            checkActiveBlitEncoder();

            activeBlitEncoder->sampleCountersInBuffer(interfaceQueryPool->sampleBuffer, queryIndex, true);
        } else if (device->mtl->supportsCounterSampling(MTL::CounterSamplingPointAtStageBoundary)) {
            // If the device does not support the required sampling point, but supports sampling at stage boundaries
            // (e.g. Apple GPUs), we need to use a dummy blit encoder configured to sample to the buffer.
            endOtherEncoders(EncoderType::None);

            NS::AutoreleasePool *releasePool = NS::AutoreleasePool::alloc()->init();

            MTL::BlitPassDescriptor *descriptor = MTL::BlitPassDescriptor::alloc()->init();
            MTL::BlitPassSampleBufferAttachmentDescriptor *sampleDescriptor = descriptor->sampleBufferAttachments()->object(0);
            sampleDescriptor->setSampleBuffer(interfaceQueryPool->sampleBuffer);
            sampleDescriptor->setStartOfEncoderSampleIndex(queryIndex);
            sampleDescriptor->setEndOfEncoderSampleIndex(queryIndex);

            MTL::BlitCommandEncoder *encoder = mtl->blitCommandEncoder(descriptor);
            encoder->setLabel(MTLSTR("Timestamp Query Encoder"));
            descriptor->release();

            // Wait for query fence and perform dummy operation to record timestamp.
            const MetalBuffer* nullBuffer = static_cast<const MetalBuffer *>(device->nullBuffer.get());
            if (startedEncoding) {
                encoder->waitForFence(timestampQueryFence);
            }
            encoder->fillBuffer(nullBuffer->mtl, NS::Range(0, 1), 0);
            encoder->endEncoding();

            releasePool->release();
        }
    }

    void MetalCommandList::endOtherEncoders(EncoderType type) {
        if (activeType == type) {
          // Early return for the most likely case.
          return;
        }

        switch (activeType) {
        case EncoderType::None:
          // Do nothing.
          break;
        case EncoderType::Render:
          endActiveRenderEncoder();
          break;
        case EncoderType::Compute:
          endActiveComputeEncoder();
          break;
        case EncoderType::Blit:
          endActiveBlitEncoder();
          break;
        case EncoderType::Resolve:
          endActiveResolveTextureComputeEncoder();
          break;
        default:
          assert(false && "Unknown encoder type.");
          break;
        }
    }

    void MetalCommandList::checkActiveComputeEncoder() {
        endOtherEncoders(EncoderType::Compute);
        activeType = EncoderType::Compute;

        if (activeComputeEncoder == nullptr) {
            NS::AutoreleasePool *releasePool = NS::AutoreleasePool::alloc()->init();
            
            activeComputeEncoder = mtl->computeCommandEncoder(MTL::DispatchTypeConcurrent);
            activeComputeEncoder->setLabel(MTLSTR("Compute Encoder"));

            activeComputeEncoder->retain();
            releasePool->release();

            startedEncoding = true;

            barrierWait(MetalBarrierStage::COMPUTE, activeComputeEncoder);

            dirtyComputeState.setAll();
        }

        if (dirtyComputeState.pipelineState) {
            activeComputeEncoder->setComputePipelineState(activeComputeState->pipelineState);
            dirtyComputeState.pipelineState = 0;
        }

        for (auto* descriptorSet : computeDescriptorSets) {
            if (descriptorSet) {
                descriptorSet->commit();
            }
        }

        if (dirtyComputeState.descriptorSets) {
            activeComputePipelineLayout->bindDescriptorSets(activeComputeEncoder, computeDescriptorSets, MAX_DESCRIPTOR_SET_BINDINGS, true, dirtyComputeState.descriptorSetDirtyIndex, currentEncoderDescriptorSets, mtl);
            dirtyComputeState.descriptorSets = 0;
            dirtyComputeState.descriptorSetDirtyIndex = MAX_DESCRIPTOR_SET_BINDINGS;
        }

        if (dirtyComputeState.pushConstants) {
            for (const PushConstantData &pushConstant : pushConstants) {
                if (pushConstant.stageFlags & RenderShaderStageFlag::COMPUTE) {
                    // Bind right after the descriptor sets, up till the max push constant index
                    const uint32_t bindIndex = PUSH_CONSTANTS_BINDING_INDEX + pushConstant.binding;
                    activeComputeEncoder->setBytes(pushConstant.data.data(), pushConstant.size, bindIndex);
                }
            }
            stateCache.lastPushConstants = pushConstants;
            dirtyComputeState.pushConstants = 0;
        }
    }

    void MetalCommandList::endActiveComputeEncoder() {
        if (activeComputeEncoder != nullptr) {
            bindEncoderResources(activeComputeEncoder, true);
            barrierUpdate(MetalBarrierStage::COMPUTE, activeComputeEncoder);
            activeComputeEncoder->updateFence(timestampQueryFence);
            activeComputeEncoder->endEncoding();
            activeComputeEncoder->release();
            activeComputeEncoder = nullptr;
            currentEncoderDescriptorSets.clear();

            // Clear state cache for compute
            stateCache.lastPushConstants.clear();
        }
    }

    void MetalCommandList::checkActiveRenderEncoder() {
        assert(targetFramebuffer != nullptr);
        endOtherEncoders(EncoderType::Render);
        
        if (pendingClears.active) {
            endActiveRenderEncoder();
        }
        
        activeType = EncoderType::Render;

        if (activeRenderEncoder == nullptr) {
            NS::AutoreleasePool *releasePool = NS::AutoreleasePool::alloc()->init();

            // target frame buffer & sample positions affect the descriptor
            MTL::RenderPassDescriptor *renderDescriptor = MTL::RenderPassDescriptor::renderPassDescriptor();

            for (uint32_t i = 0; i < targetFramebuffer->colorAttachments.size(); i++) {
                MTL::RenderPassColorAttachmentDescriptor *colorAttachment = renderDescriptor->colorAttachments()->object(i);
                colorAttachment->setTexture(targetFramebuffer->colorAttachments[i].getTexture());
                colorAttachment->setLoadAction(pendingClears.initialAction[i]);
                colorAttachment->setClearColor(mapClearColor(pendingClears.clearValues[i].color));
                colorAttachment->setStoreAction(MTL::StoreActionStore);
            }

            if (targetFramebuffer->depthAttachment.format != RenderFormat::UNKNOWN) {
                const size_t depthIndex = targetFramebuffer->colorAttachments.size();
                MTL::RenderPassDepthAttachmentDescriptor *depthAttachment = renderDescriptor->depthAttachment();
                depthAttachment->setTexture(targetFramebuffer->depthAttachment.getTexture());
                depthAttachment->setLoadAction(pendingClears.initialAction[depthIndex]);
                depthAttachment->setClearDepth(pendingClears.clearValues[depthIndex].depth);
                depthAttachment->setStoreAction(MTL::StoreActionStore);

                if (RenderFormatIsStencil(targetFramebuffer->depthAttachment.format)) {
                    MTL::RenderPassStencilAttachmentDescriptor *stencilAttachment = renderDescriptor->stencilAttachment();
                    stencilAttachment->setTexture(targetFramebuffer->depthAttachment.getTexture());
                    stencilAttachment->setLoadAction(pendingClears.initialAction[depthIndex + 1]);
                    stencilAttachment->setClearStencil(pendingClears.clearValues[depthIndex + 1].stencil);
                    stencilAttachment->setStoreAction(MTL::StoreActionStore);
                }
            }

            if (targetFramebuffer->sampleCount > 1) {
                renderDescriptor->setSamplePositions(targetFramebuffer->samplePositions, targetFramebuffer->sampleCount);
            }

            activeRenderEncoder = mtl->renderCommandEncoder(renderDescriptor);
            activeRenderEncoder->setLabel(MTLSTR("Graphics Render Encoder"));

            barrierWait(MetalBarrierStage::GRAPHICS, activeRenderEncoder);

            activeRenderEncoder->retain();
            releasePool->release();

            startedEncoding = true;
            
            // Reset pending clears since we've now handled them
            if (pendingClears.active) {
                for (auto& action : pendingClears.initialAction) {
                    action = MTL::LoadActionLoad;
                }
                pendingClears.active = false;
            }
        }
    }

    void MetalCommandList::checkForUpdatesInGraphicsState() {
        if (dirtyGraphicsState.pipelineState) {
            if (activeRenderState) {
                activeRenderEncoder->setRenderPipelineState(activeRenderState->renderPipelineState);
                activeRenderEncoder->setDepthStencilState(activeRenderState->depthStencilState);
                activeRenderEncoder->setDepthClipMode(activeRenderState->depthClipMode);
                activeRenderEncoder->setCullMode(activeRenderState->cullMode);
                activeRenderEncoder->setFrontFacingWinding(activeRenderState->winding);
                activeRenderEncoder->setStencilReferenceValue(activeRenderState->stencilReference);
                stateCache.lastPipelineState = activeRenderState->renderPipelineState;
            }
            dirtyGraphicsState.pipelineState = 0;
        }

        if (dirtyGraphicsState.viewports) {
            if (viewportVector.empty()) return;

            activeRenderEncoder->setViewports(viewportVector.data(), viewportVector.size());
            stateCache.lastViewports = viewportVector;
            dirtyGraphicsState.viewports = 0;
        }

        if (dirtyGraphicsState.depthBias) {
            if (activeRenderState->dynamicDepthBiasEnabled) {
                activeRenderEncoder->setDepthBias(dynamicDepthBias.depthBias, dynamicDepthBias.slopeScaledDepthBias, dynamicDepthBias.depthBiasClamp);
            } else {
                activeRenderEncoder->setDepthBias(activeRenderState->depthBiasConstantFactor, activeRenderState->depthBiasSlopeFactor, activeRenderState->depthBiasClamp);
            }

            dirtyGraphicsState.depthBias = 0;
        }

        if (dirtyGraphicsState.scissors) {
            if (scissorVector.empty()) return;

            activeRenderEncoder->setScissorRects(scissorVector.data(), scissorVector.size());
            stateCache.lastScissors = scissorVector;
            dirtyGraphicsState.scissors = 0;
        }

        if (dirtyGraphicsState.vertexBufferSlots) {
            uint32_t vertexBufferSlots = dirtyGraphicsState.vertexBufferSlots;
            while (vertexBufferSlots > 0) {
                const uint32_t i = __builtin_ctzll(vertexBufferSlots);
                activeRenderEncoder->setVertexBuffer(vertexBuffers[i], vertexBufferOffsets[i], VERTEX_BUFFERS_BINDING_INDEX + i);
                vertexBufferSlots &= ~(1U << i);
            }
            dirtyGraphicsState.vertexBufferSlots = 0;
        }

        for (auto* descriptorSet : renderDescriptorSets) {
            if (descriptorSet) {
                descriptorSet->commit();
            }
        }

        if (dirtyGraphicsState.descriptorSets) {
            if (activeGraphicsPipelineLayout) {
                activeGraphicsPipelineLayout->bindDescriptorSets(activeRenderEncoder, renderDescriptorSets, MAX_DESCRIPTOR_SET_BINDINGS, false, dirtyGraphicsState.descriptorSetDirtyIndex, currentEncoderDescriptorSets, mtl);
            }
            dirtyGraphicsState.descriptorSets = 0;
            dirtyGraphicsState.descriptorSetDirtyIndex = MAX_DESCRIPTOR_SET_BINDINGS + 1;
        }

        if (dirtyGraphicsState.pushConstants) {
            for (const PushConstantData &pushConstant : pushConstants) {
                // Bind right after the descriptor sets, up till the max push constant index
                const uint32_t bindIndex = PUSH_CONSTANTS_BINDING_INDEX + pushConstant.binding;
                if (pushConstant.stageFlags & RenderShaderStageFlag::VERTEX) {
                    activeRenderEncoder->setVertexBytes(pushConstant.data.data(), pushConstant.size, bindIndex);
                }
                if (pushConstant.stageFlags & RenderShaderStageFlag::PIXEL) {
                    activeRenderEncoder->setFragmentBytes(pushConstant.data.data(), pushConstant.size, bindIndex);
                }
            }

            stateCache.lastPushConstants = pushConstants;
            dirtyGraphicsState.pushConstants = 0;
        }
    }

    void MetalCommandList::endActiveRenderEncoder() {
        if (activeRenderEncoder != nullptr) {
            bindEncoderResources(activeRenderEncoder, false);
            barrierUpdate(MetalBarrierStage::GRAPHICS, activeRenderEncoder);
            activeRenderEncoder->updateFence(timestampQueryFence, MTL::RenderStageVertex | MTL::RenderStageFragment);
            activeRenderEncoder->endEncoding();
            activeRenderEncoder->release();
            activeRenderEncoder = nullptr;
            currentEncoderDescriptorSets.clear();

            // Mark all state as needing rebind for next encoder
            dirtyGraphicsState.setAll();

            // Clear state cache since we'll need to rebind everything
            stateCache.lastPipelineState = nullptr;
            stateCache.lastViewports.clear();
            stateCache.lastScissors.clear();
            stateCache.lastPushConstants.clear();
        }
    }

    void MetalCommandList::checkActiveBlitEncoder() {
        endOtherEncoders(EncoderType::Blit);
        activeType = EncoderType::Blit;

        if (activeBlitEncoder == nullptr) {
            activeBlitEncoder = mtl->blitCommandEncoder(device->sharedBlitDescriptor);
            activeBlitEncoder->setLabel(MTLSTR("Copy Blit Encoder"));

            startedEncoding = true;

            barrierWait(MetalBarrierStage::COPY, activeBlitEncoder);
        }
    }

    void MetalCommandList::endActiveBlitEncoder() {
        if (activeBlitEncoder != nullptr) {
            barrierUpdate(MetalBarrierStage::COPY, activeBlitEncoder);
            activeBlitEncoder->updateFence(timestampQueryFence);
            activeBlitEncoder->endEncoding();
            activeBlitEncoder->release();
            activeBlitEncoder = nullptr;
        }
    }

    void MetalCommandList::checkActiveResolveTextureComputeEncoder() {
        assert(targetFramebuffer != nullptr);

        endOtherEncoders(EncoderType::Resolve);
        activeType = EncoderType::Resolve;

        if (activeResolveComputeEncoder == nullptr) {
            activeResolveComputeEncoder = mtl->computeCommandEncoder(MTL::DispatchTypeConcurrent);
            activeResolveComputeEncoder->setLabel(MTLSTR("Resolve Texture Encoder"));
            activeResolveComputeEncoder->setComputePipelineState(device->resolveTexturePipelineState);

            startedEncoding = true;

            barrierWait(MetalBarrierStage::COPY, activeResolveComputeEncoder);
        }
    }

    void MetalCommandList::endActiveResolveTextureComputeEncoder() {
        if (activeResolveComputeEncoder != nullptr) {
            barrierUpdate(MetalBarrierStage::COPY, activeResolveComputeEncoder);
            activeResolveComputeEncoder->updateFence(timestampQueryFence);
            activeResolveComputeEncoder->endEncoding();
            activeResolveComputeEncoder->release();
            activeResolveComputeEncoder = nullptr;
        }
    }

    void MetalCommandList::bindEncoderResources(MTL::CommandEncoder* encoder, bool isCompute) {
        if (isCompute) {
            auto* computeEncoder = static_cast<MTL::ComputeCommandEncoder*>(encoder);
            if (device->gpuAddressableResidencySet == nullptr) {
                std::lock_guard lock(device->gpuAddressableResourcesMutex);
                for (const auto* resource : device->gpuAddressableResources) {
                    computeEncoder->useResource(resource, MTL::ResourceUsageRead);
                }
            }
            for (const auto* descriptorSet : currentEncoderDescriptorSets) {
                if (descriptorSet->residencySet == nullptr) {
                    for (const auto& entry : descriptorSet->resourceEntries) {
                        if (entry.resource != nullptr) {
                            computeEncoder->useResource(entry.resource, mapResourceUsage(entry.type));
                        }
                    }
                }
            }
        } else {
            auto* renderEncoder = static_cast<MTL::RenderCommandEncoder*>(encoder);
            if (device->gpuAddressableResidencySet == nullptr) {
                std::lock_guard lock(device->gpuAddressableResourcesMutex);
                for (const auto* resource : device->gpuAddressableResources) {
                    renderEncoder->useResource(resource, MTL::ResourceUsageRead);
                }
            }
            for (const auto* descriptorSet : currentEncoderDescriptorSets) {
                if (descriptorSet->residencySet == nullptr) {
                    for (const auto& entry : descriptorSet->resourceEntries) {
                        if (entry.resource != nullptr) {
                            renderEncoder->useResource(entry.resource, mapResourceUsage(entry.type), MTL::RenderStageVertex | MTL::RenderStageFragment);
                        }
                    }
                }
            }
        }
    }

    // MetalCommandFence

    MetalCommandFence::MetalCommandFence(MetalDevice *device) {
        semaphore = dispatch_semaphore_create(0);
    }

    MetalCommandFence::~MetalCommandFence() {
        dispatch_release(semaphore);
    }

    // MetalCommandSemaphore

    MetalCommandSemaphore::MetalCommandSemaphore(const MetalDevice *device) {
        this->mtl = device->mtl->newEvent();
        this->mtlEventValue = 1;
    }

    MetalCommandSemaphore::~MetalCommandSemaphore() {
        mtl->release();
    }

    // MetalCommandQueue

    MetalCommandQueue::MetalCommandQueue(MetalDevice *device, RenderCommandListType type) {
        assert(device != nullptr);
        assert(type != RenderCommandListType::UNKNOWN);

        this->device = device;
        this->mtl = device->mtl->newCommandQueue();

        if (device->gpuAddressableResidencySet != nullptr) {
            // Automatically add residency set for GPU-addressable buffers to all command buffers in the queue.
            mtl->addResidencySet(device->gpuAddressableResidencySet);
        }
    }

    MetalCommandQueue::~MetalCommandQueue() {
        mtl->release();
    }

    std::unique_ptr<RenderCommandList> MetalCommandQueue::createCommandList() {
        return std::make_unique<MetalCommandList>(this);
    }

    std::unique_ptr<RenderSwapChain> MetalCommandQueue::createSwapChain(RenderWindow renderWindow, uint32_t textureCount, RenderFormat format, uint32_t maxFrameLatency) {
        return std::make_unique<MetalSwapChain>(this, renderWindow, textureCount, format, maxFrameLatency);
    }

    void MetalCommandQueue::executeCommandLists(const RenderCommandList **commandLists, const uint32_t commandListCount, RenderCommandSemaphore **waitSemaphores, const uint32_t waitSemaphoreCount, RenderCommandSemaphore **signalSemaphores, const uint32_t signalSemaphoreCount, RenderCommandFence *signalFence) {
        assert(commandLists != nullptr);
        assert(commandListCount > 0);

        // Create a new command buffer to encode the wait semaphores into
        MTL::CommandBuffer* cmdBuffer = mtl->commandBufferWithUnretainedReferences();
        cmdBuffer->setLabel(MTLSTR("Wait Command Buffer"));
        cmdBuffer->enqueue();

        for (uint32_t i = 0; i < waitSemaphoreCount; i++) {
            MetalCommandSemaphore *interfaceSemaphore = static_cast<MetalCommandSemaphore *>(waitSemaphores[i]);
            cmdBuffer->encodeWait(interfaceSemaphore->mtl, interfaceSemaphore->mtlEventValue++);
        }

        cmdBuffer->commit();

        // Commit all command lists except the last one

        for (uint32_t i = 0; i < commandListCount - 1; i++) {
            assert(commandLists[i] != nullptr);

            const MetalCommandList *interfaceCommandList = static_cast<const MetalCommandList *>(commandLists[i]);
            MetalCommandList *mutableCommandList = const_cast<MetalCommandList*>(interfaceCommandList);
            mutableCommandList->mtl->enqueue();
            mutableCommandList->commit();
        }

        // Use the last command list to mark the end and signal the fence
        const MetalCommandList *interfaceCommandList = static_cast<const MetalCommandList *>(commandLists[commandListCount - 1]);
        interfaceCommandList->mtl->enqueue();

        if (signalFence != nullptr) {
            interfaceCommandList->mtl->addCompletedHandler([signalFence](MTL::CommandBuffer* cmdBuffer) {
                dispatch_semaphore_signal(static_cast<MetalCommandFence *>(signalFence)->semaphore);
            });
        }

        for (uint32_t i = 0; i < signalSemaphoreCount; i++) {
            const MetalCommandSemaphore *interfaceSemaphore = static_cast<MetalCommandSemaphore *>(signalSemaphores[i]);
            interfaceCommandList->mtl->encodeSignalEvent(interfaceSemaphore->mtl, interfaceSemaphore->mtlEventValue);
        }

        MetalCommandList *mutableCommandList = const_cast<MetalCommandList*>(interfaceCommandList);
        mutableCommandList->commit();
    }

    void MetalCommandQueue::waitForCommandFence(RenderCommandFence *fence) {
        const MetalCommandFence *metalFence = static_cast<MetalCommandFence *>(fence);
        dispatch_semaphore_wait(metalFence->semaphore, DISPATCH_TIME_FOREVER);
    }

    // MetalPipelineLayout

    MetalPipelineLayout::MetalPipelineLayout(MetalDevice *device, const RenderPipelineLayoutDesc &desc) {
        assert(device != nullptr);

        this->setLayoutCount = desc.descriptorSetDescsCount;

        pushConstantRanges.resize(desc.pushConstantRangesCount);
        memcpy(pushConstantRanges.data(), desc.pushConstantRanges, sizeof(RenderPushConstantRange) * desc.pushConstantRangesCount);
    }

    MetalPipelineLayout::~MetalPipelineLayout() {}

    void MetalPipelineLayout::bindDescriptorSets(MTL::CommandEncoder* encoder, const MetalDescriptorSet* const* descriptorSets, uint32_t descriptorSetCount, bool isCompute, uint32_t startIndex, std::unordered_set<MetalDescriptorSet*>& encoderDescriptorSets, MTL::CommandBuffer* commandBuffer) const {
        for (uint32_t i = startIndex; i < setLayoutCount; i++) {
            if (i >= descriptorSetCount || descriptorSets[i] == nullptr) {
                continue;
            }

            const MetalDescriptorSet* descriptorSet = descriptorSets[i];
            const MetalArgumentBuffer& descriptorBuffer = descriptorSet->argumentBuffer;

            if (descriptorSet->residencySet != nullptr) {
                commandBuffer->useResidencySet(descriptorSet->residencySet);
            } else {
                // Track descriptor set for later resource binding
                encoderDescriptorSets.insert(const_cast<MetalDescriptorSet*>(descriptorSet));
            }

            // Bind argument buffer
            if (isCompute) {
                static_cast<MTL::ComputeCommandEncoder*>(encoder)->setBuffer(descriptorBuffer.mtl, descriptorBuffer.offset, DESCRIPTOR_SETS_BINDING_INDEX + i);
            } else {
                static_cast<MTL::RenderCommandEncoder*>(encoder)->setFragmentBuffer(descriptorBuffer.mtl, descriptorBuffer.offset, DESCRIPTOR_SETS_BINDING_INDEX + i);
                static_cast<MTL::RenderCommandEncoder*>(encoder)->setVertexBuffer(descriptorBuffer.mtl, descriptorBuffer.offset, DESCRIPTOR_SETS_BINDING_INDEX + i);
            }
        }
    }

    // MetalDevice

    MetalDevice::MetalDevice(MetalInterface *renderInterface, const std::string &preferredDeviceName) {
        assert(renderInterface != nullptr);
        this->renderInterface = renderInterface;

        // Device Selection
        const NS::Array* devices = MTL::CopyAllDevices();
        MTL::Device *preferredDevice = nullptr;
        for (NS::UInteger i = 0; i < devices->count(); i++) {
            MTL::Device *device = (MTL::Device *)devices->object(i);
            const NS::String *preferredDeviceNameNS = NS::String::string(preferredDeviceName.c_str(), NS::UTF8StringEncoding);
            if (device->name()->isEqualToString(preferredDeviceNameNS)) {
                preferredDevice = device;
                break;
            }
        }

        mtl = preferredDevice ? preferredDevice : MTL::CreateSystemDefaultDevice();;
        const std::string deviceName(mtl->name()->utf8String());
        description.name = deviceName;
        description.type = mapDeviceType(mtl->location());
        description.driverVersion = 1; // Unavailable
        description.vendor = mtl->supportsFamily(MTL::GPUFamilyApple1) ? RenderDeviceVendor::APPLE : getRenderDeviceVendor(mtl->registryID());
        description.dedicatedVideoMemory = mtl->recommendedMaxWorkingSetSize();

        timestampCounterSet = findTimestampCounterSet();

        // Setup blit, clear and resolve shaders / pipelines
        createClearShaderLibrary();
        createResolvePipelineState();
        sharedBlitDescriptor = MTL::BlitPassDescriptor::alloc()->init();

        NS::OperatingSystemVersion osVersion = NS::ProcessInfo::processInfo()->operatingSystemVersion();

        // Fill capabilities.
        // https://developer.apple.com/documentation/metal/device-inspection
        // TODO: Support Raytracing.
        // capabilities.raytracing = mtl->supportsRaytracing();
        capabilities.maxTextureSize = mtl->supportsFamily(MTL::GPUFamilyApple3) ? 16384 : 8192;
        capabilities.sampleLocations = mtl->programmableSamplePositionsSupported();
        capabilities.resolveModes = false;
        capabilities.scalarBlockLayout = true;
        capabilities.presentWait = true;
        capabilities.preferHDR = mtl->recommendedMaxWorkingSetSize() > (512 * 1024 * 1024);
        capabilities.dynamicDepthBias = true;
        capabilities.uma = mtl->hasUnifiedMemory();
        capabilities.gpuUploadHeap = capabilities.uma;
        capabilities.queryPools = timestampCounterSet != nullptr;
        capabilities.samplerMirrorClampToEdge = true;

#if PLUME_IOS
        capabilities.descriptorIndexing = mtl->supportsFamily(MTL::GPUFamilyApple3);
        capabilities.displayTiming = false;
        capabilities.bufferDeviceAddress = osVersion.majorVersion >= 16 && mtl->supportsFamily(MTL::GPUFamilyApple3);
        supportsResidencySets = osVersion.majorVersion >= 18 && mtl->supportsFamily(MTL::GPUFamilyApple6);
#else
        capabilities.descriptorIndexing = true;
        capabilities.displayTiming = osVersion.majorVersion >= 12;
        capabilities.bufferDeviceAddress = osVersion.majorVersion >= 13 && mtl->supportsFamily(MTL::GPUFamilyApple3);
        supportsResidencySets = osVersion.majorVersion >= 15 && mtl->supportsFamily(MTL::GPUFamilyApple6);
#endif

        nullBuffer = createBuffer(RenderBufferDesc::DefaultBuffer(16, RenderBufferFlag::VERTEX));

        if (supportsResidencySets) {
            MTL::ResidencySetDescriptor* residencySetDescriptor = MTL::ResidencySetDescriptor::alloc()->init();
            gpuAddressableResidencySet = mtl->newResidencySet(residencySetDescriptor, nullptr);
            residencySetDescriptor->release();
        }
    }

    MetalDevice::~MetalDevice() {
        mtl->release();

        for (const auto& [key, state] : clearRenderPipelineStates) {
            state->release();
        }

        resolveTexturePipelineState->release();
        clearVertexFunction->release();
        clearColorFunction->release();
        clearDepthFunction->release();
        sharedBlitDescriptor->release();

        if (gpuAddressableResidencySet != nullptr) {
            gpuAddressableResidencySet->endResidency();
            gpuAddressableResidencySet->release();
        }
    }

    std::unique_ptr<RenderDescriptorSet> MetalDevice::createDescriptorSet(const RenderDescriptorSetDesc &desc) {
        return std::make_unique<MetalDescriptorSet>(this, desc);
    }

    std::unique_ptr<RenderShader> MetalDevice::createShader(const void *data, uint64_t size, const char *entryPointName, RenderShaderFormat format) {
        return std::make_unique<MetalShader>(this, data, size, entryPointName, format);
    }

    std::unique_ptr<RenderSampler> MetalDevice::createSampler(const RenderSamplerDesc &desc) {
        return std::make_unique<MetalSampler>(this, desc);
    }

    std::unique_ptr<RenderPipeline> MetalDevice::createComputePipeline(const RenderComputePipelineDesc &desc) {
        return std::make_unique<MetalComputePipeline>(this, desc);
    }

    std::unique_ptr<RenderPipeline> MetalDevice::createGraphicsPipeline(const RenderGraphicsPipelineDesc &desc) {
        return std::make_unique<MetalGraphicsPipeline>(this, desc);
    }

    std::unique_ptr<RenderPipeline> MetalDevice::createRaytracingPipeline(const RenderRaytracingPipelineDesc &desc, const RenderPipeline *previousPipeline) {
        // TODO: Unimplemented (Raytracing).
        return nullptr;
    }

    std::unique_ptr<RenderCommandQueue> MetalDevice::createCommandQueue(RenderCommandListType type) {
        return std::make_unique<MetalCommandQueue>(this, type);
    }

    std::unique_ptr<RenderBuffer> MetalDevice::createBuffer(const RenderBufferDesc &desc) {
        return std::make_unique<MetalBuffer>(this, nullptr, desc);
    }

    std::unique_ptr<RenderTexture> MetalDevice::createTexture(const RenderTextureDesc &desc) {
        return std::make_unique<MetalTexture>(this, nullptr, desc);
    }

    std::unique_ptr<RenderAccelerationStructure> MetalDevice::createAccelerationStructure(const RenderAccelerationStructureDesc &desc) {
        return std::make_unique<MetalAccelerationStructure>(this, desc);
    }

    std::unique_ptr<RenderPool> MetalDevice::createPool(const RenderPoolDesc &desc) {
        return std::make_unique<MetalPool>(this, desc);
    }

    std::unique_ptr<RenderPipelineLayout> MetalDevice::createPipelineLayout(const RenderPipelineLayoutDesc &desc) {
        return std::make_unique<MetalPipelineLayout>(this, desc);
    }

    std::unique_ptr<RenderCommandFence> MetalDevice::createCommandFence() {
        return std::make_unique<MetalCommandFence>(this);
    }

    std::unique_ptr<RenderCommandSemaphore> MetalDevice::createCommandSemaphore() {
        return std::make_unique<MetalCommandSemaphore>(this);
    }

    std::unique_ptr<RenderFramebuffer> MetalDevice::createFramebuffer(const RenderFramebufferDesc &desc) {
        return std::make_unique<MetalFramebuffer>(this, desc);
    }

    std::unique_ptr<RenderQueryPool> MetalDevice::createQueryPool(uint32_t queryCount) {
        return std::make_unique<MetalQueryPool>(this, queryCount);
    }

    void MetalDevice::setBottomLevelASBuildInfo(RenderBottomLevelASBuildInfo &buildInfo, const RenderBottomLevelASMesh *meshes, uint32_t meshCount, bool preferFastBuild, bool preferFastTrace) {
        // TODO: Unimplemented (Raytracing).
    }

    void MetalDevice::setTopLevelASBuildInfo(RenderTopLevelASBuildInfo &buildInfo, const RenderTopLevelASInstance *instances, uint32_t instanceCount, bool preferFastBuild, bool preferFastTrace) {
        // TODO: Unimplemented (Raytracing).
    }

    void MetalDevice::setShaderBindingTableInfo(RenderShaderBindingTableInfo &tableInfo, const RenderShaderBindingGroups &groups, const RenderPipeline *pipeline, RenderDescriptorSet **descriptorSets, uint32_t descriptorSetCount) {
        // TODO: Unimplemented (Raytracing).
    }

    const RenderDeviceCapabilities &MetalDevice::getCapabilities() const {
        return capabilities;
    }

    const RenderDeviceDescription &MetalDevice::getDescription() const {
        return description;
    }

    RenderSampleCounts MetalDevice::getSampleCountsSupported(RenderFormat format) const {
        RenderSampleCounts supportedSampleCounts = RenderSampleCount::COUNT_0;
        for (uint32_t sc = RenderSampleCount::COUNT_1; sc <= RenderSampleCount::COUNT_64; sc <<= 1) {
            if (mtl->supportsTextureSampleCount(sc)) {
                supportedSampleCounts |= sc;
            }
        }

        return supportedSampleCounts;
    }

    void MetalDevice::release() {
        mtl->release();
    }

    bool MetalDevice::isValid() const {
        return mtl != nullptr;
    }

    bool MetalDevice::beginCapture() {
        MTL::CaptureManager *manager = MTL::CaptureManager::sharedCaptureManager();
        manager->startCapture(mtl);
        return true;
    }

    bool MetalDevice::endCapture() {
        MTL::CaptureManager *manager = MTL::CaptureManager::sharedCaptureManager();
        manager->stopCapture();
        return true;
    }

    const MTL::CounterSet* MetalDevice::findTimestampCounterSet() const {
        for (uint32_t setIndex = 0; setIndex < mtl->counterSets()->count(); setIndex++){
            const MTL::CounterSet *counterSet = static_cast<MTL::CounterSet*>(mtl->counterSets()->object(setIndex));
            for (uint32_t counterIndex = 0; counterIndex < counterSet->counters()->count(); counterIndex++) {
                const MTL::Counter *counter = static_cast<MTL::Counter*>(counterSet->counters()->object(counterIndex));
                if (counter->name()->isEqualToString(MTL::CommonCounterTimestamp))
                    return counterSet;
            }
        }
        return nullptr;
    }

    void MetalDevice::createResolvePipelineState() {
        const char* resolve_shader = R"(
            #include <metal_stdlib>
            using namespace metal;

            struct ResolveParams {
                uint2 dstOffset;
                uint2 srcOffset;
                uint2 resolveSize;
            };

            kernel void msaaResolve(
                texture2d_ms<float> source [[texture(0)]],
                texture2d<float, access::write> destination [[texture(1)]],
                constant ResolveParams& params [[buffer(0)]],
                uint2 gid [[thread_position_in_grid]])
            {
                if (gid.x >= params.resolveSize.x || gid.y >= params.resolveSize.y) return;
                uint2 dstPos = gid + params.dstOffset;
                uint2 srcPos = gid + params.srcOffset;
                float4 color = float4(0);
                for (uint s = 0; s < source.get_num_samples(); s++) {
                    color += source.read(srcPos, s);
                }
                color /= float(source.get_num_samples());
                destination.write(color, dstPos);
            }
        )";

        NS::Error* error = nullptr;
        MTL::Library *library = mtl->newLibrary(NS::String::string(resolve_shader, NS::UTF8StringEncoding), nullptr, &error);
        assert(library != nullptr && "Failed to create library");

        MTL::Function *resolveFunction = library->newFunction(NS::String::string("msaaResolve", NS::UTF8StringEncoding));
        assert(resolveFunction != nullptr && "Failed to create resolve function");

        error = nullptr;
        resolveTexturePipelineState = mtl->newComputePipelineState(resolveFunction, &error);
        assert(resolveTexturePipelineState != nullptr && "Failed to create MSAA resolve pipeline state");

        // Destroy
        resolveFunction->release();
        library->release();
    }

    void MetalDevice::createClearShaderLibrary() {
        const char* clear_shader = R"(
            #include <metal_stdlib>
            using namespace metal;

            struct DepthClearFragmentOut {
                float depth [[depth(any)]];
            };

            struct VertexOutput {
                float4 position [[position]];
                uint rect_index [[flat]];
            };

            vertex VertexOutput clearVert(uint vid [[vertex_id]],
                                        uint instance_id [[instance_id]],
                                        constant float2* vertices [[buffer(0)]])
            {
                VertexOutput out;
                out.position = float4(vertices[vid], 0, 1);
                out.rect_index = instance_id;
                return out;
            }

            // Color clear fragment shader
            fragment float4 clearColorFrag(VertexOutput in [[stage_in]],
                                         constant float4* clearColors [[buffer(0)]])
            {
                return clearColors[in.rect_index];
            }

            // Depth clear fragment shader
            fragment DepthClearFragmentOut clearDepthFrag(VertexOutput in [[stage_in]],
                                        constant float* clearDepths [[buffer(0)]])
            {
                DepthClearFragmentOut out;
                out.depth = clearDepths[in.rect_index];
                return out;
            }
        )";

        NS::Error* error = nullptr;
        MTL::Library *clearShaderLibrary = mtl->newLibrary(NS::String::string(clear_shader, NS::UTF8StringEncoding), nullptr, &error);
        if (error != nullptr) {
            fprintf(stderr, "Error: %s\n", error->localizedDescription()->utf8String());
        }
        assert(clearShaderLibrary != nullptr && "Failed to create clear color library");

        // Create and cache the shader functions
        clearVertexFunction = clearShaderLibrary->newFunction(MTLSTR("clearVert"));
        clearColorFunction = clearShaderLibrary->newFunction(MTLSTR("clearColorFrag"));
        clearDepthFunction = clearShaderLibrary->newFunction(MTLSTR("clearDepthFrag"));

        // Create depth stencil clear states
        MTL::StencilDescriptor *stencilDescriptor = MTL::StencilDescriptor::alloc()->init();
        stencilDescriptor->setDepthStencilPassOperation(MTL::StencilOperationReplace);
        stencilDescriptor->setStencilCompareFunction(MTL::CompareFunctionAlways);
        stencilDescriptor->setWriteMask(0xFFFFFFFF);

        MTL::DepthStencilDescriptor *depthDescriptor = MTL::DepthStencilDescriptor::alloc()->init();

        depthDescriptor->setDepthWriteEnabled(true);
        depthDescriptor->setDepthCompareFunction(MTL::CompareFunctionAlways);
        clearDepthState = mtl->newDepthStencilState(depthDescriptor);

        depthDescriptor->setBackFaceStencil(stencilDescriptor);
        depthDescriptor->setFrontFaceStencil(stencilDescriptor);
        clearDepthStencilState = mtl->newDepthStencilState(depthDescriptor);

        depthDescriptor->setDepthWriteEnabled(false);
        clearStencilState = mtl->newDepthStencilState(depthDescriptor);

        depthDescriptor->release();
        stencilDescriptor->release();
        clearShaderLibrary->release();
    }

    MTL::RenderPipelineState* MetalDevice::getOrCreateClearRenderPipelineState(MTL::RenderPipelineDescriptor *pipelineDesc, const bool depthWriteEnabled, const bool stencilWriteEnabled) {
        uint64_t pipelineKey = createClearPipelineKey(pipelineDesc, depthWriteEnabled, stencilWriteEnabled);

        std::lock_guard lock(clearPipelineStateMutex);
        const auto it = clearRenderPipelineStates.find(pipelineKey);
        if (it != clearRenderPipelineStates.end()) {
            return it->second;
        }

        // If not found, create new pipeline state while holding the lock
        NS::Error *error = nullptr;
        MTL::RenderPipelineState *clearPipelineState = mtl->newRenderPipelineState(pipelineDesc, &error);

        if (error != nullptr) {
            fprintf(stderr, "Failed to create render pipeline state: %s\n", error->localizedDescription()->utf8String());
            return nullptr;
        }

        auto [inserted_it, success] = clearRenderPipelineStates.insert(std::make_pair(pipelineKey, clearPipelineState));
        return inserted_it->second;
    }

    // MetalInterface

    MetalInterface::MetalInterface() {
        NS::AutoreleasePool *releasePool = NS::AutoreleasePool::alloc()->init();
        capabilities.shaderFormat = RenderShaderFormat::METAL;

        releasePool->release();

        // Fill device names.
        const NS::Array* devices = MTL::CopyAllDevices();
        for (NS::UInteger i = 0; i < devices->count(); i++) {
            NS::String* deviceName = ((MTL::Device *)devices->object(i))->name();
            deviceNames.push_back(std::string(deviceName->utf8String()));
        }
    }

    MetalInterface::~MetalInterface() {}

    // TODO: NEW - Incorporate preferredDeviceName (new)
    std::unique_ptr<RenderDevice> MetalInterface::createDevice(const std::string &preferredDeviceName) {
        std::unique_ptr<MetalDevice> createdDevice = std::make_unique<MetalDevice>(this, preferredDeviceName);
        return createdDevice->isValid() ? std::move(createdDevice) : nullptr;
    }

    const RenderInterfaceCapabilities &MetalInterface::getCapabilities() const {
        return capabilities;
    }

    const std::vector<std::string> &MetalInterface::getDeviceNames() const {
        return deviceNames;
    }

    bool MetalInterface::isValid() const {
        return true;
    }

    // Global creation function.

    std::unique_ptr<RenderInterface> CreateMetalInterface() {
        std::unique_ptr<MetalInterface> createdInterface = std::make_unique<MetalInterface>();
        return createdInterface->isValid() ? std::move(createdInterface) : nullptr;
    }
}
