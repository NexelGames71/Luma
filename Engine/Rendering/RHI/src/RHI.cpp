#include "Luma/RHI/RHITypes.h"
#include "Luma/RHI/RHIContext.h"

#include <cstring>
#include <string>
#include <vector>

namespace Luma {
namespace RHI {

using std::string;
using std::vector;

// ============================================================================
// Format Utility Functions
// ============================================================================

u32 GetFormatSize(ETextureFormat format) {
    switch (format) {
        case ETextureFormat::R8_UNORM:
        case ETextureFormat::R8_SNORM:
        case ETextureFormat::R8_UINT:
        case ETextureFormat::R8_SINT:
            return 1;
            
        case ETextureFormat::R16_UNORM:
        case ETextureFormat::R16_SNORM:
        case ETextureFormat::R16_UINT:
        case ETextureFormat::R16_SINT:
        case ETextureFormat::R16_FLOAT:
            return 2;
            
        case ETextureFormat::R8G8_UNORM:
        case ETextureFormat::R8G8_SNORM:
        case ETextureFormat::R8G8_UINT:
        case ETextureFormat::R8G8_SINT:
            return 2;
            
        case ETextureFormat::R16G16_UNORM:
        case ETextureFormat::R16G16_SNORM:
        case ETextureFormat::R16G16_UINT:
        case ETextureFormat::R16G16_SINT:
        case ETextureFormat::R16G16_FLOAT:
            return 4;
            
        case ETextureFormat::R32_UINT:
        case ETextureFormat::R32_SINT:
        case ETextureFormat::R32_FLOAT:
            return 4;
            
        case ETextureFormat::R8G8B8A8_UNORM:
        case ETextureFormat::R8G8B8A8_SRGB:
        case ETextureFormat::R8G8B8A8_UINT:
        case ETextureFormat::R8G8B8A8_SINT:
            return 4;
            
        case ETextureFormat::R16G16B16A16_UNORM:
        case ETextureFormat::R16G16B16A16_SNORM:
        case ETextureFormat::R16G16B16A16_UINT:
        case ETextureFormat::R16G16B16A16_SINT:
        case ETextureFormat::R16G16B16A16_FLOAT:
            return 8;
            
        case ETextureFormat::R32G32_UINT:
        case ETextureFormat::R32G32_SINT:
        case ETextureFormat::R32G32_FLOAT:
            return 8;
            
        case ETextureFormat::R32G32B32A32_UINT:
        case ETextureFormat::R32G32B32A32_SINT:
        case ETextureFormat::R32G32B32A32_FLOAT:
            return 16;
            
        case ETextureFormat::D16_UNORM:
            return 2;
            
        case ETextureFormat::D24_UNORM_S8_UINT:
            return 4;
            
        case ETextureFormat::D32_FLOAT:
        case ETextureFormat::D32_FLOAT_S8_UINT:
            return 4;
            
        case ETextureFormat::BC1_UNORM:
        case ETextureFormat::BC1_SRGB:
            return 8;  // 4x4 block
            
        case ETextureFormat::BC3_UNORM:
        case ETextureFormat::BC3_SRGB:
        case ETextureFormat::BC5_UNORM:
        case ETextureFormat::BC5_SNORM:
            return 16;  // 4x4 block
            
        case ETextureFormat::BC4_UNORM:
        case ETextureFormat::BC4_SNORM:
            return 8;  // 4x4 block
            
        case ETextureFormat::BC6H_UFLOAT:
        case ETextureFormat::BC6H_SFLOAT:
        case ETextureFormat::BC7_UNORM:
        case ETextureFormat::BC7_SRGB:
            return 16;  // 4x4 block
            
        default:
            return 0;
    }
}

u32 GetFormatComponentCount(ETextureFormat format) {
    switch (format) {
        case ETextureFormat::R8_UNORM:
        case ETextureFormat::R8_SNORM:
        case ETextureFormat::R8_UINT:
        case ETextureFormat::R8_SINT:
        case ETextureFormat::R16_UNORM:
        case ETextureFormat::R16_SNORM:
        case ETextureFormat::R16_UINT:
        case ETextureFormat::R16_SINT:
        case ETextureFormat::R16_FLOAT:
        case ETextureFormat::R32_UINT:
        case ETextureFormat::R32_SINT:
        case ETextureFormat::R32_FLOAT:
        case ETextureFormat::D16_UNORM:
        case ETextureFormat::D32_FLOAT:
            return 1;
            
        case ETextureFormat::R8G8_UNORM:
        case ETextureFormat::R8G8_SNORM:
        case ETextureFormat::R8G8_UINT:
        case ETextureFormat::R8G8_SINT:
        case ETextureFormat::R16G16_UNORM:
        case ETextureFormat::R16G16_SNORM:
        case ETextureFormat::R16G16_UINT:
        case ETextureFormat::R16G16_SINT:
        case ETextureFormat::R16G16_FLOAT:
        case ETextureFormat::R32G32_UINT:
        case ETextureFormat::R32G32_SINT:
        case ETextureFormat::R32G32_FLOAT:
            return 2;
            
        case ETextureFormat::R8G8B8A8_UNORM:
        case ETextureFormat::R8G8B8A8_SRGB:
        case ETextureFormat::R8G8B8A8_UINT:
        case ETextureFormat::R8G8B8A8_SINT:
        case ETextureFormat::R16G16B16A16_UNORM:
        case ETextureFormat::R16G16B16A16_SNORM:
        case ETextureFormat::R16G16B16A16_UINT:
        case ETextureFormat::R16G16B16A16_SINT:
        case ETextureFormat::R16G16B16A16_FLOAT:
        case ETextureFormat::R32G32B32A32_UINT:
        case ETextureFormat::R32G32B32A32_SINT:
        case ETextureFormat::R32G32B32A32_FLOAT:
        case ETextureFormat::D24_UNORM_S8_UINT:
        case ETextureFormat::D32_FLOAT_S8_UINT:
            return 4;
            
        default:
            return 0;
    }
}

bool IsDepthStencilFormat(ETextureFormat format) {
    return format == ETextureFormat::D16_UNORM ||
           format == ETextureFormat::D24_UNORM_S8_UINT ||
           format == ETextureFormat::D32_FLOAT ||
           format == ETextureFormat::D32_FLOAT_S8_UINT;
}

bool IsCompressedFormat(ETextureFormat format) {
    return format == ETextureFormat::BC1_UNORM ||
           format == ETextureFormat::BC1_SRGB ||
           format == ETextureFormat::BC3_UNORM ||
           format == ETextureFormat::BC3_SRGB ||
           format == ETextureFormat::BC4_UNORM ||
           format == ETextureFormat::BC4_SNORM ||
           format == ETextureFormat::BC5_UNORM ||
           format == ETextureFormat::BC5_SNORM ||
           format == ETextureFormat::BC6H_UFLOAT ||
           format == ETextureFormat::BC6H_SFLOAT ||
           format == ETextureFormat::BC7_UNORM ||
           format == ETextureFormat::BC7_SRGB;
}

bool IsSRGBFormat(ETextureFormat format) {
    return format == ETextureFormat::R8G8B8A8_SRGB ||
           format == ETextureFormat::BC1_SRGB ||
           format == ETextureFormat::BC3_SRGB ||
           format == ETextureFormat::BC7_SRGB;
}

ETextureFormat GetNonSRGBFormat(ETextureFormat format) {
    switch (format) {
        case ETextureFormat::R8G8B8A8_SRGB:
            return ETextureFormat::R8G8B8A8_UNORM;
        case ETextureFormat::BC1_SRGB:
            return ETextureFormat::BC1_UNORM;
        case ETextureFormat::BC3_SRGB:
            return ETextureFormat::BC3_UNORM;
        case ETextureFormat::BC7_SRGB:
            return ETextureFormat::BC7_UNORM;
        default:
            return format;
    }
}

ETextureFormat GetSRGBFormat(ETextureFormat format) {
    switch (format) {
        case ETextureFormat::R8G8B8A8_UNORM:
            return ETextureFormat::R8G8B8A8_SRGB;
        case ETextureFormat::BC1_UNORM:
            return ETextureFormat::BC1_SRGB;
        case ETextureFormat::BC3_UNORM:
            return ETextureFormat::BC3_SRGB;
        case ETextureFormat::BC7_UNORM:
            return ETextureFormat::BC7_SRGB;
        default:
            return format;
    }
}

// ============================================================================
// RHI Initialization (Stub Implementation)
// ============================================================================

// Global RHI state
static RHIDevice* g_rhiDevice = nullptr;
static bool g_rhiInitialized = false;

// Backend registry
struct BackendEntry {
    string name;
    RHICreateDeviceFunc createFunc;
};

static vector<BackendEntry> g_backends;

void RegisterRHIBackend(const char* name, RHICreateDeviceFunc createFunc) {
    BackendEntry entry;
    entry.name = name;
    entry.createFunc = createFunc;
    g_backends.push_back(entry);
}

RHIDevice* CreateRHIDevice(const char* backendName, const RHIInitDesc& desc) {
    for (const auto& entry : g_backends) {
        if (entry.name == backendName) {
            return entry.createFunc(desc);
        }
    }
    return nullptr;
}

void DestroyRHIDevice(RHIDevice* device) {
    if (device) {
        delete device;
    }
}

vector<const char*> GetAvailableRHIBackends() {
    vector<const char*> names;
    for (const auto& entry : g_backends) {
        names.push_back(entry.name.c_str());
    }
    return names;
}

const char* GetDefaultRHIBackend() {
    // Default to Vulkan if available
    for (const auto& entry : g_backends) {
        if (entry.name == "Vulkan") {
            return "Vulkan";
        }
    }
    return g_backends.empty() ? nullptr : g_backends[0].name.c_str();
}

bool InitializeRHI(const char* backendName, const RHIInitDesc& desc) {
    if (g_rhiInitialized) {
        return true;
    }
    
    // Create RHI device
    g_rhiDevice = CreateRHIDevice(backendName, desc);
    if (!g_rhiDevice) {
        return false;
    }
    
    g_rhiInitialized = true;
    return true;
}

void ShutdownRHI() {
    if (g_rhiDevice) {
        DestroyRHIDevice(g_rhiDevice);
        g_rhiDevice = nullptr;
    }
    g_rhiInitialized = false;
}

RHIDevice* GetRHIDevice() {
    return g_rhiDevice;
}

RHIContext* GetRHIContext() {
    return g_rhiDevice ? g_rhiDevice->GetContext() : nullptr;
}

bool IsRHIInitialized() {
    return g_rhiInitialized;
}

} // namespace RHI
} // namespace Luma