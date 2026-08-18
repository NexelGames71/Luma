#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>

#include "Luma/Core/Log.h"
#include "Luma/Platform/Window.h"
#include "Luma/RHI/RHI.h"
#include "Luma/RHI/RHIContext.h"
#include "Luma/RHI/RHIResources.h"
#include "Luma/RHI/RHITypes.h"
#include "Luma/RHI/VulkanRenderer.h"
#include "Luma/RHI/VulkanRHIDevice.h"

// Tests for the VulkanRHIDevice adapter. The adapter is constructed on top
// of a real VulkanRenderer (which owns a VulkanInstance + VulkanDevice);
// that's how the editor wires it. Each test builds a minimal window,
// instantiates the renderer, and constructs the adapter.

using namespace Luma;

namespace {

struct RhiFixture {
    std::unique_ptr<Window> window;
    std::unique_ptr<Renderer> renderer;

    RhiFixture() {
        Log::Init(LogLevel::Warning);  // suppress chatter during tests
        WindowProps props;
        props.title = "Luma RHI Test";
        props.width = 64;
        props.height = 64;
        window = Window::Create(props);

        RendererConfig rc;
        rc.appName = "Luma RHI Test";
        rc.enableValidation = false;
        rc.vsync = false;
        renderer = CreateVulkanRenderer(*window, rc);
    }
};

}  // namespace

TEST_CASE("VulkanRHIDevice exposes a non-empty device name", "[rhi][adapter]") {
    RhiFixture f;
    REQUIRE(f.renderer != nullptr);

    auto* device = RHI::CreateVulkanRHIDevice(
        f.renderer->GetVulkanInstance(), f.renderer->GetVulkanDevice());
    REQUIRE(device != nullptr);

    const char* name = device->GetDeviceName();
    REQUIRE(name != nullptr);
    REQUIRE(std::string(name).size() > 0);

    delete device;
}

TEST_CASE("VulkanRHIDevice returns non-null context, queues, and factories",
          "[rhi][adapter]") {
    RhiFixture f;
    REQUIRE(f.renderer != nullptr);

    auto* device = RHI::CreateVulkanRHIDevice(
        f.renderer->GetVulkanInstance(), f.renderer->GetVulkanDevice());
    REQUIRE(device != nullptr);

    RHI::RHIContext* ctx = device->GetContext();
    REQUIRE(ctx != nullptr);
    REQUIRE(ctx->GetDevice() == device);

    REQUIRE(ctx->GetResourceFactory() != nullptr);
    REQUIRE(ctx->GetPipelineStateFactory() != nullptr);
    REQUIRE(ctx->GetShaderFactory() != nullptr);

    REQUIRE(ctx->GetGraphicsQueue() != nullptr);
    REQUIRE(ctx->GetComputeQueue() != nullptr);
    REQUIRE(ctx->GetCopyQueue() != nullptr);
    REQUIRE(std::string(ctx->GetGraphicsQueue()->GetQueueType()) ==
            "graphics");

    delete device;
}

TEST_CASE("VulkanRHIDevice WaitIdle returns without crashing", "[rhi][adapter]") {
    RhiFixture f;
    auto* device = RHI::CreateVulkanRHIDevice(
        f.renderer->GetVulkanInstance(), f.renderer->GetVulkanDevice());
    REQUIRE(device != nullptr);
    device->WaitIdle();
    device->Flush();
    REQUIRE_FALSE(device->IsDeviceLost());
    delete device;
}

TEST_CASE("VulkanRHIDevice IsFormatSupported matches the formats the "
          "deferred renderer uses",
          "[rhi][adapter]") {
    RhiFixture f;
    auto* device = RHI::CreateVulkanRHIDevice(
        f.renderer->GetVulkanInstance(), f.renderer->GetVulkanDevice());
    REQUIRE(device != nullptr);

    using F = RHI::ETextureFormat;
    using U = RHI::ETextureUsage;
    REQUIRE(device->IsFormatSupported(F::R8G8B8A8_UNORM,
                                        U::RenderTarget));
    REQUIRE(device->IsFormatSupported(F::R16G16B16A16_FLOAT,
                                        U::RenderTarget));
    REQUIRE(device->IsFormatSupported(F::R8G8B8A8_UNORM,
                                        U::ShaderResource));
    REQUIRE(device->IsFormatSupported(F::D32_FLOAT, U::DepthStencil));
    REQUIRE_FALSE(device->IsFormatSupported(F::BC7_SRGB,
                                             U::RenderTarget));
    delete device;
}

TEST_CASE("VulkanRHIResourceFactory creates and destroys a host-visible buffer",
          "[rhi][adapter][resources]") {
    RhiFixture f;
    auto* device = RHI::CreateVulkanRHIDevice(
        f.renderer->GetVulkanInstance(), f.renderer->GetVulkanDevice());
    REQUIRE(device != nullptr);

    auto* factory = device->GetResourceFactory();
    REQUIRE(factory != nullptr);

    RHI::BufferDesc bd{};
    bd.size = 256;
    bd.usage = RHI::EBufferUsage::Uniform;
    bd.cpuAccess = RHI::EBufferCPUAccess::Write;
    RHI::RHIBuffer* buf = factory->CreateBuffer(bd);
    REQUIRE(buf != nullptr);
    REQUIRE(buf->GetSize() == 256u);

    void* mapped = buf->Map();
    REQUIRE(mapped != nullptr);
    buf->Unmap();

    factory->DestroyBuffer(buf);
    delete device;
}

TEST_CASE("VulkanRHIResourceFactory creates and destroys a render-target "
          "texture (the path the deferred GBuffer uses)",
          "[rhi][adapter][resources]") {
    RhiFixture f;
    auto* device = RHI::CreateVulkanRHIDevice(
        f.renderer->GetVulkanInstance(), f.renderer->GetVulkanDevice());
    REQUIRE(device != nullptr);

    auto* factory = device->GetResourceFactory();
    REQUIRE(factory != nullptr);

    RHI::TextureDesc td{};
    td.width = 64;
    td.height = 64;
    td.mipLevels = 1;
    td.arraySize = 1;
    td.format = RHI::ETextureFormat::R8G8B8A8_UNORM;
    td.usage = RHI::ETextureUsage::RenderTarget |
               RHI::ETextureUsage::ShaderResource;
    td.flags = RHI::ETextureFlags::RenderTargetable |
               RHI::ETextureFlags::ShaderResource;

    RHI::RHITexture* tex = factory->CreateTexture(td);
    REQUIRE(tex != nullptr);
    REQUIRE(tex->GetSize() ==
            u64(64 * 64 * 4));  // RGBA8 = 4 bytes per pixel

    factory->DestroyTexture(tex);
    delete device;
}

TEST_CASE("VulkanRHIResourceFactory creates and destroys a depth-stencil "
          "texture",
          "[rhi][adapter][resources]") {
    RhiFixture f;
    auto* device = RHI::CreateVulkanRHIDevice(
        f.renderer->GetVulkanInstance(), f.renderer->GetVulkanDevice());
    REQUIRE(device != nullptr);

    auto* factory = device->GetResourceFactory();
    REQUIRE(factory != nullptr);

    RHI::TextureDesc td{};
    td.width = 32;
    td.height = 32;
    td.format = RHI::ETextureFormat::D32_FLOAT;
    td.usage = RHI::ETextureUsage::DepthStencil;
    td.flags = RHI::ETextureFlags::DepthStencilTargetable;

    RHI::RHITexture* tex = factory->CreateTexture(td);
    REQUIRE(tex != nullptr);
    factory->DestroyTexture(tex);

    delete device;
}

TEST_CASE("VulkanRHICommandList can be created/destroyed (still stubs)",
          "[rhi][adapter][commands]") {
    RhiFixture f;
    auto* device = RHI::CreateVulkanRHIDevice(
        f.renderer->GetVulkanInstance(), f.renderer->GetVulkanDevice());
    REQUIRE(device != nullptr);

    RHI::RHICommandList* list = device->CreateCommandList();
    REQUIRE(list != nullptr);
    device->DestroyCommandList(list);

    delete device;
}
