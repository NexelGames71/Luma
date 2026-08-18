#include "Luma/Renderer/Lighting.h"
#include "Luma/RHI/RHIContext.h"
#include "Luma/RHI/RHIResources.h"
#include <fstream>
#include <vector>
#include "Vulkan/RHI/VulkanRHIDevice.h"
#include "Vulkan/RHI/VulkanRHICommandList.h"
#include "Vulkan/RHI/VulkanRHIResources.h"

namespace Luma {
namespace Renderer2 {

// ============================================================================
// Lighting Renderer helpers
// ============================================================================

namespace {

static VkShaderModule LoadLightingShader(VkDevice device, const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return VK_NULL_HANDLE;
    std::streamsize size = file.tellg();
    if (size <= 0 || (size % 4) != 0) return VK_NULL_HANDLE;
    file.seekg(0);
    std::vector<uint32_t> code(static_cast<size_t>(size) / 4);
    file.read(reinterpret_cast<char*>(code.data()), size);
    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = static_cast<size_t>(size);
    ci.pCode = code.data();
    VkShaderModule mod = VK_NULL_HANDLE;
    vkCreateShaderModule(device, &ci, nullptr, &mod);
    return mod;
}

#define MAX_LIGHTS 16
struct GpuLight {
    Vec4 posType;   // xyz pos, w type
    Vec4 dirRange;  // xyz dir, w range
    Vec4 color;     // rgb color, w intensity
    Vec4 spot;      // x cosInner, y cosOuter
};
struct LightingUBO {
    Mat4 invViewProj;
    Vec4 camPos;
    Vec4 sunDir;
    Vec4 sunColor;
    Vec4 ambientColor;
    Vec4 params;        // x=lightCount
    GpuLight lights[MAX_LIGHTS];
};
static_assert(sizeof(LightingUBO) <= 2048, "LightingUBO too large");

} // namespace

// ============================================================================
// LightingRenderer
// ============================================================================

LightingRenderer::LightingRenderer()
    : m_gBuffer(nullptr) {
}

LightingRenderer::~LightingRenderer() {
    Shutdown();
    ClearLights();
}

void LightingRenderer::AddLight(const LightData& light) {
    m_lights.push_back(light);
}

void LightingRenderer::RemoveLight(u32 index) {
    if (index < m_lights.size()) {
        m_lights.erase(m_lights.begin() + index);
    }
}

void LightingRenderer::ClearLights() {
    m_lights.clear();
}

bool LightingRenderer::Initialize(RHI::RHIDevice* device, LightAccumulationBuffer* accumBuffer) {
    m_device = device;
    m_accumBuffer = accumBuffer;
    m_initialized = InitPipeline();
    return m_initialized;
}

void LightingRenderer::Shutdown() {
    if (m_initialized) {
        CleanupPipeline();
    }
    m_device = nullptr;
    m_accumBuffer = nullptr;
    m_initialized = false;
}

bool LightingRenderer::InitPipeline() {
    if (!m_device) return false;
    auto* vkDev = dynamic_cast<RHI::VulkanRHIDevice*>(m_device);
    if (!vkDev) return false;
    VkDevice dev = vkDev->LogicalHandle();
    std::string dir = vkDev->GetShaderDir();

    VkShaderModule vert = LoadLightingShader(dev, dir + "/deferred_lighting.vert.spv");
    VkShaderModule frag = LoadLightingShader(dev, dir + "/deferred_lighting.frag.spv");
    if (!vert || !frag) {
        LUMA_LOG_ERROR("Deferred", "Failed to load deferred lighting shaders from {}", dir);
        if (vert) vkDestroyShaderModule(dev, vert, nullptr);
        if (frag) vkDestroyShaderModule(dev, frag, nullptr);
        return false;
    }

    // Sampler for GBuffer reads
    VkSamplerCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter = sci.minFilter = VK_FILTER_NEAREST;
    sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.maxLod = 1.0f;
    if (vkCreateSampler(dev, &sci, nullptr, &m_gbufferSampler) != VK_SUCCESS) return false;

    // Descriptor set layout: 4 combined-image-samplers + 1 UBO
    VkDescriptorSetLayoutBinding bindings[5]{};
    for (int i = 0; i < 4; ++i) {
        bindings[i].binding = static_cast<uint32_t>(i);
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo dli{};
    dli.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dli.bindingCount = 5;
    dli.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(dev, &dli, nullptr, &m_descriptorLayout) != VK_SUCCESS) return false;

    // Descriptor pool
    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 4;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = 1;
    VkDescriptorPoolCreateInfo dpi{};
    dpi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpi.maxSets = 1;
    dpi.poolSizeCount = 2;
    dpi.pPoolSizes = poolSizes;
    if (vkCreateDescriptorPool(dev, &dpi, nullptr, &m_descriptorPool) != VK_SUCCESS) return false;

    VkDescriptorSetAllocateInfo dai{};
    dai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dai.descriptorPool = m_descriptorPool;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &m_descriptorLayout;
    if (vkAllocateDescriptorSets(dev, &dai, &m_descriptorSet) != VK_SUCCESS) return false;

    // Lighting UBO
    RHI::BufferDesc bd;
    bd.size = sizeof(LightingUBO);
    bd.usage = RHI::EBufferUsage::Uniform;
    bd.cpuAccess = RHI::EBufferCPUAccess::Write;
    m_lightingUBO = m_device->GetContext()->GetResourceFactory()->CreateBuffer(bd);
    if (!m_lightingUBO) return false;

    // Pipeline layout (no push constants)
    VkPipelineLayoutCreateInfo pli{};
    pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pli.setLayoutCount = 1;
    pli.pSetLayouts = &m_descriptorLayout;
    if (vkCreatePipelineLayout(dev, &pli, nullptr, &m_pipelineLayout) != VK_SUCCESS) return false;

    // Fullscreen triangle pipeline
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vin{};
    vin.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

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
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState ba{};
    ba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    ba.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &ba;

    VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynState{};
    dynState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynState.dynamicStateCount = 2;
    dynState.pDynamicStates = dyn;

    // Use the actual format of the accumulation buffer instead of hardcoded R8G8B8A8_UNORM
    VkFormat accumFmt = VK_FORMAT_R16G16B16A16_SFLOAT;  // Match GBuffer normal target format
    VkPipelineRenderingCreateInfo ri{};
    ri.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachmentFormats = &accumFmt;

    VkGraphicsPipelineCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pci.pNext = &ri;
    pci.stageCount = 2;
    pci.pStages = stages;
    pci.pVertexInputState = &vin;
    pci.pInputAssemblyState = &ia;
    pci.pViewportState = &vp;
    pci.pRasterizationState = &rs;
    pci.pMultisampleState = &ms;
    pci.pDepthStencilState = &ds;
    pci.pColorBlendState = &cb;
    pci.pDynamicState = &dynState;
    pci.layout = m_pipelineLayout;

    if (vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pci, nullptr, &m_pipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(dev, vert, nullptr);
        vkDestroyShaderModule(dev, frag, nullptr);
        return false;
    }

    vkDestroyShaderModule(dev, vert, nullptr);
    vkDestroyShaderModule(dev, frag, nullptr);
    return true;
}

void LightingRenderer::CleanupPipeline() {
    if (!m_device) return;
    auto* vkDev = dynamic_cast<RHI::VulkanRHIDevice*>(m_device);
    if (vkDev) {
        VkDevice dev = vkDev->LogicalHandle();
        if (m_pipeline)         vkDestroyPipeline(dev, m_pipeline, nullptr);
        if (m_pipelineLayout)   vkDestroyPipelineLayout(dev, m_pipelineLayout, nullptr);
        if (m_descriptorLayout) vkDestroyDescriptorSetLayout(dev, m_descriptorLayout, nullptr);
        if (m_descriptorPool)   vkDestroyDescriptorPool(dev, m_descriptorPool, nullptr);
        if (m_gbufferSampler)   vkDestroySampler(dev, m_gbufferSampler, nullptr);
    }
    if (m_lightingUBO) {
        m_device->GetContext()->GetResourceFactory()->DestroyBuffer(m_lightingUBO);
        m_lightingUBO = nullptr;
    }
    m_pipeline = VK_NULL_HANDLE;
    m_pipelineLayout = VK_NULL_HANDLE;
    m_descriptorLayout = VK_NULL_HANDLE;
    m_descriptorPool = VK_NULL_HANDLE;
    m_descriptorSet = VK_NULL_HANDLE;
    m_gbufferSampler = VK_NULL_HANDLE;
}

void LightingRenderer::UpdateDescriptorSet() {
    if (!m_gBuffer || !m_descriptorSet || !m_gbufferSampler) return;
    auto* vkDev = dynamic_cast<RHI::VulkanRHIDevice*>(m_device);
    if (!vkDev) return;
    VkDevice dev = vkDev->LogicalHandle();

    auto getView = [](RHI::RHIShaderResourceView* srv) -> VkImageView {
        if (!srv) return VK_NULL_HANDLE;
        auto* tex = static_cast<RHI::VulkanRHITexture*>(srv->GetResource());
        return tex ? tex->DefaultView() : VK_NULL_HANDLE;
    };

    VkImageView views[4] = {
        getView(m_gBuffer->GetColorSRV()),
        getView(m_gBuffer->GetNormalSRV()),
        getView(m_gBuffer->GetMaterialSRV()),
        getView(m_gBuffer->GetDepthSRV()),
    };

    VkWriteDescriptorSet writes[5]{};
    VkDescriptorImageInfo imgInfos[4]{};
    for (int i = 0; i < 4; ++i) {
        imgInfos[i].sampler     = m_gbufferSampler;
        imgInfos[i].imageView   = views[i];
        imgInfos[i].imageLayout = (i == 3)
            ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
            : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        writes[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet          = m_descriptorSet;
        writes[i].dstBinding      = static_cast<uint32_t>(i);
        writes[i].descriptorCount = 1;
        writes[i].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].pImageInfo      = &imgInfos[i];
    }

    auto* vkBuf = static_cast<RHI::VulkanRHIBuffer*>(m_lightingUBO);
    VkDescriptorBufferInfo bi{};
    bi.buffer = vkBuf->Handle();
    bi.offset = 0;
    bi.range  = sizeof(LightingUBO);
    writes[4].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet          = m_descriptorSet;
    writes[4].dstBinding      = 4;
    writes[4].descriptorCount = 1;
    writes[4].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[4].pBufferInfo     = &bi;

    vkUpdateDescriptorSets(dev, 5, writes, 0, nullptr);
}

void LightingRenderer::RenderLighting(RHI::RHICommandList* cmdList) {
    if (!m_gBuffer || !m_gBuffer->IsValid() || !m_initialized || !m_accumBuffer) return;

    auto* vkList = dynamic_cast<RHI::VulkanRHICommandList*>(cmdList);
    if (!vkList) return;
    VkCommandBuffer cmd = vkList->Handle();

    // Transition GBuffer SRVs to shader-read layouts handled by prior barriers
    // (GBuffer::Clear already transitioned them to RenderTarget; caller must
    // transition to ShaderResource before invoking RenderLighting)

    // Update UBO
    SetLightingUniforms(cmdList);

    // Point descriptor set at current GBuffer textures
    UpdateDescriptorSet();

    // Bind accumulation buffer as render target
    if (m_accumBuffer->IsValid()) {
        m_accumBuffer->Clear(cmdList);
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
                            0, 1, &m_descriptorSet, 0, nullptr);

    u32 w = m_gBuffer->GetWidth();
    u32 h = m_gBuffer->GetHeight();
    VkViewport viewport{0.0f, 0.0f, static_cast<f32>(w), static_cast<f32>(h), 0.0f, 1.0f};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{{0, 0}, {w, h}};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Draw fullscreen triangle (3 vertices, no VB needed – generated in vert shader)
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

void LightingRenderer::RenderDirectionalLight(RHI::RHICommandList* cmdList) {
    RenderLighting(cmdList); // handled inside unified deferred shader
}

void LightingRenderer::RenderPunctualLights(RHI::RHICommandList* cmdList) {
    (void)cmdList; // handled inside unified deferred shader
}

void LightingRenderer::RenderIBL(RHI::RHICommandList* cmdList) {
    (void)cmdList; // handled inside unified deferred shader
}

void LightingRenderer::Prepare(RHI::RHICommandList* cmdList) {
    if (!m_gBuffer || !m_gBuffer->IsValid()) return;
    // Transition GBuffer targets from RenderTarget → ShaderResource
    cmdList->ResourceBarrier(m_gBuffer->GetColorTarget(),    RHI::EResourceState::ShaderResource);
    cmdList->ResourceBarrier(m_gBuffer->GetNormalTarget(),   RHI::EResourceState::ShaderResource);
    cmdList->ResourceBarrier(m_gBuffer->GetMaterialTarget(), RHI::EResourceState::ShaderResource);
    cmdList->ResourceBarrier(m_gBuffer->GetDepthTarget(),    RHI::EResourceState::ShaderResource);
}

void LightingRenderer::SetLightingUniforms(RHI::RHICommandList* /*cmdList*/) {
    if (!m_lightingUBO) return;

    LightingUBO ubo{};
    // invViewProj – identity placeholder (caller should supply real matrices)
    ubo.invViewProj = Mat4::Identity();
    ubo.camPos      = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
    ubo.sunDir      = Vec4(m_lightingParams.sunDirection.x,
                           m_lightingParams.sunDirection.y,
                           m_lightingParams.sunDirection.z, 0.0f);
    ubo.sunColor    = Vec4(m_lightingParams.sunColor.x,
                           m_lightingParams.sunColor.y,
                           m_lightingParams.sunColor.z,
                           m_lightingParams.sunIntensity);
    ubo.ambientColor = Vec4(m_lightingParams.skyZenith.x,
                            m_lightingParams.skyZenith.y,
                            m_lightingParams.skyZenith.z,
                            m_lightingParams.iblIntensity);

    u32 lightCount = static_cast<u32>(std::min(m_lights.size(), static_cast<size_t>(MAX_LIGHTS)));
    ubo.params = Vec4(static_cast<f32>(lightCount), 0.0f, 0.0f, 0.0f);
    for (u32 i = 0; i < lightCount; ++i) {
        const LightData& ld = m_lights[i];
        ubo.lights[i].posType  = Vec4(ld.position.x, ld.position.y, ld.position.z,
                                      static_cast<f32>(ld.type));
        ubo.lights[i].dirRange = Vec4(ld.direction.x, ld.direction.y, ld.direction.z, ld.range);
        ubo.lights[i].color    = Vec4(ld.color.x, ld.color.y, ld.color.z, ld.intensity);
        ubo.lights[i].spot     = Vec4(ld.cosInner, ld.cosOuter, 0.0f, 0.0f);
    }

    m_lightingUBO->UpdateData(&ubo, sizeof(ubo), 0);
}

// ============================================================================
// Light Accumulation Buffer
// ============================================================================

LightAccumulationBuffer::LightAccumulationBuffer()
    : m_created(false) {
}

LightAccumulationBuffer::~LightAccumulationBuffer() {
    Destroy();
}

bool LightAccumulationBuffer::Create(u32 width, u32 height, RHI::ETextureFormat format, RHI::RHIDevice* device) {
    m_width = width;
    m_height = height;
    m_device = device;

    if (!device) {
        m_created = true;
        return true;
    }

    auto* factory = device->GetContext()->GetResourceFactory();
    if (!factory) return false;

    RHI::TextureDesc td;
    td.width = width;
    td.height = height;
    td.mipLevels = 1;
    td.arraySize = 1;
    td.format = format;
    td.usage = RHI::ETextureUsage::RenderTarget | RHI::ETextureUsage::ShaderResource;
    td.flags = RHI::ETextureFlags::RenderTargetable | RHI::ETextureFlags::ShaderResource;

    m_accumulationTexture = factory->CreateTexture(td);
    if (!m_accumulationTexture) return false;

    RHI::TextureSubresourceRange range{};
    range.baseMipLevel = 0;
    range.mipLevels = 1;
    range.baseArrayLayer = 0;
    range.arrayLayers = 1;

    m_accumulationRTV = factory->CreateRenderTargetView(m_accumulationTexture, format, range);
    m_accumulationSRV = factory->CreateShaderResourceView(m_accumulationTexture, format, range);

    if (!m_accumulationRTV || !m_accumulationSRV) {
        Destroy();
        return false;
    }

    m_created = true;
    return true;
}

bool LightAccumulationBuffer::Resize(u32 width, u32 height, RHI::RHIDevice* device) {
    Destroy();
    return Create(width, height, RHI::ETextureFormat::R16G16B16A16_FLOAT, device);
}

void LightAccumulationBuffer::Destroy() {
    if (m_device) {
        auto* factory = m_device->GetContext()->GetResourceFactory();
        if (factory) {
            if (m_accumulationRTV) factory->DestroyRenderTargetView(m_accumulationRTV);
            if (m_accumulationSRV) factory->DestroyShaderResourceView(m_accumulationSRV);
            if (m_accumulationTexture) factory->DestroyTexture(m_accumulationTexture);
        }
    }
    m_accumulationRTV = nullptr;
    m_accumulationSRV = nullptr;
    m_accumulationTexture = nullptr;
    m_device = nullptr;
    m_created = false;
}

void LightAccumulationBuffer::Clear(RHI::RHICommandList* cmdList) {
    if (!IsValid() || !cmdList) return;

    cmdList->ResourceBarrier(m_accumulationTexture, RHI::EResourceState::RenderTarget);
    RHI::RHIRenderTargetView* rtvs[1] = { m_accumulationRTV };
    cmdList->SetRenderTargets(1, rtvs, nullptr);
    cmdList->ClearRenderTargetView(m_accumulationRTV, Vec4(0.0f, 0.0f, 0.0f, 0.0f));
}

VkImageView LightAccumulationBuffer::GetVulkanImageView() const {
    if (!m_accumulationSRV) return VK_NULL_HANDLE;
    auto* vkSrv = static_cast<RHI::VulkanRHIShaderResourceView*>(m_accumulationSRV);
    if (!vkSrv) return VK_NULL_HANDLE;
    auto* vkTex = static_cast<RHI::VulkanRHITexture*>(vkSrv->GetResource());
    return vkTex ? vkTex->DefaultView() : VK_NULL_HANDLE;
}

} // namespace Renderer2
} // namespace Luma