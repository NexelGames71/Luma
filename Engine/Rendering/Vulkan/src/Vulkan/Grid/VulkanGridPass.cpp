#include "Vulkan/Grid/VulkanGridPass.h"

#include <cstring>

#include "Vulkan/VulkanShader.h"

namespace Luma {
namespace {

// std140 layout matching the GridUBO block in grid.frag.
struct GridUBO {
    f32 camPos[4];
    f32 minorColor[4];
    f32 majorColor[4];
    f32 axisX[4];
    f32 axisZ[4];
    f32 params[4];  // cellSize, majorEvery, fadeStart, fadeEnd
};

struct GridPush {
    Math::Mat4 invViewProj;
    Math::Mat4 viewProj;
};

}  // namespace

VulkanGridPass::VulkanGridPass(VkPhysicalDevice physical, VkDevice device,
                               const std::string& shaderDir,
                               VkSampleCountFlagBits samples,
                               VkFormat colorFormat, VkFormat depthFormat)
    : m_device(device) {
    // Params UBO (host-visible, persistently mapped) + its descriptor set.
    m_ubo = CreateBuffer(physical, device, sizeof(GridUBO),
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

    VkDescriptorBufferInfo bufInfo{m_ubo.buffer, 0, sizeof(GridUBO)};
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_set;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = &bufInfo;
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push.offset = 0;
    push.size = sizeof(GridPush);
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_setLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &push;
    VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_layout));

    VkShaderModule vert = LoadShaderModule(device, shaderDir + "/grid.vert.spv");
    VkShaderModule frag = LoadShaderModule(device, shaderDir + "/grid.frag.spv");
    LUMA_ASSERT(vert && frag, "failed to load grid shaders");

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
    ms.rasterizationSamples = samples;

    // Depth-tested and written (fragment writes true plane depth), so geometry
    // occludes the grid and vice-versa.
    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS;

    // Straight alpha blend over the sky.
    VkPipelineColorBlendAttachmentState ba{};
    ba.blendEnable = VK_TRUE;
    ba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    ba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    ba.colorBlendOp = VK_BLEND_OP_ADD;
    ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    ba.alphaBlendOp = VK_BLEND_OP_ADD;
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

VulkanGridPass::~VulkanGridPass() {
    if (m_pipeline) vkDestroyPipeline(m_device, m_pipeline, nullptr);
    if (m_layout) vkDestroyPipelineLayout(m_device, m_layout, nullptr);
    if (m_pool) vkDestroyDescriptorPool(m_device, m_pool, nullptr);
    if (m_setLayout)
        vkDestroyDescriptorSetLayout(m_device, m_setLayout, nullptr);
    DestroyBuffer(m_device, m_ubo);
}

void VulkanGridPass::Record(VkCommandBuffer cmd, const GridParams& grid,
                            const Math::Mat4& view, const Math::Mat4& viewProj) {
    using namespace Math;
    Mat4 invView = Inverse(view);

    GridUBO ubo{};
    ubo.camPos[0] = invView.m[12];
    ubo.camPos[1] = invView.m[13];
    ubo.camPos[2] = invView.m[14];
    auto setRGB = [](f32* dst, const Vec3& c) {
        dst[0] = c.x;
        dst[1] = c.y;
        dst[2] = c.z;
        dst[3] = 1.0f;
    };
    setRGB(ubo.minorColor, grid.minorColor);
    setRGB(ubo.majorColor, grid.majorColor);
    setRGB(ubo.axisX, grid.axisX);
    setRGB(ubo.axisZ, grid.axisZ);
    ubo.params[0] = grid.cellSize;
    ubo.params[1] = static_cast<f32>(grid.majorEvery);
    ubo.params[2] = grid.fadeStart;
    ubo.params[3] = grid.fadeEnd;
    std::memcpy(m_ubo.mapped, &ubo, sizeof(ubo));

    GridPush pc{};
    pc.invViewProj = Inverse(viewProj);
    pc.viewProj = viewProj;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 0, 1,
                            &m_set, 0, nullptr);
    vkCmdPushConstants(cmd, m_layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(GridPush), &pc);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

}  // namespace Luma
