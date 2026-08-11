#include "Vulkan/Sky/VulkanSkyPass.h"

#include <cmath>

#include "Vulkan/VulkanShader.h"

namespace Luma {
namespace {

// Matches the push_constant block in sky.vert/sky.frag (128 bytes, within the
// guaranteed 128-byte push-constant minimum).
struct SkyPush {
    Math::Mat4 invViewProj;
    f32 cameraPos[4];
    f32 sunDir[4];  // xyz dir to sun, w = below-horizon fade
    f32 params[4];  // turbidity, sunIntensity, cosSunRadius, skyIntensity
    f32 ground[4];  // rgb
};

}  // namespace

VulkanSkyPass::VulkanSkyPass(VkDevice device, const std::string& shaderDir,
                             VkSampleCountFlagBits samples, VkFormat colorFormat,
                             VkFormat depthFormat)
    : m_device(device) {
    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push.offset = 0;
    push.size = sizeof(SkyPush);
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
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
}

void VulkanSkyPass::Record(VkCommandBuffer cmd, const SkyParams& sky,
                           const Math::Mat4& view, const Math::Mat4& viewProj) {
    using namespace Math;
    Mat4 invView = Inverse(view);
    Mat4 invViewProj = Inverse(viewProj);

    Vec3 sunDir = Normalize(sky.sunDirection);
    // Fade the analytic sky out as the sun drops below the horizon (Preetham is
    // invalid there); smooth over a few degrees of elevation.
    f32 fade = (sunDir.y - (-0.02f)) / (0.12f - (-0.02f));
    fade = fade < 0.0f ? 0.0f : (fade > 1.0f ? 1.0f : fade);

    f32 sunRadius = sky.sunSizeDegrees * 0.5f * (kPi / 180.0f);

    SkyPush pc{};
    pc.invViewProj = invViewProj;
    pc.cameraPos[0] = invView.m[12];
    pc.cameraPos[1] = invView.m[13];
    pc.cameraPos[2] = invView.m[14];
    pc.cameraPos[3] = 1.0f;
    pc.sunDir[0] = sunDir.x;
    pc.sunDir[1] = sunDir.y;
    pc.sunDir[2] = sunDir.z;
    pc.sunDir[3] = fade;
    pc.params[0] = sky.turbidity;
    pc.params[1] = sky.sunIntensity;
    pc.params[2] = std::cos(sunRadius);
    pc.params[3] = sky.skyIntensity;
    pc.ground[0] = sky.groundColor.x;
    pc.ground[1] = sky.groundColor.y;
    pc.ground[2] = sky.groundColor.z;
    pc.ground[3] = 1.0f;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    vkCmdPushConstants(cmd, m_layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(SkyPush), &pc);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

}  // namespace Luma
