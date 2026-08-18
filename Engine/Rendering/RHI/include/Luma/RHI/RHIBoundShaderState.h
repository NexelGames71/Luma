#pragma once

#include <memory>
#include <vector>
#include <string>

#include "Luma/Core/Types.h"
#include "Luma/RHI/RHIShader.h"
#include "Luma/RHI/RHIPipeline.h"

// RHI bound shader state. Inspired by UE5's bound shader state system but
// simplified for Luma's architecture. Bound shader state encapsulates the
// shader programs and their resource bindings for efficient state changes.

namespace Luma {
namespace RHI {

using std::vector;
using std::string;

// Forward declarations
class RHIBuffer;
class RHITexture;
class RHISamplerState;

// ============================================================================
// Shader Resource Binding
// ============================================================================

// Shader resource binding type
enum class EShaderResourceType : u32 {
    UniformBuffer,
    StorageBuffer,
    SampledTexture,
    StorageTexture,
    Sampler,
    InputAttachment,
};

// Shader resource binding description
struct ShaderResourceBinding {
    u32 set = 0;
    u32 binding = 0;
    EShaderResourceType type = EShaderResourceType::SampledTexture;
    EShaderStage stages = EShaderStage::Vertex;
    const char* name = nullptr;
    
    // Resource binding (for runtime binding)
    RHIResource* resource = nullptr;
    RHISamplerState* sampler = nullptr;
};

// ============================================================================
// Vertex Attribute
// ============================================================================

// Vertex attribute description
struct VertexAttribute {
    u32 location = 0;
    u32 offset = 0;
    ETextureFormat format = ETextureFormat::Unknown;
    const char* semanticName = nullptr;
    u32 semanticIndex = 0;
};

// Vertex buffer binding
struct VertexBufferBinding {
    u32 slot = 0;
    u32 stride = 0;
    EBufferUsage inputRate = EBufferUsage::Vertex;  // Vertex or Instance
};

// ============================================================================
// Bound Shader State
// ============================================================================

// Bound shader state description
struct BoundShaderStateDesc {
    // Shaders
    RHIShader* vertexShader = nullptr;
    RHIShader* fragmentShader = nullptr;
    RHIShader* geometryShader = nullptr;
    RHIShader* tessControlShader = nullptr;
    RHIShader* tessEvalShader = nullptr;
    
    // Vertex attributes
    const VertexAttribute* vertexAttributes = nullptr;
    u32 vertexAttributeCount = 0;
    
    // Vertex buffer bindings
    const VertexBufferBinding* vertexBufferBindings = nullptr;
    u32 vertexBufferBindingCount = 0;
    
    // Resource bindings
    const ShaderResourceBinding* resourceBindings = nullptr;
    u32 resourceBindingCount = 0;
    
    // Debug name
    string name;
};

// Bound shader state interface
class RHIBoundShaderState {
public:
    virtual ~RHIBoundShaderState() = default;
    
    const BoundShaderStateDesc& GetDesc() const { return m_desc; }
    
    // Get the vertex shader
    RHIShader* GetVertexShader() const { return m_desc.vertexShader; }
    
    // Get the fragment shader
    RHIShader* GetFragmentShader() const { return m_desc.fragmentShader; }
    
    // Get the geometry shader
    RHIShader* GetGeometryShader() const { return m_desc.geometryShader; }
    
    // Get the tessellation control shader
    RHIShader* GetTessControlShader() const { return m_desc.tessControlShader; }
    
    // Get the tessellation evaluation shader
    RHIShader* GetTessEvalShader() const { return m_desc.tessEvalShader; }
    
protected:
    BoundShaderStateDesc m_desc;
};

// ============================================================================
// Bound Shader State Factory
// ============================================================================

// Bound shader state creation interface (implemented by RHI backends)
class RHIBoundShaderStateFactory {
public:
    virtual ~RHIBoundShaderStateFactory() = default;
    
    // Create bound shader state
    virtual RHIBoundShaderState* CreateBoundShaderState(const BoundShaderStateDesc& desc) = 0;
    
    // Destroy bound shader state
    virtual void DestroyBoundShaderState(RHIBoundShaderState* bss) = 0;
};

} // namespace RHI
} // namespace Luma