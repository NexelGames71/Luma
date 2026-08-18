#include "Vulkan/Sky/VulkanSkyPass.h"

#include <cmath>
#include <cstring>

#include "Vulkan/Sky/AtmosphereParams.h"
#include "Vulkan/VulkanShader.h"

namespace Luma {
namespace {

// Matches the push_constant block in sky.vert/sky.frag (80 bytes).
struct SkyPush {
    Math::Mat4 invViewProj;
    f32 cameraPos[4];
};

}  // namespace

VulkanSkyPass::VulkanSkyPass(VkPhysicalDevice physical, VkDevice device,
                             const std::string& shaderDir,
                             VkSampleCountFlagBits samples, VkFormat colorFormat,
                             VkFormat depthFormat)
    : m_device(device) {
    // Atmosphere params UBO (set 0) — the push constant stays small so the
    // pass works on devices with the minimum 128-byte push-constant limit.
    m_ubo = CreateBuffer(physical, device, sizeof(Rendering::AtmosphereParams),
                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         true);

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo slInfo{};
    slInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    slInfo.bindingCount = 1;
    slInfo.pBindings = &binding;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &slInfo, nullptr, &m_setLayout));

    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1};
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_pool));

    VkDescriptorSetAllocateInfo setAlloc{};
    setAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAlloc.descriptorPool = m_pool;
    setAlloc.descriptorSetCount = 1;
    setAlloc.pSetLayouts = &m_setLayout;
    VK_CHECK(vkAllocateDescriptorSets(device, &setAlloc, &m_set));

    VkDescriptorBufferInfo bufInfo{m_ubo.buffer, 0,
                                   sizeof(Rendering::AtmosphereParams)};
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_set;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = &bufInfo;
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    // Push constants: camera matrices only (invViewProj + cameraPos, 80 bytes).
    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push.offset = 0;
    push.size = sizeof(SkyPush);
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_setLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &push;
    VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_layout));

    VkShaderModule vert = LoadShaderModule(device, shaderDir + "/sky.vert.spv");
    VkShaderModule frag = LoadShaderModule(device, shaderDir + "/sky.frag.spv");
    LUMA_ASSERT(vert && frag, "failed to load sky shaders");

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    // No vertex input: the fullscreen triangle is generated from gl_VertexIndex.
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
    ms.rasterizationSamples = samples;

    // Sky is infinitely far: never test or write depth so geometry paints over.
    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;
    ds.depthCompareOp = VK_COMPARE_OP_ALWAYS;

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

    VkPipelineRenderingCreateInfo ri{};
    ri.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachmentFormats = &colorFormat;
    ri.depthAttachmentFormat = depthFormat;

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
    info.layout = m_layout;
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr,
                                       &m_pipeline));
    vkDestroyShaderModule(device, vert, nullptr);
    vkDestroyShaderModule(device, frag, nullptr);
}

VulkanSkyPass::~VulkanSkyPass() {
    if (m_pipeline) vkDestroyPipeline(m_device, m_pipeline, nullptr);
    if (m_layout) vkDestroyPipelineLayout(m_device, m_layout, nullptr);
    if (m_pool) vkDestroyDescriptorPool(m_device, m_pool, nullptr);
    if (m_setLayout)
        vkDestroyDescriptorSetLayout(m_device, m_setLayout, nullptr);
    if (m_ubo.buffer) DestroyBuffer(m_device, m_ubo);
}

void VulkanSkyPass::Record(VkCommandBuffer cmd, const SkyParams& sky,
                           const Math::Mat4& view, const Math::Mat4& viewProj) {
    using namespace Math;
    Mat4 invView = Inverse(view);
    Mat4 invViewProj = Inverse(viewProj);

    Rendering::AtmosphereParams params;
    Rendering::FillAtmosphereParams(params, sky);
    std::memcpy(m_ubo.mapped, &params, sizeof(params));

    SkyPush pc{};
    pc.invViewProj = invViewProj;
    pc.cameraPos[0] = invView.m[12];
    pc.cameraPos[1] = invView.m[13];
    pc.cameraPos[2] = invView.m[14];
    pc.cameraPos[3] = 1.0f;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 0, 1,
                            &m_set, 0, nullptr);
    vkCmdPushConstants(cmd, m_layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(SkyPush), &pc);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

}  // namespace Luma
