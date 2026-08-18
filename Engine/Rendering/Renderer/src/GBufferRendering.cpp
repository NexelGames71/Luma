#include "Luma/Renderer/GBufferInfo.h"
#include <string>
#include <fstream>
#include <vector>
#include <map>
#include "Luma/Core/Log.h"
#include "Luma/RHI/RHIContext.h"
#include "Luma/RHI/RHIResources.h"
#include "Luma/Mesh/Mesh.h"
#include "Vulkan/RHI/VulkanRHIDevice.h"
#include "Vulkan/RHI/VulkanRHICommandList.h"
#include "Vulkan/RHI/VulkanRHIResources.h"

namespace Luma {
namespace Renderer2 {

using std::string;

// ============================================================================
// GBuffer
// ============================================================================

GBuffer::GBuffer()
    : m_created(false) {
}

GBuffer::~GBuffer() {
    // The factory is required to destroy any textures the GBuffer created.
    // Without one (e.g. if Initialize failed), the GBuffer just leaks — this
    // matches the existing stub behaviour and avoids dereferencing a null
    // device during shutdown. Real projects always pass a device.
    m_created = false;
}

bool GBuffer::Create(const GBufferDesc& desc, RHI::RHIDevice* device) {
    m_desc = desc;

    if (!device) {
        // Soft success for testing/stubs
        m_created = true;
        return true;
    }

    auto* factory = device->GetContext()->GetResourceFactory();
    if (!factory) {
        LUMA_LOG_ERROR("Deferred", "GBuffer::Create: device has no resource factory");
        return false;
    }

    // Allocate one texture per GBuffer target. Color + normal + material are
    // sampled in the lighting pass; depth is the depth-stencil attachment.
    auto makeTexture = [&](RHI::ETextureFormat format, RHI::ETextureUsage usage,
                            RHI::ETextureFlags flags) -> RHI::RHITexture* {
        RHI::TextureDesc td;
        td.width = m_desc.width;
        td.height = m_desc.height;
        td.mipLevels = 1;
        td.arraySize = 1;
        td.format = format;
        td.usage = usage;
        td.flags = flags;
        return factory->CreateTexture(td);
    };

    m_colorTarget = makeTexture(m_desc.formats.colorFormat,
                                  RHI::ETextureUsage::RenderTarget |
                                      RHI::ETextureUsage::ShaderResource,
                                  RHI::ETextureFlags::RenderTargetable |
                                      RHI::ETextureFlags::ShaderResource);
    m_normalTarget = makeTexture(m_desc.formats.normalFormat,
                                   RHI::ETextureUsage::RenderTarget |
                                       RHI::ETextureUsage::ShaderResource,
                                   RHI::ETextureFlags::RenderTargetable |
                                       RHI::ETextureFlags::ShaderResource);
    m_materialTarget = makeTexture(m_desc.formats.materialFormat,
                                     RHI::ETextureUsage::RenderTarget |
                                         RHI::ETextureUsage::ShaderResource,
                                     RHI::ETextureFlags::RenderTargetable |
                                         RHI::ETextureFlags::ShaderResource);
    m_depthTarget = makeTexture(m_desc.formats.depthFormat,
                                  RHI::ETextureUsage::DepthStencil |
                                      RHI::ETextureUsage::ShaderResource,
                                  RHI::ETextureFlags::DepthStencilTargetable |
                                      RHI::ETextureFlags::ShaderResource);

    if (!m_colorTarget || !m_normalTarget || !m_materialTarget || !m_depthTarget) {
        LUMA_LOG_ERROR("Deferred", "GBuffer::Create: failed to allocate one or "
                                     "more GBuffer targets ({}x{}, {} sample(s))",
                       m_desc.width, m_desc.height, m_desc.samples);
        Destroy(device);
        return false;
    }

    // Create RTV, DSV, and SRV views for the targets
    RHI::TextureSubresourceRange range{};
    range.baseMipLevel = 0;
    range.mipLevels = 1;
    range.baseArrayLayer = 0;
    range.arrayLayers = 1;

    m_colorRTV = factory->CreateRenderTargetView(m_colorTarget, m_desc.formats.colorFormat, range);
    m_normalRTV = factory->CreateRenderTargetView(m_normalTarget, m_desc.formats.normalFormat, range);
    m_materialRTV = factory->CreateRenderTargetView(m_materialTarget, m_desc.formats.materialFormat, range);

    RHI::TextureSubresourceRange depthRange = range;
    m_depthDSV = factory->CreateDepthStencilView(m_depthTarget, m_desc.formats.depthFormat, depthRange);

    m_colorSRV = factory->CreateShaderResourceView(m_colorTarget, m_desc.formats.colorFormat, range);
    m_normalSRV = factory->CreateShaderResourceView(m_normalTarget, m_desc.formats.normalFormat, range);
    m_materialSRV = factory->CreateShaderResourceView(m_materialTarget, m_desc.formats.materialFormat, range);
    m_depthSRV = factory->CreateShaderResourceView(m_depthTarget, m_desc.formats.depthFormat, depthRange);

    if (!m_colorRTV || !m_normalRTV || !m_materialRTV || !m_depthDSV ||
        !m_colorSRV || !m_normalSRV || !m_materialSRV || !m_depthSRV) {
        LUMA_LOG_ERROR("Deferred", "GBuffer::Create: failed to create one or more GBuffer views");
        Destroy(device);
        return false;
    }

    m_created = true;
    return true;
}

bool GBuffer::Resize(u32 width, u32 height, RHI::RHIDevice* device) {
    Destroy(device);
    GBufferDesc next = m_desc;
    next.width = width;
    next.height = height;
    return Create(next, device);
}

void GBuffer::Destroy(RHI::RHIDevice* device) {
    if (device) {
        auto* factory = device->GetContext()->GetResourceFactory();
        if (factory) {
            if (m_colorRTV) factory->DestroyRenderTargetView(m_colorRTV);
            if (m_normalRTV) factory->DestroyRenderTargetView(m_normalRTV);
            if (m_materialRTV) factory->DestroyRenderTargetView(m_materialRTV);
            if (m_depthDSV) factory->DestroyDepthStencilView(m_depthDSV);

            if (m_colorSRV) factory->DestroyShaderResourceView(m_colorSRV);
            if (m_normalSRV) factory->DestroyShaderResourceView(m_normalSRV);
            if (m_materialSRV) factory->DestroyShaderResourceView(m_materialSRV);
            if (m_depthSRV) factory->DestroyShaderResourceView(m_depthSRV);

            if (m_colorTarget) factory->DestroyTexture(m_colorTarget);
            if (m_normalTarget) factory->DestroyTexture(m_normalTarget);
            if (m_materialTarget) factory->DestroyTexture(m_materialTarget);
            if (m_depthTarget) factory->DestroyTexture(m_depthTarget);
        }
    }
    m_colorTarget = nullptr;
    m_normalTarget = nullptr;
    m_materialTarget = nullptr;
    m_depthTarget = nullptr;

    m_colorRTV = nullptr;
    m_normalRTV = nullptr;
    m_materialRTV = nullptr;
    m_depthDSV = nullptr;

    m_colorSRV = nullptr;
    m_normalSRV = nullptr;
    m_materialSRV = nullptr;
    m_depthSRV = nullptr;

    m_created = false;
}

void GBuffer::Clear(RHI::RHICommandList* cmdList) {
    if (!IsValid() || !cmdList) return;

    // Transition targets to RenderTarget/DepthWrite states
    cmdList->ResourceBarrier(m_colorTarget, RHI::EResourceState::RenderTarget);
    cmdList->ResourceBarrier(m_normalTarget, RHI::EResourceState::RenderTarget);
    cmdList->ResourceBarrier(m_materialTarget, RHI::EResourceState::RenderTarget);
    cmdList->ResourceBarrier(m_depthTarget, RHI::EResourceState::DepthWrite);

    // Bind render targets to begin dynamic rendering pass
    RHI::RHIRenderTargetView* rtvs[3] = { m_colorRTV, m_normalRTV, m_materialRTV };
    cmdList->SetRenderTargets(3, rtvs, m_depthDSV);

    // Clear GBuffer targets
    cmdList->ClearRenderTargetView(m_colorRTV, Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    cmdList->ClearRenderTargetView(m_normalRTV, Vec4(0.5f, 0.5f, 0.5f, 1.0f)); // Neutral normal encoded as [0,1]
    cmdList->ClearRenderTargetView(m_materialRTV, Vec4(0.0f, 0.5f, 1.0f, 0.0f)); // metallic=0, specular=0.5, AO=1.0
    cmdList->ClearDepthStencilView(m_depthDSV, 1.0f, 0); // reverse Z depth clear
}

// ============================================================================
// GBuffer Renderer
// ============================================================================

namespace {

VkShaderModule LoadShaderModuleLocal(VkDevice device, const std::string& spirvPath) {
    std::ifstream file(spirvPath, std::ios::binary | std::ios::ate);
    if (!file) {
        LUMA_LOG_ERROR("Deferred", "shader not found: {}", spirvPath);
        return VK_NULL_HANDLE;
    }
    std::streamsize size = file.tellg();
    if (size <= 0 || (size % 4) != 0) {
        LUMA_LOG_ERROR("Deferred", "invalid SPIR-V size ({}): {}", size, spirvPath);
        return VK_NULL_HANDLE;
    }
    file.seekg(0);
    std::vector<uint32_t> code(static_cast<size_t>(size) / 4);
    file.read(reinterpret_cast<char*>(code.data()), size);

    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = static_cast<size_t>(size);
    info.pCode = code.data();

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &info, nullptr, &module) != VK_SUCCESS) {
        LUMA_LOG_ERROR("Deferred", "failed to create shader module for {}", spirvPath);
        return VK_NULL_HANDLE;
    }
    return module;
}

struct GBufferPush {
    Mat4 model;
    Vec4 albedo;
    Vec4 material;
};

} // namespace

GBufferRenderer::GBufferRenderer()
    : m_gBuffer(nullptr), m_device(nullptr), m_initialized(false) {
}

GBufferRenderer::~GBufferRenderer() {
    Shutdown();
}

bool GBufferRenderer::Initialize(RHI::RHIDevice* device) {
    m_device = device;
    m_initialized = InitPipeline();
    if (m_initialized) {
        CreatePrimitives();
    }
    return m_initialized;
}

void GBufferRenderer::Shutdown() {
    if (m_initialized) {
        CleanupPrimitives();
        CleanupPipeline();
    }
    m_device = nullptr;
    m_initialized = false;
}

bool GBufferRenderer::InitPipeline() {
    if (!m_device) return false;

    auto* vkDevice = dynamic_cast<RHI::VulkanRHIDevice*>(m_device);
    if (!vkDevice) return false;

    VkDevice device = vkDevice->LogicalHandle();
    std::string shaderDir = vkDevice->GetShaderDir();

    VkShaderModule vert = LoadShaderModuleLocal(device, shaderDir + "/gbuffer.vert.spv");
    VkShaderModule frag = LoadShaderModuleLocal(device, shaderDir + "/gbuffer.frag.spv");
    if (!vert || !frag) {
        LUMA_LOG_ERROR("Deferred", "Failed to load GBuffer shaders from {}", shaderDir);
        return false;
    }

    // Shader stages
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    // Descriptor set layout
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_uboLayout) != VK_SUCCESS) {
        return false;
    }

    // Descriptor pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        return false;
    }

    // Descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_uboLayout;
    if (vkAllocateDescriptorSets(device, &allocInfo, &m_descriptorSet) != VK_SUCCESS) {
        return false;
    }

    // Camera UBO
    RHI::BufferDesc bufDesc;
    bufDesc.size = 256;
    bufDesc.usage = RHI::EBufferUsage::Uniform;
    bufDesc.cpuAccess = RHI::EBufferCPUAccess::Write;
    m_cameraUBO = m_device->GetContext()->GetResourceFactory()->CreateBuffer(bufDesc);
    if (!m_cameraUBO) return false;

    // Update descriptor set
    auto* vkBuffer = static_cast<RHI::VulkanRHIBuffer*>(m_cameraUBO);
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = vkBuffer->Handle();
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(Mat4) + sizeof(Vec4); // viewProj + camPos

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    // Push constant range
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(GBufferPush);

    VkPipelineLayoutCreateInfo pipLayoutInfo{};
    pipLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipLayoutInfo.setLayoutCount = 1;
    pipLayoutInfo.pSetLayouts = &m_uboLayout;
    pipLayoutInfo.pushConstantRangeCount = 1;
    pipLayoutInfo.pPushConstantRanges = &pushRange;
    if (vkCreatePipelineLayout(device, &pipLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        return false;
    }

    // Vertex input state
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(MeshVertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrDescs[2]{};
    attrDescs[0].binding = 0;
    attrDescs[0].location = 0;
    attrDescs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescs[0].offset = offsetof(MeshVertex, position);
    attrDescs[1].binding = 0;
    attrDescs[1].location = 1;
    attrDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescs[1].offset = offsetof(MeshVertex, normal);

    VkPipelineVertexInputStateCreateInfo vin{};
    vin.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vin.vertexBindingDescriptionCount = 1;
    vin.pVertexBindingDescriptions = &bindingDesc;
    vin.vertexAttributeDescriptionCount = 2;
    vin.pVertexAttributeDescriptions = attrDescs;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState bas[3]{};
    for (int i = 0; i < 3; ++i) {
        bas[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        bas[i].blendEnable = VK_FALSE;
    }

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 3;
    cb.pAttachments = bas;

    VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynState{};
    dynState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynState.dynamicStateCount = 2;
    dynState.pDynamicStates = dyn;

    VkFormat colorFormats[3] = {
        VK_FORMAT_R8G8B8A8_UNORM,       // Albedo
        VK_FORMAT_R16G16B16A16_SFLOAT,  // Normal
        VK_FORMAT_R8G8B8A8_UNORM        // Material
    };
    VkPipelineRenderingCreateInfo ri{};
    ri.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    ri.colorAttachmentCount = 3;
    ri.pColorAttachmentFormats = colorFormats;
    ri.depthAttachmentFormat = VK_FORMAT_D24_UNORM_S8_UINT;

    VkGraphicsPipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.pNext = &ri;
    info.stageCount = 2;
    info.pStages = stages;
    info.pVertexInputState = &vin;
    info.pInputAssemblyState = &ia;
    info.pViewportState = &vp;
    info.pRasterizationState = &rs;
    info.pMultisampleState = &ms;
    info.pDepthStencilState = &ds;
    info.pColorBlendState = &cb;
    info.pDynamicState = &dynState;
    info.layout = m_pipelineLayout;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &m_pipeline) != VK_SUCCESS) {
        return false;
    }

    vkDestroyShaderModule(device, vert, nullptr);
    vkDestroyShaderModule(device, frag, nullptr);
    return true;
}

void GBufferRenderer::CleanupPipeline() {
    if (m_device) {
        auto* vkDevice = dynamic_cast<RHI::VulkanRHIDevice*>(m_device);
        if (vkDevice) {
            VkDevice device = vkDevice->LogicalHandle();
            if (m_pipeline) vkDestroyPipeline(device, m_pipeline, nullptr);
            if (m_pipelineLayout) vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
            if (m_uboLayout) vkDestroyDescriptorSetLayout(device, m_uboLayout, nullptr);
            if (m_descriptorPool) vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
        }
        if (m_cameraUBO) {
            m_device->GetContext()->GetResourceFactory()->DestroyBuffer(m_cameraUBO);
        }
    }
    m_pipeline = VK_NULL_HANDLE;
    m_pipelineLayout = VK_NULL_HANDLE;
    m_uboLayout = VK_NULL_HANDLE;
    m_descriptorPool = VK_NULL_HANDLE;
    m_descriptorSet = VK_NULL_HANDLE;
    m_cameraUBO = nullptr;
}

void GBufferRenderer::CreatePrimitives() {
    const MeshPrimitive kinds[4] = {
        MeshPrimitive::Cube, MeshPrimitive::Plane, MeshPrimitive::Sphere,
        MeshPrimitive::Cylinder
    };
    auto* factory = m_device->GetContext()->GetResourceFactory();
    for (u32 i = 0; i < 4; ++i) {
        MeshData data = BuildPrimitive(kinds[i]);
        
        RHI::BufferDesc vd;
        vd.size = sizeof(MeshVertex) * data.vertices.size();
        vd.usage = RHI::EBufferUsage::Vertex;
        vd.cpuAccess = RHI::EBufferCPUAccess::Write;
        m_primitives[i].vertexBuffer = factory->CreateBuffer(vd);
        if (m_primitives[i].vertexBuffer) {
            m_primitives[i].vertexBuffer->UpdateData(data.vertices.data(), vd.size);
        }

        RHI::BufferDesc id;
        id.size = sizeof(u32) * data.indices.size();
        id.usage = RHI::EBufferUsage::Index;
        id.cpuAccess = RHI::EBufferCPUAccess::Write;
        m_primitives[i].indexBuffer = factory->CreateBuffer(id);
        if (m_primitives[i].indexBuffer) {
            m_primitives[i].indexBuffer->UpdateData(data.indices.data(), id.size);
        }
        m_primitives[i].indexCount = static_cast<u32>(data.indices.size());
        m_primitives[i].vertexCount = static_cast<u32>(data.vertices.size());
    }
}

void GBufferRenderer::CleanupPrimitives() {
    auto* factory = m_device->GetContext()->GetResourceFactory();
    for (u32 i = 0; i < 4; ++i) {
        if (m_primitives[i].vertexBuffer) factory->DestroyBuffer(m_primitives[i].vertexBuffer);
        if (m_primitives[i].indexBuffer) factory->DestroyBuffer(m_primitives[i].indexBuffer);
        m_primitives[i].vertexBuffer = nullptr;
        m_primitives[i].indexBuffer = nullptr;
    }
    for (auto& pair : m_customMeshes) {
        if (pair.second.vertexBuffer) factory->DestroyBuffer(pair.second.vertexBuffer);
        if (pair.second.indexBuffer) factory->DestroyBuffer(pair.second.indexBuffer);
    }
    m_customMeshes.clear();
}

const GBufferRenderer::RhiMesh* GBufferRenderer::GetOrCreateCustomMesh(
    const Math::Vec3* vertices, u32 vertexCount, const u32* indices, u32 indexCount) {
    LUMA_LOG_INFO("Deferred", "GetOrCreateCustomMesh: vertexCount={}, indexCount={}", vertexCount, indexCount);
    
    // Validate vertex count - reject obviously invalid values
    if (vertexCount == 0 || vertexCount > 1000000) {
        LUMA_LOG_ERROR("Deferred", "GetOrCreateCustomMesh: Invalid vertexCount: {}", vertexCount);
        return nullptr;
    }
    
    if (!vertices) {
        LUMA_LOG_ERROR("Deferred", "GetOrCreateCustomMesh: vertices pointer is null");
        return nullptr;
    }
    
    // Validate index count
    if (indexCount > 1000000) {
        LUMA_LOG_ERROR("Deferred", "GetOrCreateCustomMesh: Invalid indexCount: {}", indexCount);
        return nullptr;
    }

    // TODO: Implement proper caching - for now, always create new mesh
    // The pointer-based cache is problematic, disable for now
    // auto it = m_customMeshes.find(vertices);
    // if (it != m_customMeshes.end()) {
    //     LUMA_LOG_INFO("Deferred", "GetOrCreateCustomMesh: Found in cache");
    //     return &it->second;
    // }

    LUMA_LOG_INFO("Deferred", "GetOrCreateCustomMesh: Creating new mesh");
    RhiMesh mesh;
    mesh.vertexCount = vertexCount;
    mesh.indexCount = indexCount;

    LUMA_LOG_INFO("Deferred", "GetOrCreateCustomMesh: Converting vertices");
    std::vector<MeshVertex> meshVertices(vertexCount);
    for (u32 i = 0; i < vertexCount; ++i) {
        meshVertices[i].position = vertices[i];
        meshVertices[i].normal = Math::Vec3(0.0f, 0.0f, 1.0f);
    }

    LUMA_LOG_INFO("Deferred", "GetOrCreateCustomMesh: Getting resource factory");
    auto* factory = m_device->GetContext()->GetResourceFactory();
    if (!factory) {
        LUMA_LOG_ERROR("Deferred", "GetOrCreateCustomMesh: Resource factory is null");
        return nullptr;
    }
    
    LUMA_LOG_INFO("Deferred", "GetOrCreateCustomMesh: Creating vertex buffer");
    RHI::BufferDesc vd;
    vd.size = sizeof(MeshVertex) * vertexCount;
    vd.usage = RHI::EBufferUsage::Vertex;
    vd.cpuAccess = RHI::EBufferCPUAccess::Write;
    mesh.vertexBuffer = factory->CreateBuffer(vd);
    if (mesh.vertexBuffer) {
        LUMA_LOG_INFO("Deferred", "GetOrCreateCustomMesh: Updating vertex buffer data");
        mesh.vertexBuffer->UpdateData(meshVertices.data(), vd.size);
    } else {
        LUMA_LOG_ERROR("Deferred", "GetOrCreateCustomMesh: Failed to create vertex buffer");
    }

    if (indices && indexCount > 0) {
        LUMA_LOG_INFO("Deferred", "GetOrCreateCustomMesh: Creating index buffer");
        RHI::BufferDesc id;
        id.size = sizeof(u32) * indexCount;
        id.usage = RHI::EBufferUsage::Index;
        id.cpuAccess = RHI::EBufferCPUAccess::Write;
        mesh.indexBuffer = factory->CreateBuffer(id);
        if (mesh.indexBuffer) {
            LUMA_LOG_INFO("Deferred", "GetOrCreateCustomMesh: Updating index buffer data");
            mesh.indexBuffer->UpdateData(indices, id.size);
        } else {
            LUMA_LOG_ERROR("Deferred", "GetOrCreateCustomMesh: Failed to create index buffer");
        }
    }

    LUMA_LOG_INFO("Deferred", "GetOrCreateCustomMesh: Adding to cache");
    m_customMeshes[vertices] = mesh;
    LUMA_LOG_INFO("Deferred", "GetOrCreateCustomMesh: Completed");
    return &m_customMeshes[vertices];
}

void GBufferRenderer::RenderGeometry(RHI::RHICommandList* cmdList, const Luma::SceneView& sceneView) {
    if (!m_gBuffer || !m_gBuffer->IsValid() || !m_initialized) {
        LUMA_LOG_ERROR("Deferred", "RenderGeometry: GBuffer invalid or not initialized");
        return;
    }

    LUMA_LOG_INFO("Deferred", "RenderGeometry: Starting geometry render");

    auto* vkList = dynamic_cast<RHI::VulkanRHICommandList*>(cmdList);
    if (!vkList) {
        LUMA_LOG_ERROR("Deferred", "RenderGeometry: Failed to cast to VulkanRHICommandList");
        return;
    }
    VkCommandBuffer cmd = vkList->Handle();
    LUMA_LOG_INFO("Deferred", "RenderGeometry: Got command buffer");

    // Map camera viewProj and camPos into UBO
    struct CameraUBO {
        Mat4 viewProj;
        Vec4 camPos;
    } uboData;
    uboData.viewProj = sceneView.view; // viewProj
    Mat4 invView = Inverse(sceneView.view);
    uboData.camPos = Vec4(invView.m[12], invView.m[13], invView.m[14], 1.0f);
    
    LUMA_LOG_INFO("Deferred", "RenderGeometry: Updating camera UBO");
    m_cameraUBO->UpdateData(&uboData, sizeof(uboData), 0);
    LUMA_LOG_INFO("Deferred", "RenderGeometry: Camera UBO updated");

    // Bind pipeline and descriptor set
    LUMA_LOG_INFO("Deferred", "RenderGeometry: Binding pipeline");
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
                            0, 1, &m_descriptorSet, 0, nullptr);
    LUMA_LOG_INFO("Deferred", "RenderGeometry: Pipeline and descriptor set bound");

    // Set dynamic states
    LUMA_LOG_INFO("Deferred", "RenderGeometry: Setting viewport and scissor");
    VkViewport viewport{0, 0, static_cast<f32>(m_gBuffer->GetWidth()), static_cast<f32>(m_gBuffer->GetHeight()), 0.0f, 1.0f};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{{0, 0}, {m_gBuffer->GetWidth(), m_gBuffer->GetHeight()}};
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    LUMA_LOG_INFO("Deferred", "RenderGeometry: Viewport and scissor set");

    // Render mesh instances
    LUMA_LOG_INFO("Deferred", "RenderGeometry: Rendering {} instances", sceneView.instanceCount);
    
    // Validate instance count to prevent garbage data crashes
    if (sceneView.instanceCount > 10000) {
        LUMA_LOG_ERROR("Deferred", "RenderGeometry: Invalid instance count: {}", sceneView.instanceCount);
        return;
    }
    
    for (u32 i = 0; i < sceneView.instanceCount; ++i) {
        const SceneInstance& inst = sceneView.instances[i];

        // Validate instance data
        if (inst.customVertexCount > 1000000) {
            LUMA_LOG_ERROR("Deferred", "RenderGeometry: Invalid vertexCount in instance {}: {}", i, inst.customVertexCount);
            continue;
        }
        
        if (inst.customIndexCount > 1000000) {
            LUMA_LOG_ERROR("Deferred", "RenderGeometry: Invalid indexCount in instance {}: {}", i, inst.customIndexCount);
            continue;
        }

        // Retrieve mesh
        const RhiMesh* mesh = nullptr;
        if (inst.customMeshValid && inst.customVertices) {
            LUMA_LOG_INFO("Deferred", "RenderGeometry: Getting custom mesh for instance {}", i);
            mesh = GetOrCreateCustomMesh(inst.customVertices, inst.customVertexCount, inst.customIndices, inst.customIndexCount);
        } else {
            u32 prim = static_cast<u32>(inst.primitive);
            if (prim < 4) {
                mesh = &m_primitives[prim];
            }
        }

        if (!mesh || !mesh->vertexBuffer) {
            LUMA_LOG_WARN("Deferred", "RenderGeometry: Invalid mesh or vertex buffer for instance {}", i);
            continue;
        }

        // Push model transform and material properties
        GBufferPush push{};
        push.model = inst.model;
        push.albedo = Vec4(inst.albedo.x, inst.albedo.y, inst.albedo.z, inst.metallic);
        push.material = Vec4(inst.roughness, 0.5f, 1.0f, 0.0f); // roughness, specular, AO, unused

        vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);

        // Bind and draw
        VkDeviceSize offset = 0;
        VkBuffer vkVertBuf = static_cast<RHI::VulkanRHIBuffer*>(mesh->vertexBuffer)->Handle();
        vkCmdBindVertexBuffers(cmd, 0, 1, &vkVertBuf, &offset);

        if (mesh->indexBuffer) {
            VkBuffer vkIdxBuf = static_cast<RHI::VulkanRHIBuffer*>(mesh->indexBuffer)->Handle();
            vkCmdBindIndexBuffer(cmd, vkIdxBuf, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, mesh->indexCount, 1, 0, 0, 0);
        } else {
            vkCmdDraw(cmd, mesh->vertexCount, 1, 0, 0);
        }
    }
    
    LUMA_LOG_INFO("Deferred", "RenderGeometry: Completed geometry render");
}

void GBufferRenderer::Prepare(RHI::RHICommandList* cmdList) {
    if (!m_gBuffer || !m_gBuffer->IsValid()) {
        return;
    }
    m_gBuffer->Clear(cmdList);
}

void GBufferRenderer::SetRenderTargets(RHI::RHICommandList* cmdList) {
    if (!m_gBuffer || !m_gBuffer->IsValid()) {
        return;
    }
    RHI::RHIRenderTargetView* rtvs[3] = {
        m_gBuffer->GetColorRTV(),
        m_gBuffer->GetNormalRTV(),
        m_gBuffer->GetMaterialRTV()
    };
    cmdList->SetRenderTargets(3, rtvs, m_gBuffer->GetDepthDSV());
}

Vec4 GBufferRenderer::PackNormalRoughness(const Vec3& normal, f32 roughness) {
    return Vec4(normal.x, normal.y, normal.z, roughness);
}

Vec4 GBufferRenderer::PackMaterialMetallicAO(f32 metallic, f32 specular, f32 ao) {
    return Vec4(metallic, specular, ao, 0.0f);
}

// ============================================================================
// GBuffer Pool
// ============================================================================

GBufferPool::GBufferPool() {
}

GBufferPool::~GBufferPool() {
    // TODO: Destroy all GBuffers - requires device reference
    m_gbuffers.clear();
}

GBuffer* GBufferPool::GetGBuffer(u32 index) const {
    if (index < m_gbuffers.size()) {
        return m_gbuffers[index];
    }
    return nullptr;
}

GBuffer* GBufferPool::GetOrCreateGBuffer(u32 width, u32 height, const GBufferFormats& formats, RHI::RHIDevice* device) {
    // Try to find a matching GBuffer
    for (auto* gbuffer : m_gbuffers) {
        if (gbuffer->GetWidth() == width && gbuffer->GetHeight() == height) {
            return gbuffer;
        }
    }
    
    // Create new GBuffer
    GBufferDesc desc;
    desc.width = width;
    desc.height = height;
    desc.formats = formats;
    
    auto* gbuffer = new GBuffer();
    gbuffer->Create(desc, device);
    m_gbuffers.push_back(gbuffer);
    return gbuffer;
}

void GBufferPool::ResizeAll(u32 width, u32 height, RHI::RHIDevice* device) {
    for (auto* gbuffer : m_gbuffers) {
        gbuffer->Resize(width, height, device);
    }
}

void GBufferPool::ClearAll(RHI::RHICommandList* cmdList) {
    for (auto* gbuffer : m_gbuffers) {
        gbuffer->Clear(cmdList);
    }
}

void GBufferPool::DestroyAll(RHI::RHIDevice* device) {
    for (auto* gbuffer : m_gbuffers) {
        gbuffer->Destroy(device);
        delete gbuffer;
    }
    m_gbuffers.clear();
}

} // namespace Renderer2
} // namespace Luma