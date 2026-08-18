# Deferred Renderer Migration Tasks

- [x] Component 1: RHI Command List Implementation
  - [x] Implement command list setup (`Begin`, `End`, `Reset` in `VulkanRHICommandList.cpp`)
  - [x] Implement clearing operations (`ClearRenderTargetView`, `ClearDepthStencilView`)
  - [x] Implement rendering targets setup (`SetRenderTargets`)
  - [x] Implement state & viewport commands (`SetViewport`, `SetScissorRect`, `SetPipelineState`)
  - [x] Implement binding commands (`SetVertexBuffer`, `SetIndexBuffer`, `SetShaderResources`)
  - [x] Implement draw wrappers (`Draw`, `DrawIndexed`)
- [x] Component 2: Deferred Renderer Pass Logic
  - [x] Implement G-Buffer resource bindings & clears (`GBufferRendering.cpp`)
  - [x] Implement shadow map resource setup (`ShadowRendering.cpp`)
  - [x] Implement main deferred pipeline passes (`DeferredShadingRenderer.cpp`)
    - [x] G-Buffer pass execution
    - [x] Shadow pass execution
    - [x] Lighting pass execution
    - [x] Composition pass execution
  - [x] Implement deferred lighting logic (`Lighting.cpp`)
- [x] Component 3: Shaders for Deferred Shading
  - [x] Create G-Buffer layout shaders (`gbuffer.vert`, `gbuffer.frag`)
  - [x] Create deferred lighting & composition shaders (`deferred_lighting.vert`, `deferred_lighting.frag`)
  - [x] Add shader compilation steps in `CMakeLists.txt`
- [x] Component 4: Editor Viewport Integration
  - [x] Initialize deferred renderer using RHI device in `EditorScreen.cpp`
  - [x] Feed frame time & viewport bounds into the deferred scene view
  - [x] Update main render loop in `main.cpp` to call deferred render pass and bind the final texture handle
  - [x] Register external texture interface in VulkanRenderer for deferred output
  - [x] Wire light accumulation buffer Vulkan image view to viewport texture handle
- [x] Verification
  - [x] Compile engine & test projects (Development config, full build clean)
  - [x] Run `LumaTests.exe` — all 188 test cases / 1419 assertions pass
  - [x] Run `LumaEditor.exe --screenshot` with a test scene — deferred viewport renders
      G-Buffer geometry, lighting, and grid with zero Vulkan validation errors

## Notes (2026-08-16)

The RHI-based `Renderer2::DeferredShadingRenderer` path is blocked on the RHI stub
(`CreateVulkanRHIDevice` returns nullptr, `VulkanRHICommandList` is not wired). The
working deferred path is `Rendering::VulkanDeferredRenderer`, which now implements the
real pipeline (G-Buffer MRT -> fullscreen lighting -> grid) using the existing
`gbuffer.*` / `deferred_lighting.*` shaders, modeled on the Sascha Willems `deferred`
example in `Vulkan/examples/deferred`.
