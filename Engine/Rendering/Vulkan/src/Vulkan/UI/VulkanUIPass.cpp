#include "Vulkan/UI/VulkanUIPass.h"

#include <algorithm>
#include <cstring>

#include "Vulkan/VulkanShader.h"

namespace Luma {
namespace {

struct UIPushConstants {
    f32 screenSize[2];
};

}  // namespace

VulkanUIPass::VulkanUIPass(VkPhysicalDevice physical, VkDevice device,
                           VkCommandPool uploadPool, VkQueue uploadQueue,
                           VkFormat colorFormat, u32 framesInFlight,
                           const std::string& shaderDir)
    : m_physical(physical),
      m_device(device),
      m_uploadPool(uploadPool),
      m_uploadQueue(uploadQueue) {
    // Sampler (linear, clamp).
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VK_CHECK(vkCreateSampler(device, &samplerInfo, nullptr, &m_sampler));

    // Descriptor set layout: one combined image sampler for the fragment stage.
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr,
                                         &m_setLayout));

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 128;
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 128;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr,
                                    &m_descriptorPool));

    // Pipeline layout: set 0 + push constant (screen size).
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(UIPushConstants);
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_setLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
                                    &m_pipelineLayout));

    // Pipeline.
    VkShaderModule vert = LoadShaderModule(device, shaderDir + "/ui.vert.spv");
    VkShaderModule frag = LoadShaderModule(device, shaderDir + "/ui.frag.spv");
    LUMA_ASSERT(vert && frag, "failed to load UI shaders");

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkVertexInputBindingDescription vertexBinding{};
    vertexBinding.binding = 0;
    vertexBinding.stride = sizeof(UIVertex);
    vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[3]{};
    attrs[0].location = 0;
    attrs[0].binding = 0;
    attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[0].offset = offsetof(UIVertex, x);
    attrs[1].location = 1;
    attrs[1].binding = 0;
    attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[1].offset = offsetof(UIVertex, u);
    attrs[2].location = 2;
    attrs[2].binding = 0;
    attrs[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    attrs[2].offset = offsetof(UIVertex, color);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &vertexBinding;
    vertexInput.vertexAttributeDescriptionCount = 3;
    vertexInput.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &blend;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                      VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &colorFormat;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderingInfo;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &raster;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                       nullptr, &m_pipeline));
    vkDestroyShaderModule(device, vert, nullptr);
    vkDestroyShaderModule(device, frag, nullptr);

    m_vertexBuffers.resize(framesInFlight);
    m_indexBuffers.resize(framesInFlight);

    // 1x1 white texture so solid-colored quads pass their vertex color through.
    const u32 white = 0xFFFFFFFFu;
    m_whiteTexture = CreateTexture(1, 1, &white);

    LUMA_LOG_INFO("Vulkan", "UI pass ready");
}

VulkanUIPass::~VulkanUIPass() {
    m_textures.clear();  // frees descriptor sets + images
    for (auto& buffer : m_vertexBuffers) DestroyBuffer(m_device, buffer);
    for (auto& buffer : m_indexBuffers) DestroyBuffer(m_device, buffer);
    if (m_pipeline) vkDestroyPipeline(m_device, m_pipeline, nullptr);
    if (m_pipelineLayout)
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    if (m_descriptorPool)
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    if (m_setLayout)
        vkDestroyDescriptorSetLayout(m_device, m_setLayout, nullptr);
    if (m_sampler) vkDestroySampler(m_device, m_sampler, nullptr);
}

TextureHandle VulkanUIPass::CreateTexture(u32 width, u32 height,
                                          const void* pixels) {
    auto texture = std::make_unique<VulkanTexture>(
        m_physical, m_device, m_uploadPool, m_uploadQueue, width, height,
        pixels, m_descriptorPool, m_setLayout, m_sampler);
    TextureHandle handle = m_nextHandle++;
    m_textures.emplace(handle, std::move(texture));
    return handle;
}

void VulkanUIPass::DestroyTexture(TextureHandle handle) {
    if (handle == m_whiteTexture) return;  // owned by the pass
    m_textures.erase(handle);
}

void VulkanUIPass::EnsureBuffer(GpuBuffer& buffer, VkDeviceSize needed,
                                VkBufferUsageFlags usage) {
    if (buffer.size >= needed && buffer.buffer != VK_NULL_HANDLE) return;
    if (buffer.buffer != VK_NULL_HANDLE) DestroyBuffer(m_device, buffer);
    VkDeviceSize newSize = std::max<VkDeviceSize>(needed, 4096);
    newSize += newSize / 2;  // headroom to reduce reallocation churn
    buffer = CreateBuffer(m_physical, m_device, newSize, usage,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          true);
}

void VulkanUIPass::Record(VkCommandBuffer cmd, u32 frame,
                          const UIDrawData& data) {
    if (data.vertexCount == 0 || data.indexCount == 0 ||
        data.commandCount == 0) {
        return;
    }

    GpuBuffer& vtx = m_vertexBuffers[frame];
    GpuBuffer& idx = m_indexBuffers[frame];
    EnsureBuffer(vtx, sizeof(UIVertex) * data.vertexCount,
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    EnsureBuffer(idx, sizeof(u32) * data.indexCount,
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    std::memcpy(vtx.mapped, data.vertices,
                sizeof(UIVertex) * data.vertexCount);
    std::memcpy(idx.mapped, data.indices, sizeof(u32) * data.indexCount);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    UIPushConstants push{{data.displayWidth, data.displayHeight}};
    vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(push), &push);

    VkViewport viewport{};
    viewport.width = data.displayWidth;
    viewport.height = data.displayHeight;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vtx.buffer, &offset);
    vkCmdBindIndexBuffer(cmd, idx.buffer, 0, VK_INDEX_TYPE_UINT32);

    for (u32 i = 0; i < data.commandCount; ++i) {
        const UIDrawCommand& c = data.commands[i];

        VkRect2D scissor{};
        i32 x = static_cast<i32>(c.clipX);
        i32 y = static_cast<i32>(c.clipY);
        scissor.offset.x = std::max(0, x);
        scissor.offset.y = std::max(0, y);
        scissor.extent.width = static_cast<u32>(std::max(0.0f, c.clipW));
        scissor.extent.height = static_cast<u32>(std::max(0.0f, c.clipH));
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        TextureHandle handle = c.texture ? c.texture : m_whiteTexture;
        auto it = m_textures.find(handle);
        if (it == m_textures.end()) it = m_textures.find(m_whiteTexture);
        VkDescriptorSet set = it->second->DescriptorSet();
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_pipelineLayout, 0, 1, &set, 0, nullptr);

        vkCmdDrawIndexed(cmd, c.indexCount, 1, c.indexOffset, 0, 0);
    }
}

}  // namespace Luma
