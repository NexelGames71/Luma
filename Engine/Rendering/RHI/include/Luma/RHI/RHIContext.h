#pragma once

#include <memory>
#include <vector>

#include "Luma/Core/Types.h"
#include "Luma/RHI/RHIResources.h"
#include "Luma/RHI/RHICommandList.h"
#include "Luma/RHI/RHIPipeline.h"
#include "Luma/RHI/RHIShader.h"

// RHI context interface for managing device state and creating resources.
// This is the main entry point for the RHI system, inspired by UE5's RHICmdList
// but adapted for Luma's simpler architecture.

namespace Luma {
namespace RHI {

// Forward declarations
class RHIDevice;
class RHICommandList;
class RHICommandQueue;
class RHIPipelineStateFactory;
class RHIShaderFactory;

// ============================================================================
// RHI Context
// ============================================================================

// RHI context manages device state and provides access to RHI functionality
class RHIContext {
public:
    virtual ~RHIContext() = default;
    
    // Get the device this context belongs to
    virtual RHIDevice* GetDevice() = 0;
    
    // Get the resource factory for creating resources
    virtual RHIResourceFactory* GetResourceFactory() = 0;
    
    // Get the pipeline state factory for creating pipeline states
    virtual RHIPipelineStateFactory* GetPipelineStateFactory() = 0;
    
    // Get the shader factory for creating shaders
    virtual RHIShaderFactory* GetShaderFactory() = 0;
    
    // Create a command list for recording commands
    virtual RHICommandList* CreateCommandList() = 0;
    
    // Destroy a command list
    virtual void DestroyCommandList(RHICommandList* cmdList) = 0;
    
    // Get the graphics command queue
    virtual RHICommandQueue* GetGraphicsQueue() = 0;
    
    // Get the compute command queue (may be same as graphics)
    virtual RHICommandQueue* GetComputeQueue() = 0;
    
    // Get the copy command queue (may be same as graphics)
    virtual RHICommandQueue* GetCopyQueue() = 0;
    
    // Flush all queues and wait for GPU idle
    virtual void Flush() = 0;
    
    // Check if the device is lost
    virtual bool IsDeviceLost() = 0;
};

// ============================================================================
// RHI Device
// ============================================================================

// RHI device represents the GPU and manages all RHI resources
class RHIDevice {
public:
    virtual ~RHIDevice() = default;
    
    // Get the device name
    virtual const char* GetDeviceName() = 0;
    
    // Get the RHI context
    virtual RHIContext* GetContext() = 0;
    
    // Get the resource factory
    virtual RHIResourceFactory* GetResourceFactory() = 0;
    
    // Get the pipeline state factory
    virtual RHIPipelineStateFactory* GetPipelineStateFactory() = 0;
    
    // Get the shader factory
    virtual RHIShaderFactory* GetShaderFactory() = 0;
    
    // Create command lists
    virtual RHICommandList* CreateCommandList() = 0;
    virtual void DestroyCommandList(RHICommandList* cmdList) = 0;
    
    // Get command queues
    virtual RHICommandQueue* GetGraphicsQueue() = 0;
    virtual RHICommandQueue* GetComputeQueue() = 0;
    virtual RHICommandQueue* GetCopyQueue() = 0;
    
    // Flush all queues
    virtual void Flush() = 0;
    
    // Wait for GPU idle
    virtual void WaitIdle() = 0;
    
    // Check if device is lost
    virtual bool IsDeviceLost() = 0;
    
    // Get device limits/capabilities
    struct DeviceLimits {
        u32 maxTextureDimension1D;
        u32 maxTextureDimension2D;
        u32 maxTextureDimension3D;
        u32 maxTextureDimensionCube;
        u32 maxTextureArrayLayers;
        u32 maxColorAttachments;
        u32 maxUniformBufferRange;
        u32 maxPushConstantsSize;
        u32 maxPerStageDescriptorSamplers;
        u32 maxPerStageDescriptorUniformBuffers;
        u32 maxPerStageDescriptorStorageBuffers;
        u32 maxPerStageDescriptorSampledImages;
        u32 maxPerStageDescriptorStorageImages;
        u32 maxPerStageDescriptorInputAttachments;
        u32 maxDescriptorSetSamplers;
        u32 maxDescriptorSetUniformBuffers;
        u32 maxDescriptorSetStorageBuffers;
        u32 maxDescriptorSetSampledImages;
        u32 maxDescriptorSetStorageImages;
        u32 maxVertexInputAttributes;
        u32 maxVertexInputBindings;
        u32 maxVertexInputAttributeOffset;
        u32 maxVertexInputBindingStride;
        u32 minUniformBufferOffsetAlignment;
        u32 minStorageBufferOffsetAlignment;
    };
    
    virtual const DeviceLimits& GetLimits() = 0;
    
    // Check if a format is supported
    virtual bool IsFormatSupported(ETextureFormat format, ETextureUsage usage) = 0;
    
    // Get optimal sample count for a format
    virtual ESampleCount GetOptimalSampleCount(ETextureFormat format) = 0;
};

// ============================================================================
// RHI Initialization
// ============================================================================

// RHI initialization description
struct RHIInitDesc {
    // Application name for device identification
    const char* appName = "Luma Engine";
    
    // Enable validation layers (debug builds)
    bool enableValidation = true;
    
    // Enable GPU profiling
    bool enableProfiling = false;
    
    // Requested GPU index (for multi-GPU systems)
    u32 gpuIndex = 0;
    
    // Enable debug markers
    bool enableDebugMarkers = true;
};

// Create RHI device (implemented by backends)
using RHICreateDeviceFunc = RHIDevice* (*)(const RHIInitDesc& desc);

// Register RHI backend
void RegisterRHIBackend(const char* name, RHICreateDeviceFunc createFunc);

// Create RHI device by name
RHIDevice* CreateRHIDevice(const char* backendName, const RHIInitDesc& desc);

// Destroy RHI device
void DestroyRHIDevice(RHIDevice* device);

// Get available RHI backends
std::vector<const char*> GetAvailableRHIBackends();

// Get default RHI backend name
const char* GetDefaultRHIBackend();

} // namespace RHI
} // namespace Luma