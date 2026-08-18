# Deferred Renderer — Deep Scan

> Compiled for an upcoming engineering session on the Luma Engine deferred
> renderer. Code is read against the working tree at the time of the scan;
> re-run git status / git diff HEAD before applying changes — the topic
> branch (or staged + untracked changes) may have moved on since this scan.

## 1. Where the deferred renderer lives

There is **one** active deferred path and **one** draft RHI path that
collides with it. They share most of the public surface but the active
implementation is the raw-Vulkan one.

- **Active (raw Vulkan, used by the editor):**
  `Engine/Rendering/Vulkan/include/Luma/Rendering/Vulkan/VulkanDeferredRenderer.h`
  + `src/VulkanDeferredRenderer.cpp`. Owned by `VulkanRenderer::m_vulkanDeferredRenderer`,
  accessed from `Editor/LumaEditor/main.cpp:305-370` and exposed via
  `Luma::Renderer::GetVulkanDeferredRenderer()`.
- **RHI stub (target):** `Engine/Rendering/Renderer/{include,src}/{DeferredShadingRenderer.h,DeferredShadingRenderer.cpp}`
  declares `Renderer2::DeferredShadingRenderer` (and the G-Buffer / lighting /
  shadow subsystems it composes). It compiles and runs, but the RHI device
  it needs returns nullptr at runtime, so it never actually renders — see
  `task.md`:
  > The RHI-based `Renderer2::DeferredShadingRenderer` path is blocked on
  > the RHI stub (`CreateVulkanRHIDevice` returns nullptr, `VulkanRHICommandList`
  > is not wired). The working deferred path is `Rendering::VulkanDeferredRenderer`…

**Conclusion for the next session:** "the deferred renderer" =
`Luma::Rendering::VulkanDeferredRenderer` plus its shaders. The RHI
abstraction (`Renderer2::DeferredShadingRenderer` etc.) is a parallel future
facade we should keep in sync but not break; both compile today and the
editor uses only the raw-Vulkan one.

## 2. Where it is wired into the editor

Trace (top-down, with line numbers):

1. `Editor/LumaEditor/main.cpp:156` — `CreateVulkanRenderer(*window, rc)`
   instantiates `VulkanRenderer`.
2. `Engine/Rendering/Vulkan/src/Vulkan/VulkanRenderer.cpp:79-89` —
   inside `VulkanRenderer::VulkanRenderer`, the
   `Rendering::VulkanDeferredRenderer` is created with
   `m_device->Physical()` / `Logical()` / `GraphicsQueue()` / queue family
   + the `VulkanUIPass` reference + `LUMA_SHADER_DIR`. It is then
   `Initialize(1920, 1080)`-d. (If init fails, the unique_ptr is reset and
   `GetVulkanDeferredRenderer()` returns nullptr — the editor handles the
   null case by going black.)
3. `Editor/LumaEditor/main.cpp:305` — `renderer->GetVulkanDeferredRenderer()`
   grabs the opaque void* and reinterprets it as
   `Rendering::VulkanDeferredRenderer*`.
4. `Editor/LumaEditor/main.cpp:351-386` — every editor frame, before
   `BeginFrame`, the renderer:
   - Computes the viewport rect (`editor->ViewportRect()`).
   - Calls `SetViewportDimensions(w, h)` (triggers a `Cleanup` + `Initialize`
     if the size actually changed — it's a teardown-and-rebuild path).
   - Calls `PrepareScene()` (currently empty).
   - Calls `RenderScene(deferredScene)`.
   - Calls `GetViewportTextureHandle()` and `editor->SetViewportTexture(handle)`
     so the Slate viewport panel can show it.
5. `Editor/LumaEditor/EditorScreen.cpp:493-534` — `BuildDeferredSceneView()`:
   builds the RHI `Luma::SceneView` (via `BuildSceneView()` at `:243-449`),
   then translates it into `Renderer2::DeferredSceneView` (note: this is
   the same shape as `Renderer2::DeferredSceneView` from
   `Engine/Rendering/Renderer/include/Luma/Renderer/DeferredShadingRenderer.h:40-58`,
   but the working renderer's `RenderScene` is currently ignoring every
   field except `width`/`height`). Width/height are still hard-coded to
   `1920/1080` at `:524-525` — the per-frame viewport size comes from
   `main.cpp:360-361` and overwrites them.

So the public contract `RenderScene(const Renderer2::DeferredSceneView&)`
must consume (even if everything except `sceneData` is unused right now):

- `sceneData` → `const Luma::SceneView*` (the RHI scene with sky, lighting,
  instances, lights, lines).
- `lightingParams` → convenience pointer (the forward renderer also pulls
  it directly from `sceneData->lighting`).
- `width` / `height`.
- `cameraPosition`, `cameraDirection`, `viewMatrix`, `projectionMatrix`,
  `viewProjectionMatrix`, `fov`, `nearPlane`, `farPlane`, `time`,
  `deltaTime` (some already filled from `EditorScreen::BuildDeferredSceneView`).

The RHI `Luma::SceneView` (full definition at
`Engine/Rendering/RHI/include/Luma/RHI/Renderer.h:196-221`) also carries
`SkyParams`, `GridParams`, `LightingParams`, `SceneInstance*` (+count),
`SceneLight*` (+count), `LineVertex* lines`, `LineVertex* overlayLines`.
These will all eventually flow through the deferred renderer.

## 3. Pipeline architecture (the design the deferred renderer matches)

`task.md` already records this; reconfirmed from code:

```
G-Buffer pass (offscreen, MRT into the GBuffer struct):
  binding 0: albedo  R8G8B8A8_UNORM
  binding 1: normal  R16G16B16A16_SFLOAT, roughness in .a
  binding 2: material R8G8B8A8_UNORM (metallic/spec/AO)
  depth:    D32_SFLOAT
  lit by: gbuffer.vert + gbuffer.frag
           push constant: mat4 model, albedo+metallic, roughness,
                          ivec4 baseColor/normal/roughness/metallic tex idx

Sky-atmosphere pass (background pixels in the lighting shader):
  deferred_lighting.frag sees depth==1.0, reconstructs view ray,
  uses sky_atmosphere.glsl (same module the forward sky uses).

Lighting pass (fullscreen triangle, writes lightAccum):
  deferred_lighting.vert: gl_VertexIndex -> fullscreen tri, no VB
  deferred_lighting.frag:
    set 0 bindings 0..3: gBuffer albedo/normal/material/depth
    binding 4:           LightingUBO (camera, sun, shadows, lights, atmo)
    binding 5:           sampler2DArray shadow map (4 cascades)
    reuses AtmosphereParams + RenderSkyColor(...) from sky_atmosphere.glsl
    output: tonemapped + gamma-encoded -> lightAccum

Grid pass (overlay, from VulkanGridPass):
  depth-tested against the G-buffer depth so geometry occludes it
```

Format layout already declared on `VulkanDeferredRenderer.h:263-267`
(`kAlbedoFormat`, `kNormalFormat`, `kMaterialFormat`, `kDepthFormat`,
`kLightFormat`) and (re)used by `CreateRenderPass()` / `CreateFramebuffer()`.

The forward-path blueprint for the same problem (cascaded shadows +
sky + grid + PBR mesh + MSAA + texture registration) is in
`Engine/Rendering/Vulkan/src/Vulkan/Scene/VulkanSceneView.cpp` /
`.h`, already fully wired and tested (see §5 for the pattern we
should copy).

## 4. What exists vs. what is stubbed

`VulkanDeferredRenderer.cpp` (`src/VulkanDeferredRenderer.cpp`,
757 lines) is in the **shape of a working deferred renderer but most of its
methods are no-ops**. Concrete state:

### Already implemented
- `CreateCommandPool()` — pools & fences & command buffer (`Initialize:91-111`).
- `CreateGBuffer()` — five `Attachment`s (albedo/normal/material/depth/lightAccum)
  (`CreateGBuffer:238-274`).
- `CreateRenderPass()` — MRT render pass with the 4 color + depth attachments,
  explicit subpass dependencies (`CreateRenderPass:276-382`).
- `CreateAttachment()` / `DestroyAttachment()` — single-allocation image +
  view helper with `FindMemoryType` lookup (`:384-459`).
- `CreateFramebuffer()` — wires the 5 views together (`:461-484`).
- `CreateSampler()` — linear, repeat, no anisotropy, no compare (`:486-511`).
- `DestroyAttachment` ×5, framebuffer, render pass, sampler cleanup
  (`Cleanup:192-219`).
- `Cleanup()` — destruction in reverse order; handles a duplicated block
  (`m_gbufferSetLayout` is destroyed twice — see §6, bug #5).
- `SetViewportDimensions()` — currently teardown-and-rebuild via
  `Cleanup()`+`Initialize()` (`:591-598`). That triggers a full G-Buffer +
  pipeline rebuild on every viewport-pixel change; the rest of the codebase
  uses in-place resize. See §6 bug #4.
- `PrepareScene()` — empty no-op (`:600-602`).
- `Initialize()` — calls everything that exists, in dependency order
  (`:76-131`).

### Stubs (the body is `(void)var; return true;`)
- `CreateUBOs()` — does nothing (`:513-517`); GbufferUBO (`viewProj+camPos`)
  and `LightingUBO` (matches `deferred_lighting.frag`) are declared in the
  header but never allocated.
- `CreateLayouts()` — empty (`:519-523`); `m_gbufferSetLayout`,
  `m_gbufferLayout`, `m_lightingSetLayout`, `m_lightingLayout` are still
  declared in the header but never created.
- `CreateDescriptorSets()` — empty (`:525-529`); `m_gbufferPool/Set`,
  `m_lightingPool/Set` similarly unbuilt.
- `CreatePipelines(shaderDir)` — never compiles `gbuffer.*.spv` or
  `deferred_lighting.*.spv` (`:531-537`). `m_gbufferPipeline`/`m_lightingPipeline`
  stay null.
- `CreatePrimitives()` — empty (`:539-542`); the four `MeshPrimitive`
  GPU meshes + material push contract are not built.
- `CreateShadowResources()` / `DestroyShadowResources()` —
  empty (`:574-582`); the cascaded shadow image + per-layer views are not
  allocated.
- `CreateShadowPipeline(shaderDir)` — empty (`:584-589`); `shadow.vert/.frag`
  exist in the shader folder and the forward `VulkanSceneView::CreateShadowPipeline`
  is the working pattern.
- `GetOrCreateCustomMesh(...)` — returns `nullptr` (`:559-572`);
  needed so the deferred pass can render asset-loaded meshes, not just the
  built-in primitives.
- `UploadMaterialTexture(...)` — returns `-1` (`:746-754`); the G-Buffer
  shader has a `sampler2D uMatTextures[32]` (binding 1) plus
  `ivec4 texIdx` per instance, but no textures get uploaded.

### Fully implemented but **wrong**: `RenderScene()` (`:604-740`)
The current `RenderScene` only clears the light-accumulation image to solid
green and re-presents it — that is the path the editor is calling every
frame, so the viewport shows a flat green rectangle. The 4 GPU-stages it
needs to do instead:
  1. build GBuffer UBO + descriptor set + pipeline,
  2. record GBuffer pass (binds MRT, draws each instance with its push constant,
     transitions the attachments to shader-read),
  3. record shadow pass (depth-only into each cascade layer),
  4. record lighting pass (fullscreen triangle, descriptor set sampled,
     writes to lightAccum),
  5. record grid pass (using `m_gridPass`),
  6. transition lightAccum to `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`
     so `GetViewportTextureHandle()` already routed through `m_uiPass`
     samples cleanly.

The `m_textureHandle` field is declared in the header but never set.
The `EditorScreen::SetViewportTexture` call is gated on `sceneTex != 0`
(`main.cpp:371-373`), so until we register `m_gBuffer.lightAccum.view` with
`m_uiPass`, the Slate viewport panel continues to display the existing UI
texture (or nothing if it was never registered). The flow is parallel to
the one in `VulkanSceneView::Render` (`:783-1194`): on first resize,
`m_uiPass.RegisterExternalTexture(m_colorView)`; on later resizes,
`m_uiPass.UpdateExternalTexture(handle, view)`.

## 5. Working forward-path pattern to copy

`Engine/Rendering/Vulkan/src/Vulkan/Scene/VulkanSceneView.{h,cpp}` is the
closest reference. The exact machinery for what the deferred renderer needs:

### Resource/initialization order
1. Create command pool, single primary command buffer, fence created
   signaled (`VulkanSceneView::VulkanSceneView:73-92`).
2. `CreatePrimitives()` — for each `MeshPrimitive` shape, build a
   vertex+index GpuBuffer, upload `MeshData = BuildPrimitive(kind)`.
3. `CreateShadowResources()` — `VkImage` with `arrayLayers=4`, format D32,
   mipLevels=1, sample=1, usage `DEPTH_STENCIL_ATTACHMENT_BIT|SAMPLED_BIT`.
   Bind dedicated device memory. One `VK_IMAGE_VIEW_TYPE_2D_ARRAY` view for
   sampling + 4 `VK_IMAGE_VIEW_TYPE_2D` per-layer views for rendering.
   A linear-clamp sampler with `BORDER_COLOR_FLOAT_OPAQUE_WHITE`.
4. `CreateSceneUBO()` — one host-visible uniform buffer; one
   `DESCRIPTOR_TYPE_UNIFORM_BUFFER` + one `COMBINED_IMAGE_SAMPLER`
   descriptor set.
5. `CreateLayouts()` — separate layout per pipeline: mesh (UBO + push),
   line (push only), shadow (push only).
6. `CreatePipelines(shaderDir)` — load SPV with `LoadShaderModule()`,
   build each pipeline with the dynamic-rendering extension
   (`VkPipelineRenderingCreateInfo` chained via `pNext`).
7. `CreateTargets(width, height)` — color (R8G8B8A8_UNORM), MSAA color,
   depth (D32), with resolved-to-base layout if MSAA in use.
8. **Texture registration (critical):**
   ```cpp
   m_textureHandle = m_uiPass.RegisterExternalTexture(m_colorView);    // first
   m_uiPass.UpdateExternalTexture(m_textureHandle, newColorView);       // on resize
   ```

### Per-frame Record
```
vkWaitForFences + vkResetFences + vkResetCommandBuffer
vkBeginCommandBuffer(ONE_TIME_SUBMIT_BIT)

# shadow image -> DEPTH_ATTACHMENT_OPTIMAL
for each cascade:
   vkCmdBeginRendering(depth-attachment-only)
   vkCmdBindPipeline(shadow)
   for each instance:
       choose primitive or GetOrCreateCustomMesh
       push lightVP * model
       bind vb/ib, vkCmdDrawIndexed
   vkCmdEndRendering

# GBuffer attachments -> COLOR/DEPTH_ATTACHMENT (and MSAA->color)
vkCmdBeginRendering(color = GBuffer MRT, depth = GBuffer depth)
# sky (depth off), grid (depth-tested)
# mesh pipeline, UBO set, per-instance push (model+albedo+material)
vkCmdEndRendering

# color -> SHADER_READ_ONLY for the UI sampler
vkEndCommandBuffer
vkQueueSubmit(..., fence)
```

### Helpers to crib
- `VkShaderModule LoadShaderModule(device, path)` — in
  `Engine/Rendering/Vulkan/src/Vulkan/VulkanShader.{h,cpp}`; reads the
  `.spv` file. Every module (`VulkanSkyPass`, `VulkanGridPass`,
  `VulkanSceneView`, `Lighting.cpp`, `GBufferRendering.cpp`) uses it.
- `GpuBuffer CreateBuffer(...)`, `DestroyBuffer(...)`, `FindMemoryType(...)` —
  `Vulkan/VulkanMemory.{h,cpp}`.
- `VK_CHECK(expr)` — `Vulkan/VulkanCommon.h` (LOG_ERROR + LUMA_ASSERT).
- `Rendering::AtmosphereParams` + `Rendering::FillAtmosphereParams(...)` —
  `Vulkan/Sky/AtmosphereParams.h:11-73`. Bridges the RHI's `SkyParams` to
  the `std140` struct the `sky_atmosphere.glsl` header uses; the deferred
  lighting shader includes the same `sky_atmosphere.glsl`, so we can
  reuse this header verbatim when filling `LightingUBO::atmo`.
- `VulkanGridPass` is created and owned as a `std::unique_ptr` in the
  deferred renderer already (`m_gridPass`), with the format arguments
  matching `kLightFormat`/`kDepthFormat` — the call from
  `Initialize:122-125` is correct, just nothing else wires it.

## 6. Known concrete issues in the stub (likely to be the first fix list)

1. **`RenderScene` clears to green and returns.** The function only writes
   `clearValue.color = {{0.0f, 0.5f, 0.0f, 1.0f}}` and exits with the log
   line `"Scene rendered successfully (green clear)"` (`:697-739`). Until
   the four real passes replace this, the viewport can never render any
   scene content.
2. **`m_textureHandle` is never set.** `CreatePipelines` /
   `Initialize` should call `m_uiPass.RegisterExternalTexture(...)` /
   `UpdateExternalTexture(...)` around `m_gBuffer.lightAccum.view`, mirroring
   `VulkanSceneView::Render:791-795`. Today both stay 0, so
   `main.cpp:370-373` short-circuits and the viewport panel never sees
   the deferred output.
3. **`Cleanup()` builds a one-off render pass / framebuffer every frame.**
   Lines `:646-737` allocate and immediately destroy a temporary
   `VkRenderPass` + `VkFramebuffer` for the green clear. After the
   real pipeline lands this whole helper goes away.
4. **`SetViewportDimensions` tears down + rebuilds the entire renderer.**
   `:591-598` calls `Cleanup()` + `Initialize(width, height)` for every
   resize. While the GBuffer attachments must be resized, the descriptor
   sets, pipelines, shadow maps, primitive meshes and UBOs do not. Match
   `VulkanSceneView` which recreates only the per-frame
   color/depth/MSAA/lightAccum while keeping the pipelines and shadow
   resources intact.
5. **`Cleanup()` double-destroys `m_gbufferSetLayout` and friends.**
   `:165-175` destroys the gbuffer pool/setLayout, then `:185-190`
   repeats the same block. Not a leak per se but reading the function
   suggests it was edited twice. Same drift for
   `m_gbufferPool/m_lightingPool` reassignments. Cleanup 165-175
   already destroys gbufferPool twice and reassigns twice; the second
   block at 185-190 leaves the same handles being destroyed again.
   Selection: in cleanup, destroy everything once and assign nullptr
   once. Note `m_gbufferSet` is the only set not explicitly destroyed
   anywhere; it goes away with `m_gbufferPool` so that's fine but
   not obvious.
6. **Per-frame green-clear path duplicates the green VkClearValue
   constant in two places** (`:699` and inside the now-unused code path).
   Cosmetic, will fall out when #1 is implemented.
7. **Header/data contract mismatch:**
   `MeshPush` (`:106-110`) declares `i32 texIdx[4]` with `int32_t[]`
   but GLSL's `ivec4` is `int[4]`. The actual `VkPushConstantRange::size`
   in code that hasn't been written yet would need to match GLSL's
   tight packing of `ivec4` (16 bytes, not 16 bytes-aligned-after-mat4).
   Worth keeping in mind when finally implementing the push.
   Same for `LightingUBO` fields: header at `:131-145` declares
   `camForward`, `cascadeSplits`, and `cascadeViewProj[kCascadesUBO]`
   exactly matching the shader's `LightingUBO` block, so this should
   be fine — but `kMaxLights=16` and `kCascadesUBO=4` only allow 16
   lights / 4 cascades in the deferred pass, matching the shader's
   `MAX_LIGHTS=16`, `MAX_CASCADES=4`.
8. **`GBuffer::RenderPass` / `GBuffer::framebuffer` mismatch risk:**
   the subpass's attachment references 4 color attachments
   (albedo, normal, material, lightAccum) and 1 depth. The current
   render pass declares them in that order, but lightAccum is cleared by
   the GBuffer pass via `loadOp = LOAD_OP_CLEAR` and `initialLayout =
   UNDEFINED` — that means the lighting pass output gets wiped every
   frame *before* the lighting pass runs. **The current render pass
   cannot be reused for both the GBuffer clear and the lighting
   write.** Two options: (a) bind the GBuffer MRT pass only with
   albedo/normal/material/depth (skip lightAccum for the GBuffer pass
   and use a separate pipeline for lighting), or (b) move to dynamic
   rendering (`vkCmdBeginRendering`/`vkCmdEndRendering` —
   `VkPipelineRenderingCreateInfo` per pipeline — that's what
   `VulkanSceneView` and `VulkanSkyPass` do). Option (b) matches the
   rest of the codebase (every other pass module uses dynamic
   rendering), so the renderPass object stored in `m_gBuffer.renderPass`
   may end up being deleted in favor of dynamic rendering.

## 7. Files we'll most likely touch

| Path | Why |
| --- | --- |
| `Engine/Rendering/Vulkan/src/VulkanDeferredRenderer.cpp` | implement the four real passes and remove the green-clear stub |
| `Engine/Rendering/Vulkan/include/Luma/Rendering/Vulkan/VulkanDeferredRenderer.h` | add private helpers / members as needed; current header shape is mostly correct |
| `Engine/Rendering/Vulkan/shaders/gbuffer.frag` / `.vert` | already correct; no edits unless we add tex-array support beyond `uMatTextures[32]` |
| `Engine/Rendering/Vulkan/shaders/deferred_lighting.frag` / `.vert` | already correct; no edits unless we add missing tangent for normal mapping (frag uses TBN via gbuffer.vert's `vTangent`, fine) |
| `Engine/Rendering/Vulkan/shaders/sky_atmosphere.glsl` | already shared; no edits |
| `Engine/Rendering/Renderer/include/Luma/Renderer/DeferredShadingRenderer.h` | already declares `Renderer2::DeferredSceneView`; no edits |
| `Engine/Rendering/Renderer/src/DeferredShadingRenderer.cpp` | separate path; keep compiling but no current changes |
| `Engine/Rendering/Renderer/src/Lighting.cpp` / `GBufferRendering.cpp` | separate RHI path; reference only, do not modify alongside |
| `Engine/Rendering/Vulkan/src/Vulkan/Scene/VulkanSceneView.cpp` | reference forward pass; reuse the same patterns verbatim |
| `Engine/Rendering/Vulkan/src/Vulkan/Sky/{VulkanSkyPass,AtmosphereParams}.{h,cpp}` | reuse `FillAtmosphereParams(...)` for `LightingUBO::atmo` |
| `Editor/LumaEditor/main.cpp` | tiny possibility: `deferredScene.width/height` set before `RenderScene` is already correct |
| `Editor/LumaEditor/EditorScreen.cpp` | already passes a populated `DeferredSceneView`; no edits |

## 8. The shorter path: what "working" looks like at the end

1. `Initialize` actually builds:
   - `m_gbufferUBO` + `m_gbufferSet` (binding 0: `CameraUBO`).
   - `m_lightingUBO` + `m_lightingSet` (binding 0..3: GBuffer RTs via
     `m_gBuffer.albedo/normal/material/depth.view` + each subresource at
     `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` after the GBuffer pass;
     binding 4: lightingUBO; binding 5: shadow array view).
   - `m_gbufferPipeline` (`gbuffer.vert.spv` + `gbuffer.frag.spv`, 4-color
     MRT, depth, push=MeshPush).
   - `m_lightingPipeline` (`deferred_lighting.vert.spv` +
     `deferred_lighting.frag.spv`, 1-color lightAccum output, depth off,
     push=null).
   - `m_shadowPipeline` (`shadow.vert.spv` + `shadow.frag.spv`,
     depth-only, `kCascades` layers).
   - `m_primitives[4]` (cube/plane/sphere/cylinder GPU meshes).
   - `m_shadowImage` + 4 layer views + array view + sampler.
2. `PrepareScene` does nothing for now (kept for future culling/visibility).
3. `RenderScene` does, in order:
   1. Wait / reset fence + command buffer.
   2. Fill `m_gbufferUBO` (viewProj + camPos) and `m_lightingUBO`
      (the full `LightingUBO` from `SceneView`/`LightingParams` +
      `SkyParams` + cascade matrices computed like
      `VulkanSceneView::Render:814-873`).
   3. **Shadow pass** — for each cascade: transition layer to DEPTH,
      `vkCmdBeginRendering` depth-only, draw each instance with
      `cascadeVP[c] * model`.
   4. **GBuffer pass** — barrier everything GBuffer-related to
      ATTACHMENT, then `vkCmdBeginRendering` with all 4 colors + depth,
      per instance: push MeshPush (model + material + texIdx) +
      vkCmdDrawIndexed(primitives) or vkCmdDrawIndexed(custom mesh).
   5. **Barrier** GBuffer RTs → SHADER_READ_ONLY and shadow → SHADER_READ_ONLY.
   6. **Lighting pass** — new `vkCmdBeginRendering` into lightAccum
      (depth off), bind lighting set, `vkCmdDraw(3,1,0,0)`.
   7. **Grid pass** — using `m_gridPass->Record(...)` against the
      GBuffer depth, then `vkCmdEndRendering`.
   8. **Transition** lightAccum → SHADER_READ_ONLY.
   9. `vkQueueSubmit + vkWaitForFences`.
   10. Register/update `m_textureHandle` against `m_gBuffer.lightAccum.view`
       via `m_uiPass`.
4. `SetViewportDimensions` only resizes the per-frame `Attachment`s
   (lightAccum, depth — and via `m_gridPass`); keeps pipelines, shadow
   resources, materials and shaders.

## 9. Current snapshot tail

- HEAD: `29b4ebe tweak(editor): grow Content Browser card thumbnails (70 -> 102px)`
- 62 staged, 119 untracked. Topics of interest:
  - `Engine/Rendering/Vulkan/shaders/{gbuffer,deferred_lighting,sky_atmosphere}.*`
    were modified Aug 16-17 and (mostly) tracked in `git diff` but
    unstaged; the working GLSL is in the tree.
  - `Engine/Rendering/Vulkan/src/VulkanDeferredRenderer.cpp` is the file
    the deferred topic is about; staged at 757 lines currently.
- `task.md` `Notes (2026-08-16)` matches this scan's findings.

— end of scan —
