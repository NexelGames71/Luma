#include "Vulkan/Scene/VulkanSceneView.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include "Luma/Math/Math.h"
#include "Luma/Mesh/Mesh.h"
#include "Vulkan/VulkanShader.h"

namespace Luma {
namespace {

// Per-instance material push for the lit mesh pipeline (96 bytes).
struct MeshPush {
    Math::Mat4 model;
    f32 albedo[4];    // rgb, w = metallic
    f32 material[4];  // x = roughness
};

// Per-draw push for the line pipeline (80 bytes).
struct LinePush {
    Math::Mat4 mvp;
    f32 tint[4];
};

constexpr u32 kMaxLights = 16;

// Per-frame camera + lighting UBO (matches SceneUBO in scene.vert/frag).
struct GpuLight {
    f32 posType[4];   // xyz position, w type
    f32 dirRange[4];  // xyz direction, w range
    f32 color[4];     // rgb color, w intensity
    f32 spot[4];      // x cosInner, y cosOuter
};
constexpr u32 kCascadesUBO = 4;
struct SceneUBO {
    Math::Mat4 viewProj;
    f32 camPos[4];
    f32 camForward[4];
    f32 sunDir[4];
    f32 sunColor[4];  // rgb, w = intensity
    f32 skyZenith[4];
    f32 skyHorizon[4];
    f32 groundColor[4];
    f32 params[4];        // x=iblIntensity, y=lightCount, z=sunShadows, w=1/size
    f32 shadowParams[4];  // x=softness, y=cascadeCount
    f32 cascadeSplits[4];
    Math::Mat4 cascadeViewProj[kCascadesUBO];
    GpuLight lights[kMaxLights];
};

void SetVec3(f32* dst, const Math::Vec3& v, f32 w = 1.0f) {
    dst[0] = v.x;
    dst[1] = v.y;
    dst[2] = v.z;
    dst[3] = w;
}

// Transforms a world point by a column-major matrix (w = 1, no perspective).
Math::Vec3 TransformPoint(const Math::Mat4& m, const Math::Vec3& p) {
    return {m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12],
            m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13],
            m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14]};
}

}  // namespace

VulkanSceneView::VulkanSceneView(VkPhysicalDevice physical, VkDevice device,
                                 VkQueue queue, u32 graphicsFamily,
                                 VulkanUIPass& uiPass,
                                 const std::string& shaderDir)
    : m_physical(physical), m_device(device), m_queue(queue), m_uiPass(uiPass) {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsFamily;
    VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &m_pool));

    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = m_pool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(device, &alloc, &m_cmd));

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &m_fence));

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physical, &props);
    VkSampleCountFlags supported = props.limits.framebufferColorSampleCounts &
                                   props.limits.framebufferDepthSampleCounts;
    if (supported & VK_SAMPLE_COUNT_4_BIT) m_samples = VK_SAMPLE_COUNT_4_BIT;
    else if (supported & VK_SAMPLE_COUNT_2_BIT) m_samples = VK_SAMPLE_COUNT_2_BIT;
    else m_samples = VK_SAMPLE_COUNT_1_BIT;

    CreatePrimitives();
    CreateShadowResources();
    CreateSceneUBO();
    CreateLayouts();
    m_meshPipeline = CreateMeshPipeline(shaderDir);
    m_linePipeline = CreateLinePipeline(shaderDir, /*depthTest=*/true);
    m_overlayPipeline = CreateLinePipeline(shaderDir, /*depthTest=*/false);
    m_shadowPipeline = CreateShadowPipeline(shaderDir);

    m_skyPass = std::make_unique<VulkanSkyPass>(device, shaderDir, m_samples,
                                                kColorFormat, kDepthFormat);
    m_gridPass = std::make_unique<VulkanGridPass>(
        physical, device, shaderDir, m_samples, kColorFormat, kDepthFormat);
}

VulkanSceneView::~VulkanSceneView() {
    vkDeviceWaitIdle(m_device);
    DestroyTargets();
    for (Primitive& p : m_primitives) {
        DestroyBuffer(m_device, p.vertexBuffer);
        DestroyBuffer(m_device, p.indexBuffer);
    }
    DestroyBuffer(m_device, m_lineBuffer);
    DestroyBuffer(m_device, m_overlayBuffer);
    DestroyBuffer(m_device, m_ubo);
    if (m_meshPipeline) vkDestroyPipeline(m_device, m_meshPipeline, nullptr);
    if (m_linePipeline) vkDestroyPipeline(m_device, m_linePipeline, nullptr);
    if (m_overlayPipeline)
        vkDestroyPipeline(m_device, m_overlayPipeline, nullptr);
    if (m_shadowPipeline)
        vkDestroyPipeline(m_device, m_shadowPipeline, nullptr);
    if (m_shadowSampler) vkDestroySampler(m_device, m_shadowSampler, nullptr);
    if (m_shadowArrayView)
        vkDestroyImageView(m_device, m_shadowArrayView, nullptr);
    for (VkImageView v : m_shadowLayerViews) {
        if (v) vkDestroyImageView(m_device, v, nullptr);
    }
    if (m_shadowImage) vkDestroyImage(m_device, m_shadowImage, nullptr);
    if (m_shadowMem) vkFreeMemory(m_device, m_shadowMem, nullptr);
    if (m_meshLayout) vkDestroyPipelineLayout(m_device, m_meshLayout, nullptr);
    if (m_lineLayout) vkDestroyPipelineLayout(m_device, m_lineLayout, nullptr);
    if (m_shadowLayout)
        vkDestroyPipelineLayout(m_device, m_shadowLayout, nullptr);
    if (m_uboPool) vkDestroyDescriptorPool(m_device, m_uboPool, nullptr);
    if (m_uboSetLayout)
        vkDestroyDescriptorSetLayout(m_device, m_uboSetLayout, nullptr);
    if (m_fence) vkDestroyFence(m_device, m_fence, nullptr);
    if (m_pool) vkDestroyCommandPool(m_device, m_pool, nullptr);
}

void VulkanSceneView::CreatePrimitives() {
    const MeshPrimitive kinds[kPrimitiveCount] = {
        MeshPrimitive::Cube, MeshPrimitive::Plane, MeshPrimitive::Sphere,
        MeshPrimitive::Cylinder};
    for (u32 i = 0; i < kPrimitiveCount; ++i) {
        MeshData data = BuildPrimitive(kinds[i]);
        VkDeviceSize vsize = sizeof(MeshVertex) * data.vertices.size();
        VkDeviceSize isize = sizeof(u32) * data.indices.size();
        m_primitives[i].vertexBuffer =
            CreateBuffer(m_physical, m_device, vsize,
                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         true);
        std::memcpy(m_primitives[i].vertexBuffer.mapped, data.vertices.data(),
                    static_cast<usize>(vsize));
        m_primitives[i].indexBuffer =
            CreateBuffer(m_physical, m_device, isize,
                         VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         true);
        std::memcpy(m_primitives[i].indexBuffer.mapped, data.indices.data(),
                    static_cast<usize>(isize));
        m_primitives[i].indexCount = static_cast<u32>(data.indices.size());
    }
}

void VulkanSceneView::CreateShadowResources() {
    VkImageCreateInfo ii{};
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = kShadowFormat;
    ii.extent = {kShadowSize, kShadowSize, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = kCascades;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
               VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CHECK(vkCreateImage(m_device, &ii, nullptr, &m_shadowImage));

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(m_device, m_shadowImage, &req);
    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = FindMemoryType(m_physical, req.memoryTypeBits,
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vkAllocateMemory(m_device, &ai, nullptr, &m_shadowMem));
    VK_CHECK(vkBindImageMemory(m_device, m_shadowImage, m_shadowMem, 0));

    // Array view for sampling (sampler2DArray) + per-layer views to render into.
    VkImageViewCreateInfo av{};
    av.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    av.image = m_shadowImage;
    av.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    av.format = kShadowFormat;
    av.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, kCascades};
    VK_CHECK(vkCreateImageView(m_device, &av, nullptr, &m_shadowArrayView));
    for (u32 i = 0; i < kCascades; ++i) {
        VkImageViewCreateInfo lv{};
        lv.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        lv.image = m_shadowImage;
        lv.viewType = VK_IMAGE_VIEW_TYPE_2D;
        lv.format = kShadowFormat;
        lv.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, i, 1};
        VK_CHECK(vkCreateImageView(m_device, &lv, nullptr, &m_shadowLayerViews[i]));
    }

    VkSamplerCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter = VK_FILTER_LINEAR;
    si.minFilter = VK_FILTER_LINEAR;
    si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    si.maxLod = 1.0f;
    VK_CHECK(vkCreateSampler(m_device, &si, nullptr, &m_shadowSampler));
}

void VulkanSceneView::CreateSceneUBO() {
    m_ubo = CreateBuffer(m_physical, m_device, sizeof(SceneUBO),
                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         true);

    VkDescriptorSetLayoutBinding bindings[2]{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo slInfo{};
    slInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    slInfo.bindingCount = 2;
    slInfo.pBindings = bindings;
    VK_CHECK(
        vkCreateDescriptorSetLayout(m_device, &slInfo, nullptr, &m_uboSetLayout));

    VkDescriptorPoolSize poolSizes[2] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    VK_CHECK(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_uboPool));

    VkDescriptorSetAllocateInfo setAlloc{};
    setAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAlloc.descriptorPool = m_uboPool;
    setAlloc.descriptorSetCount = 1;
    setAlloc.pSetLayouts = &m_uboSetLayout;
    VK_CHECK(vkAllocateDescriptorSets(m_device, &setAlloc, &m_uboSet));

    VkDescriptorBufferInfo bufInfo{m_ubo.buffer, 0, sizeof(SceneUBO)};
    VkDescriptorImageInfo imgInfo{m_shadowSampler, m_shadowArrayView,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet writes[2]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_uboSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &bufInfo;
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_uboSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = &imgInfo;
    vkUpdateDescriptorSets(m_device, 2, writes, 0, nullptr);
}

void VulkanSceneView::CreateLayouts() {
    // Mesh layout: UBO descriptor set 0 + per-instance material push.
    VkPushConstantRange meshPush{};
    meshPush.stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    meshPush.offset = 0;
    meshPush.size = sizeof(MeshPush);
    VkPipelineLayoutCreateInfo meshInfo{};
    meshInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    meshInfo.setLayoutCount = 1;
    meshInfo.pSetLayouts = &m_uboSetLayout;
    meshInfo.pushConstantRangeCount = 1;
    meshInfo.pPushConstantRanges = &meshPush;
    VK_CHECK(
        vkCreatePipelineLayout(m_device, &meshInfo, nullptr, &m_meshLayout));

    // Line layout: just an mvp+tint push.
    VkPushConstantRange linePush{};
    linePush.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    linePush.offset = 0;
    linePush.size = sizeof(LinePush);
    VkPipelineLayoutCreateInfo lineInfo{};
    lineInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    lineInfo.pushConstantRangeCount = 1;
    lineInfo.pPushConstantRanges = &linePush;
    VK_CHECK(
        vkCreatePipelineLayout(m_device, &lineInfo, nullptr, &m_lineLayout));

    // Shadow layout: a single lightViewProj*model matrix push.
    VkPushConstantRange shadowPush{};
    shadowPush.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    shadowPush.offset = 0;
    shadowPush.size = sizeof(Math::Mat4);
    VkPipelineLayoutCreateInfo shadowInfo{};
    shadowInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    shadowInfo.pushConstantRangeCount = 1;
    shadowInfo.pPushConstantRanges = &shadowPush;
    VK_CHECK(
        vkCreatePipelineLayout(m_device, &shadowInfo, nullptr, &m_shadowLayout));
}

VkPipeline VulkanSceneView::CreateShadowPipeline(const std::string& shaderDir) {
    VkShaderModule vert = LoadShaderModule(m_device, shaderDir + "/shadow.vert.spv");
    VkShaderModule frag = LoadShaderModule(m_device, shaderDir + "/shadow.frag.spv");
    LUMA_ASSERT(vert && frag, "failed to load shadow shaders");

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(MeshVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attr{0, 0, VK_FORMAT_R32G32B32_SFLOAT,
                                           offsetof(MeshVertex, position)};
    VkPipelineVertexInputStateCreateInfo vin{};
    vin.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vin.vertexBindingDescriptionCount = 1;
    vin.pVertexBindingDescriptions = &binding;
    vin.vertexAttributeDescriptionCount = 1;
    vin.pVertexAttributeDescriptions = &attr;

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
    rs.depthBiasEnable = VK_TRUE;
    rs.depthBiasConstantFactor = 1.5f;
    rs.depthBiasSlopeFactor = 2.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 0;

    VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynState{};
    dynState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynState.dynamicStateCount = 2;
    dynState.pDynamicStates = dyn;

    VkPipelineRenderingCreateInfo ri{};
    ri.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    ri.colorAttachmentCount = 0;
    ri.depthAttachmentFormat = kShadowFormat;

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
    info.layout = m_shadowLayout;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &info,
                                       nullptr, &pipeline));
    vkDestroyShaderModule(m_device, vert, nullptr);
    vkDestroyShaderModule(m_device, frag, nullptr);
    return pipeline;
}

VkPipeline VulkanSceneView::CreateMeshPipeline(const std::string& shaderDir) {
    VkShaderModule vert = LoadShaderModule(m_device, shaderDir + "/scene.vert.spv");
    VkShaderModule frag = LoadShaderModule(m_device, shaderDir + "/scene.frag.spv");
    LUMA_ASSERT(vert && frag, "failed to load scene shaders");

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(MeshVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attrs[2]{};
    attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, position)};
    attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, normal)};
    VkPipelineVertexInputStateCreateInfo vin{};
    vin.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vin.vertexBindingDescriptionCount = 1;
    vin.pVertexBindingDescriptions = &binding;
    vin.vertexAttributeDescriptionCount = 2;
    vin.pVertexAttributeDescriptions = attrs;

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
    ms.rasterizationSamples = m_samples;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState ba{};
    ba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &ba;

    VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynState{};
    dynState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynState.dynamicStateCount = 2;
    dynState.pDynamicStates = dyn;

    VkFormat colorFmt = kColorFormat;
    VkPipelineRenderingCreateInfo ri{};
    ri.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachmentFormats = &colorFmt;
    ri.depthAttachmentFormat = kDepthFormat;

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
    info.layout = m_meshLayout;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &info,
                                       nullptr, &pipeline));
    vkDestroyShaderModule(m_device, vert, nullptr);
    vkDestroyShaderModule(m_device, frag, nullptr);
    return pipeline;
}

VkPipeline VulkanSceneView::CreateLinePipeline(const std::string& shaderDir,
                                               bool depthTest) {
    VkShaderModule vert = LoadShaderModule(m_device, shaderDir + "/line.vert.spv");
    VkShaderModule frag = LoadShaderModule(m_device, shaderDir + "/line.frag.spv");
    LUMA_ASSERT(vert && frag, "failed to load line shaders");

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(LineVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attrs[2]{};
    attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LineVertex, position)};
    attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LineVertex, color)};
    VkPipelineVertexInputStateCreateInfo vin{};
    vin.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vin.vertexBindingDescriptionCount = 1;
    vin.pVertexBindingDescriptions = &binding;
    vin.vertexAttributeDescriptionCount = 2;
    vin.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

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
    ms.rasterizationSamples = m_samples;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = depthTest ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState ba{};
    ba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &ba;

    VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynState{};
    dynState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynState.dynamicStateCount = 2;
    dynState.pDynamicStates = dyn;

    VkFormat colorFmt = kColorFormat;
    VkPipelineRenderingCreateInfo ri{};
    ri.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachmentFormats = &colorFmt;
    ri.depthAttachmentFormat = kDepthFormat;

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
    info.layout = m_lineLayout;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &info,
                                       nullptr, &pipeline));
    vkDestroyShaderModule(m_device, vert, nullptr);
    vkDestroyShaderModule(m_device, frag, nullptr);
    return pipeline;
}

void VulkanSceneView::UploadLines(GpuBuffer& buffer, const LineVertex* lines,
                                  u32 count) {
    VkDeviceSize needed = sizeof(LineVertex) * count;
    if (buffer.buffer == VK_NULL_HANDLE || buffer.size < needed) {
        if (buffer.buffer != VK_NULL_HANDLE) DestroyBuffer(m_device, buffer);
        VkDeviceSize size = needed + needed / 2 + 256;
        buffer = CreateBuffer(m_physical, m_device, size,
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              true);
    }
    std::memcpy(buffer.mapped, lines, static_cast<usize>(needed));
}

void VulkanSceneView::CreateTargets(u32 width, u32 height) {
    auto makeImage = [&](VkFormat format, VkImageUsageFlags usage,
                         VkSampleCountFlagBits samples, VkImage& image,
                         VkDeviceMemory& mem, VkImageView& view,
                         VkImageAspectFlags aspect) {
        VkImageCreateInfo ii{};
        ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ii.imageType = VK_IMAGE_TYPE_2D;
        ii.format = format;
        ii.extent = {width, height, 1};
        ii.mipLevels = 1;
        ii.arrayLayers = 1;
        ii.samples = samples;
        ii.tiling = VK_IMAGE_TILING_OPTIMAL;
        ii.usage = usage;
        ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VK_CHECK(vkCreateImage(m_device, &ii, nullptr, &image));

        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(m_device, image, &req);
        VkMemoryAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = FindMemoryType(m_physical, req.memoryTypeBits,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(m_device, &ai, nullptr, &mem));
        VK_CHECK(vkBindImageMemory(m_device, image, mem, 0));

        VkImageViewCreateInfo vi{};
        vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image = image;
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = format;
        vi.subresourceRange = {aspect, 0, 1, 0, 1};
        VK_CHECK(vkCreateImageView(m_device, &vi, nullptr, &view));
    };

    makeImage(kColorFormat,
              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_SAMPLE_COUNT_1_BIT, m_color, m_colorMem, m_colorView,
              VK_IMAGE_ASPECT_COLOR_BIT);
    makeImage(kColorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, m_samples,
              m_msaaColor, m_msaaColorMem, m_msaaColorView,
              VK_IMAGE_ASPECT_COLOR_BIT);
    makeImage(kDepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
              m_samples, m_depth, m_depthMem, m_depthView,
              VK_IMAGE_ASPECT_DEPTH_BIT);
    m_width = width;
    m_height = height;
}

void VulkanSceneView::DestroyTargets() {
    if (m_colorView) vkDestroyImageView(m_device, m_colorView, nullptr);
    if (m_color) vkDestroyImage(m_device, m_color, nullptr);
    if (m_colorMem) vkFreeMemory(m_device, m_colorMem, nullptr);
    if (m_msaaColorView) vkDestroyImageView(m_device, m_msaaColorView, nullptr);
    if (m_msaaColor) vkDestroyImage(m_device, m_msaaColor, nullptr);
    if (m_msaaColorMem) vkFreeMemory(m_device, m_msaaColorMem, nullptr);
    if (m_depthView) vkDestroyImageView(m_device, m_depthView, nullptr);
    if (m_depth) vkDestroyImage(m_device, m_depth, nullptr);
    if (m_depthMem) vkFreeMemory(m_device, m_depthMem, nullptr);
    m_colorView = VK_NULL_HANDLE;
    m_color = VK_NULL_HANDLE;
    m_colorMem = VK_NULL_HANDLE;
    m_msaaColorView = VK_NULL_HANDLE;
    m_msaaColor = VK_NULL_HANDLE;
    m_msaaColorMem = VK_NULL_HANDLE;
    m_depthView = VK_NULL_HANDLE;
    m_depth = VK_NULL_HANDLE;
    m_depthMem = VK_NULL_HANDLE;
}

TextureHandle VulkanSceneView::Render(u32 width, u32 height,
                                      const SceneView& scene) {
    if (width == 0 || height == 0) return m_textureHandle;

    if (width != m_width || height != m_height) {
        vkDeviceWaitIdle(m_device);
        DestroyTargets();
        CreateTargets(width, height);
        if (m_textureHandle == 0) {
            m_textureHandle = m_uiPass.RegisterExternalTexture(m_colorView);
        } else {
            m_uiPass.UpdateExternalTexture(m_textureHandle, m_colorView);
        }
    }

    vkWaitForFences(m_device, 1, &m_fence, VK_TRUE,
                    std::numeric_limits<u64>::max());
    vkResetFences(m_device, 1, &m_fence);
    vkResetCommandBuffer(m_cmd, 0);

    using namespace Math;
    f32 aspect = static_cast<f32>(width) / static_cast<f32>(height);
    Mat4 viewProj =
        Perspective(scene.fovYRadians, aspect, scene.nearZ, scene.farZ) *
        scene.view;
    Mat4 invView = Inverse(scene.view);
    Vec3 camPos{invView.m[12], invView.m[13], invView.m[14]};
    Vec3 camFwd = Normalize(Vec3(-invView.m[8], -invView.m[9], -invView.m[10]));
    Vec3 camRight = Normalize(Vec3(invView.m[0], invView.m[1], invView.m[2]));
    Vec3 camUp = Normalize(Vec3(invView.m[4], invView.m[5], invView.m[6]));

    // --- Cascaded shadow map fit: split the view frustum and fit an ortho box
    //     to each slice in light space. ---
    Vec3 sunDirN = Normalize(scene.lighting.sunDirection);
    Vec3 shadowUp = (sunDirN.y > 0.98f || sunDirN.y < -0.98f) ? Vec3(0, 0, 1)
                                                              : Vec3(0, 1, 0);
    f32 nearZ = scene.nearZ;
    f32 farZ = scene.farZ < scene.lighting.shadowDistance
                   ? scene.farZ
                   : scene.lighting.shadowDistance;
    f32 tanY = std::tan(scene.fovYRadians * 0.5f);
    f32 tanX = tanY * aspect;

    Mat4 cascadeVP[kCascades];
    f32 splitFar[kCascades];
    f32 lastFar = nearZ;
    for (u32 c = 0; c < kCascades; ++c) {
        f32 p = static_cast<f32>(c + 1) / static_cast<f32>(kCascades);
        f32 logSplit = nearZ * std::pow(farZ / nearZ, p);
        f32 uniSplit = nearZ + (farZ - nearZ) * p;
        f32 cf = 0.6f * logSplit + 0.4f * uniSplit;
        f32 cn = lastFar;
        lastFar = cf;
        splitFar[c] = cf;

        Vec3 corners[8];
        int k = 0;
        for (f32 zd : {cn, cf}) {
            Vec3 center = camPos + camFwd * zd;
            f32 hh = tanY * zd, hw = tanX * zd;
            corners[k++] = center + camUp * hh + camRight * hw;
            corners[k++] = center + camUp * hh - camRight * hw;
            corners[k++] = center - camUp * hh + camRight * hw;
            corners[k++] = center - camUp * hh - camRight * hw;
        }
        Vec3 cc{0, 0, 0};
        for (const Vec3& v : corners) cc = cc + v;
        cc = cc * (1.0f / 8.0f);
        f32 radius = 0.0f;
        for (const Vec3& v : corners) {
            Vec3 d = v - cc;
            f32 len = std::sqrt(Dot(d, d));
            if (len > radius) radius = len;
        }
        Mat4 lightView = LookAt(cc + sunDirN * (radius + 50.0f), cc, shadowUp);
        Vec3 mn{1e9f, 1e9f, 1e9f}, mx{-1e9f, -1e9f, -1e9f};
        for (const Vec3& v : corners) {
            Vec3 lp = TransformPoint(lightView, v);
            mn = Vec3(std::min(mn.x, lp.x), std::min(mn.y, lp.y),
                      std::min(mn.z, lp.z));
            mx = Vec3(std::max(mx.x, lp.x), std::max(mx.y, lp.y),
                      std::max(mx.z, lp.z));
        }
        // Light looks down -z; distances are -z. Pull the near plane toward the
        // light so tall casters outside the slice still shadow it.
        f32 nearD = -mx.z - 40.0f;
        if (nearD < 0.05f) nearD = 0.05f;
        f32 farD = -mn.z;
        cascadeVP[c] =
            Ortho(mn.x, mx.x, mn.y, mx.y, nearD, farD) * lightView;
    }

    // Fill the per-frame camera + lighting UBO.
    SceneUBO ubo{};
    for (u32 c = 0; c < kCascades; ++c) {
        ubo.cascadeViewProj[c] = cascadeVP[c];
        ubo.cascadeSplits[c] = splitFar[c];
    }
    ubo.camForward[0] = camFwd.x;
    ubo.camForward[1] = camFwd.y;
    ubo.camForward[2] = camFwd.z;
    ubo.shadowParams[0] = scene.lighting.shadowSoftness;
    ubo.shadowParams[1] = static_cast<f32>(kCascades);
    ubo.viewProj = viewProj;
    ubo.camPos[0] = invView.m[12];
    ubo.camPos[1] = invView.m[13];
    ubo.camPos[2] = invView.m[14];
    Vec3 sun = Normalize(scene.lighting.sunDirection);
    SetVec3(ubo.sunDir, sun);
    SetVec3(ubo.sunColor, scene.lighting.sunColor, scene.lighting.sunIntensity);
    SetVec3(ubo.skyZenith, scene.lighting.skyZenith);
    SetVec3(ubo.skyHorizon, scene.lighting.skyHorizon);
    SetVec3(ubo.groundColor, scene.lighting.groundColor);
    ubo.params[0] = scene.lighting.iblIntensity;
    u32 lightCount = scene.lightCount < kMaxLights ? scene.lightCount : kMaxLights;
    ubo.params[1] = static_cast<f32>(lightCount);
    ubo.params[2] = scene.lighting.sunShadows ? 1.0f : 0.0f;
    ubo.params[3] = 1.0f / static_cast<f32>(kShadowSize);
    for (u32 i = 0; i < lightCount; ++i) {
        const SceneLight& l = scene.lights[i];
        GpuLight& g = ubo.lights[i];
        g.posType[0] = l.position.x;
        g.posType[1] = l.position.y;
        g.posType[2] = l.position.z;
        g.posType[3] = static_cast<f32>(l.type);
        Vec3 dir = Normalize(l.direction);
        g.dirRange[0] = dir.x;
        g.dirRange[1] = dir.y;
        g.dirRange[2] = dir.z;
        g.dirRange[3] = l.range;
        g.color[0] = l.color.x;
        g.color[1] = l.color.y;
        g.color[2] = l.color.z;
        g.color[3] = l.intensity;
        g.spot[0] = l.cosInner;
        g.spot[1] = l.cosOuter;
    }
    std::memcpy(m_ubo.mapped, &ubo, sizeof(ubo));

    // Upload dynamic overlay-line data.
    if (scene.lineVertexCount) {
        UploadLines(m_lineBuffer, scene.lines, scene.lineVertexCount);
    }
    if (scene.overlayLineVertexCount) {
        UploadLines(m_overlayBuffer, scene.overlayLines,
                    scene.overlayLineVertexCount);
    }

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(m_cmd, &begin));

    auto barrier = [&](VkImage image, VkImageAspectFlags aspect,
                       VkImageLayout oldL, VkImageLayout newL,
                       VkAccessFlags src, VkAccessFlags dst,
                       VkPipelineStageFlags srcS, VkPipelineStageFlags dstS) {
        VkImageMemoryBarrier b{};
        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.oldLayout = oldL;
        b.newLayout = newL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = image;
        b.subresourceRange = {aspect, 0, 1, 0, 1};
        b.srcAccessMask = src;
        b.dstAccessMask = dst;
        vkCmdPipelineBarrier(m_cmd, srcS, dstS, 0, 0, nullptr, 0, nullptr, 1, &b);
    };

    // --- Cascaded sun shadow depth passes (one per cascade layer) ---
    auto shadowLayerBarrier = [&](VkImageLayout oldL, VkImageLayout newL,
                                  VkAccessFlags src, VkAccessFlags dst,
                                  VkPipelineStageFlags srcS,
                                  VkPipelineStageFlags dstS) {
        VkImageMemoryBarrier b{};
        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.oldLayout = oldL;
        b.newLayout = newL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = m_shadowImage;
        b.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, kCascades};
        b.srcAccessMask = src;
        b.dstAccessMask = dst;
        vkCmdPipelineBarrier(m_cmd, srcS, dstS, 0, 0, nullptr, 0, nullptr, 1, &b);
    };
    shadowLayerBarrier(VK_IMAGE_LAYOUT_UNDEFINED,
                       VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, 0,
                       VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
    for (u32 c = 0; c < kCascades; ++c) {
        VkRenderingAttachmentInfo sd{};
        sd.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        sd.imageView = m_shadowLayerViews[c];
        sd.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        sd.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        sd.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        sd.clearValue.depthStencil = {1.0f, 0};
        VkRenderingInfo sr{};
        sr.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        sr.renderArea.extent = {kShadowSize, kShadowSize};
        sr.layerCount = 1;
        sr.pDepthAttachment = &sd;
        vkCmdBeginRendering(m_cmd, &sr);
        VkViewport sv{0, 0, static_cast<f32>(kShadowSize),
                      static_cast<f32>(kShadowSize), 0.0f, 1.0f};
        vkCmdSetViewport(m_cmd, 0, 1, &sv);
        VkRect2D ssc{{0, 0}, {kShadowSize, kShadowSize}};
        vkCmdSetScissor(m_cmd, 0, 1, &ssc);
        if (scene.lighting.sunShadows) {
            vkCmdBindPipeline(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              m_shadowPipeline);
            VkDeviceSize soff = 0;
            for (u32 i = 0; i < scene.instanceCount; ++i) {
                const SceneInstance& inst = scene.instances[i];
                u32 prim = static_cast<u32>(inst.primitive);
                if (prim >= kPrimitiveCount) prim = 0;
                const Primitive& mesh = m_primitives[prim];
                Math::Mat4 mvp = cascadeVP[c] * inst.model;
                vkCmdPushConstants(m_cmd, m_shadowLayout,
                                   VK_SHADER_STAGE_VERTEX_BIT, 0,
                                   sizeof(Math::Mat4), &mvp);
                vkCmdBindVertexBuffers(m_cmd, 0, 1, &mesh.vertexBuffer.buffer,
                                       &soff);
                vkCmdBindIndexBuffer(m_cmd, mesh.indexBuffer.buffer, 0,
                                     VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(m_cmd, mesh.indexCount, 1, 0, 0, 0);
            }
        }
        vkCmdEndRendering(m_cmd);
    }
    shadowLayerBarrier(VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT,
                       VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    barrier(m_msaaColor, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    barrier(m_color, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    barrier(m_depth, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, 0,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);

    VkRenderingAttachmentInfo color{};
    color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color.imageView = m_msaaColorView;
    color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
    color.resolveImageView = m_colorView;
    color.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.clearValue.color = {{0.07f, 0.08f, 0.10f, 1.0f}};

    VkRenderingAttachmentInfo depth{};
    depth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depth.imageView = m_depthView;
    depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.clearValue.depthStencil = {1.0f, 0};

    VkRenderingInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea.extent = {width, height};
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments = &color;
    rendering.pDepthAttachment = &depth;
    vkCmdBeginRendering(m_cmd, &rendering);

    VkViewport viewport{0, 0, static_cast<f32>(width), static_cast<f32>(height),
                        0.0f, 1.0f};
    vkCmdSetViewport(m_cmd, 0, 1, &viewport);
    VkRect2D scissor{{0, 0}, {width, height}};
    vkCmdSetScissor(m_cmd, 0, 1, &scissor);

    VkDeviceSize offset = 0;

    // Sky background, then infinite grid.
    if (scene.sky.enabled) {
        m_skyPass->Record(m_cmd, scene.sky, scene.view, viewProj);
    }
    if (scene.grid.enabled) {
        m_gridPass->Record(m_cmd, scene.grid, scene.view, viewProj);
    }

    // Lit PBR mesh instances.
    vkCmdBindPipeline(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_meshPipeline);
    vkCmdBindDescriptorSets(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_meshLayout,
                            0, 1, &m_uboSet, 0, nullptr);
    for (u32 i = 0; i < scene.instanceCount; ++i) {
        const SceneInstance& inst = scene.instances[i];
        u32 prim = static_cast<u32>(inst.primitive);
        if (prim >= kPrimitiveCount) prim = 0;
        const Primitive& mesh = m_primitives[prim];

        MeshPush push{};
        push.model = inst.model;
        push.albedo[0] = inst.albedo.x;
        push.albedo[1] = inst.albedo.y;
        push.albedo[2] = inst.albedo.z;
        push.albedo[3] = inst.metallic;
        push.material[0] = inst.roughness;
        vkCmdPushConstants(m_cmd, m_meshLayout,
                           VK_SHADER_STAGE_VERTEX_BIT |
                               VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(MeshPush), &push);
        vkCmdBindVertexBuffers(m_cmd, 0, 1, &mesh.vertexBuffer.buffer, &offset);
        vkCmdBindIndexBuffer(m_cmd, mesh.indexBuffer.buffer, 0,
                             VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(m_cmd, mesh.indexCount, 1, 0, 0, 0);
    }

    // Overlay lines (gizmo), drawn on top without depth.
    if (scene.overlayLineVertexCount) {
        LinePush lp{};
        lp.mvp = viewProj;
        lp.tint[0] = lp.tint[1] = lp.tint[2] = lp.tint[3] = 1.0f;
        vkCmdBindPipeline(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_overlayPipeline);
        vkCmdPushConstants(m_cmd, m_lineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(LinePush), &lp);
        vkCmdBindVertexBuffers(m_cmd, 0, 1, &m_overlayBuffer.buffer, &offset);
        vkCmdDraw(m_cmd, scene.overlayLineVertexCount, 1, 0, 0);
    }

    vkCmdEndRendering(m_cmd);

    barrier(m_color, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    VK_CHECK(vkEndCommandBuffer(m_cmd));

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &m_cmd;
    VK_CHECK(vkQueueSubmit(m_queue, 1, &submit, m_fence));

    return m_textureHandle;
}

}  // namespace Luma
