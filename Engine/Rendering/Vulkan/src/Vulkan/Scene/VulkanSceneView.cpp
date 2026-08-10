#include "Vulkan/Scene/VulkanSceneView.h"

#include <cstring>
#include <limits>

#include "Luma/Math/Math.h"
#include "Vulkan/VulkanShader.h"

namespace Luma {
namespace {

struct SceneVertex {
    f32 px, py, pz;
    f32 r, g, b;
};

// Ground quad (verts 0..3) + cube (verts 4..11).
const SceneVertex kVertices[] = {
    // Ground (y=0)
    {-4, 0, -4, 0.15f, 0.16f, 0.19f},
    {4, 0, -4, 0.15f, 0.16f, 0.19f},
    {4, 0, 4, 0.11f, 0.12f, 0.15f},
    {-4, 0, 4, 0.11f, 0.12f, 0.15f},
    // Cube
    {-0.5f, -0.5f, -0.5f, 0.90f, 0.25f, 0.25f},
    {0.5f, -0.5f, -0.5f, 0.30f, 0.85f, 0.35f},
    {0.5f, 0.5f, -0.5f, 0.30f, 0.55f, 0.95f},
    {-0.5f, 0.5f, -0.5f, 0.95f, 0.85f, 0.30f},
    {-0.5f, -0.5f, 0.5f, 0.85f, 0.35f, 0.85f},
    {0.5f, -0.5f, 0.5f, 0.30f, 0.85f, 0.90f},
    {0.5f, 0.5f, 0.5f, 0.95f, 0.95f, 0.95f},
    {-0.5f, 0.5f, 0.5f, 0.95f, 0.55f, 0.25f},
};

const u32 kIndices[] = {
    // Ground
    0, 1, 2, 2, 3, 0,
    // Cube (+4 vertex offset baked in)
    4, 5, 6, 6, 7, 4,        // front
    9, 8, 11, 11, 10, 9,     // back
    8, 4, 7, 7, 11, 8,       // left
    5, 9, 10, 10, 6, 5,      // right
    7, 6, 10, 10, 11, 7,     // top
    8, 9, 5, 5, 4, 8,        // bottom
};

constexpr u32 kGroundIndexCount = 6;
constexpr u32 kCubeIndexCount = 36;

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

    CreateGeometry();
    CreatePipeline(shaderDir);
}

VulkanSceneView::~VulkanSceneView() {
    vkDeviceWaitIdle(m_device);
    DestroyTargets();
    DestroyBuffer(m_device, m_vertexBuffer);
    DestroyBuffer(m_device, m_indexBuffer);
    if (m_pipeline) vkDestroyPipeline(m_device, m_pipeline, nullptr);
    if (m_pipelineLayout)
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    if (m_fence) vkDestroyFence(m_device, m_fence, nullptr);
    if (m_pool) vkDestroyCommandPool(m_device, m_pool, nullptr);
}

void VulkanSceneView::CreateGeometry() {
    m_vertexBuffer = CreateBuffer(m_physical, m_device, sizeof(kVertices),
                                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  true);
    std::memcpy(m_vertexBuffer.mapped, kVertices, sizeof(kVertices));

    m_indexBuffer = CreateBuffer(m_physical, m_device, sizeof(kIndices),
                                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 true);
    std::memcpy(m_indexBuffer.mapped, kIndices, sizeof(kIndices));
}

void VulkanSceneView::CreatePipeline(const std::string& shaderDir) {
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
    binding.stride = sizeof(SceneVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attrs[2]{};
    attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SceneVertex, px)};
    attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SceneVertex, r)};
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
    rs.cullMode = VK_CULL_MODE_NONE;
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

    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push.offset = 0;
    push.size = sizeof(Math::Mat4);
    VkPipelineLayoutCreateInfo plInfo{};
    plInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plInfo.pushConstantRangeCount = 1;
    plInfo.pPushConstantRanges = &push;
    VK_CHECK(vkCreatePipelineLayout(m_device, &plInfo, nullptr,
                                    &m_pipelineLayout));

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
    info.layout = m_pipelineLayout;
    VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &info,
                                       nullptr, &m_pipeline));
    vkDestroyShaderModule(m_device, vert, nullptr);
    vkDestroyShaderModule(m_device, frag, nullptr);
}

void VulkanSceneView::CreateTargets(u32 width, u32 height) {
    auto makeImage = [&](VkFormat format, VkImageUsageFlags usage,
                         VkImage& image, VkDeviceMemory& mem,
                         VkImageView& view, VkImageAspectFlags aspect) {
        VkImageCreateInfo ii{};
        ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ii.imageType = VK_IMAGE_TYPE_2D;
        ii.format = format;
        ii.extent = {width, height, 1};
        ii.mipLevels = 1;
        ii.arrayLayers = 1;
        ii.samples = VK_SAMPLE_COUNT_1_BIT;
        ii.tiling = VK_IMAGE_TILING_OPTIMAL;
        ii.usage = usage;
        ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VK_CHECK(vkCreateImage(m_device, &ii, nullptr, &image));

        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(m_device, image, &req);
        VkMemoryAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = FindMemoryType(
            m_physical, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
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
              m_color, m_colorMem, m_colorView, VK_IMAGE_ASPECT_COLOR_BIT);
    makeImage(kDepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
              m_depth, m_depthMem, m_depthView, VK_IMAGE_ASPECT_DEPTH_BIT);
    m_width = width;
    m_height = height;
}

void VulkanSceneView::DestroyTargets() {
    if (m_colorView) vkDestroyImageView(m_device, m_colorView, nullptr);
    if (m_color) vkDestroyImage(m_device, m_color, nullptr);
    if (m_colorMem) vkFreeMemory(m_device, m_colorMem, nullptr);
    if (m_depthView) vkDestroyImageView(m_device, m_depthView, nullptr);
    if (m_depth) vkDestroyImage(m_device, m_depth, nullptr);
    if (m_depthMem) vkFreeMemory(m_device, m_depthMem, nullptr);
    m_colorView = VK_NULL_HANDLE;
    m_color = VK_NULL_HANDLE;
    m_colorMem = VK_NULL_HANDLE;
    m_depthView = VK_NULL_HANDLE;
    m_depth = VK_NULL_HANDLE;
    m_depthMem = VK_NULL_HANDLE;
}

TextureHandle VulkanSceneView::Render(u32 width, u32 height, f32 dt) {
    if (width == 0 || height == 0) return m_textureHandle;
    m_time += dt;

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
        vkCmdPipelineBarrier(m_cmd, srcS, dstS, 0, 0, nullptr, 0, nullptr, 1,
                             &b);
    };

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
    color.imageView = m_colorView;
    color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.clearValue.color = {{0.06f, 0.07f, 0.09f, 1.0f}};

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

    vkCmdBindPipeline(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(m_cmd, 0, 1, &m_vertexBuffer.buffer, &offset);
    vkCmdBindIndexBuffer(m_cmd, m_indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    using namespace Math;
    f32 aspect = static_cast<f32>(width) / static_cast<f32>(height);
    Mat4 proj = Perspective(Radians(50.0f), aspect, 0.1f, 100.0f);
    Mat4 view = LookAt(Vec3(4.0f, 3.5f, 6.0f), Vec3(0.0f, 0.8f, 0.0f),
                       Vec3(0.0f, 1.0f, 0.0f));
    Mat4 viewProj = proj * view;

    vkCmdPushConstants(m_cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(Mat4), &viewProj);
    vkCmdDrawIndexed(m_cmd, kGroundIndexCount, 1, 0, 0, 0);

    Mat4 model = Translate(Vec3(0.0f, 1.0f, 0.0f)) * RotateY(m_time) *
                 RotateX(m_time * 0.4f);
    Mat4 mvp = viewProj * model;
    vkCmdPushConstants(m_cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(Mat4), &mvp);
    vkCmdDrawIndexed(m_cmd, kCubeIndexCount, 1, kGroundIndexCount, 0, 0);

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
