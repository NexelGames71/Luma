# Luma Engine — Master Goal & Implementation Tracker

> The single source of truth for **what we are building, in what order, and how far
> along each piece is.** Any agent (or human) should be able to open this file, find
> the next unchecked task, read its acceptance criteria, and start building without
> re-deriving context.

**Vision:** evolve the existing, working Luma Engine into a complete, production-grade
3D game engine and editor — the full *create → edit → test → debug → package → ship*
workflow — with its own custom **Luma Slate** UI framework, capable of eventually
standing alongside Unreal and Unity, while keeping its own premium/technical/minimal
identity.

**Prime directive:** `PRESERVE → IMPROVE → EXTEND → REFACTOR (when justified) → ADD WHAT'S MISSING`.
We do **not** rebuild. One authoritative implementation per subsystem. Stay buildable
and runnable at every step.

---

## How to use this file

1. **Find the next task.** Work top-down: Phase 1 → 8. Within a phase, finish a
   sub-phase before moving on unless a dependency forces otherwise.
2. **Before starting a task**, read its **Deliverables** and **Definition of Done (DoD)**.
   Confirm the thing doesn't already exist (audit the code — see the current status
   markers, but the code is the real source of truth).
3. **While working**, keep the build green. Compile → fix → run → verify after each
   meaningful change. Never batch hundreds of untested changes.
4. **When a task is done**, flip its box to `[x]`, update the sub-phase status marker,
   and add a dated one-line note in that sub-phase's **Log**.
5. **Classify every change** as one of: `VISUAL · BUG FIX · REFACTOR · EXTENSION ·
   NEW SYSTEM · PERFORMANCE · ORGANIZATION`. Prefer the first ones over rewrites.

### Status legend
- `✅ DONE` — implemented, builds, verified working.
- `🟡 PARTIAL` — foundation or a subset exists; explicitly listed gaps remain.
- `🔲 TODO` — not started.
- `[x]` / `[ ]` — individual task done / not done.

### Definition of "production-grade" (applies to every subsystem)
Clear ownership · defined interface/API · proper lifecycle (init/shutdown) ·
error handling (no silent failures) · logging · serialization where relevant ·
tests for critical paths · editor integration where relevant · documented in
`docs/Architecture/`. **No fake implementations** (no pretend asset loading, fake
compilation, stub Vulkan resources) unless clearly marked as temporary scaffold.

---

## Architecture ground rules (do not violate)

- **Vulkan stays isolated** behind the `Luma::RHI` interface. Feature modules feed the
  renderer data through the RHI; they never call Vulkan directly.
- **Every feature is its own module/folder + public API** under `Engine/<Area>/<Module>`
  with `include/Luma/<Module>` + `src/`. `.cpp` grouped by subsystem, not a flat `src/`.
- **Luma Slate is reusable** — not hard-coded to the editor. Project Browser, editor
  panels, and future tools all consume the same Slate widgets, tokens, and Inter typography.
- **Inter** is the one primary UI font across the entire product; monospace only for
  console/code/logs.
- **The Project Browser and the Editor must feel like one product** — same design tokens.

---

## Current repository map (as audited)

```
Engine/Core/        ✅ types, assert, log, config(INI), events, layers, app loop, timestep
Engine/Math/        🟡 single Math.h (vec/mat/transform); no full math lib yet
Engine/Platform/    ✅ Window (GLFW backend hidden), Process, Cursor
Engine/Project/     🟡 Project load/save (.luma); needs full project format
Engine/Scene/       🟡 EnTT ECS + Name/Transform/MeshRenderer/Light/Environment
Engine/Rendering/RHI/     ✅ abstract Renderer interface (header)
Engine/Rendering/Vulkan/  ✅ instance→device→swapchain→memory→shader→texture; scene/sky/grid/UI passes
Engine/Rendering/Mesh/    🟡 primitive meshes + PBR material data
Engine/Rendering/Grid/    ✅ analytic infinite grid pass
Engine/UI/Slate/    🟡 Context, DrawList, Font, Image, DockSpace, Types (docking + theme)
Editor/LumaEditor/  🟡 EditorScreen (viewport/outliner/inspector/console), splash, menu bar
Editor/ProjectBrowser/ 🟡 project list + creation
Editor/Gizmo/       🟡 translate gizmo only
Runtime/Sandbox/    ✅ runtime host (window + renderer + loop)
Tests/              🟡 Core + Project tests (Catch2 via ctest)
Templates/          🟡 4 thumbnails (Empty/FP/TP/TopDown); no data-driven template packages
Config/, Content/, Scripts/, cmake/, docs/
```

> Legacy milestone mapping: **M0–M2 complete**, and much of **M3** (mesh/PBR/lights/
> shadows/sky) plus early **M4/M5/M6** (ECS, Slate docking, editor panels, project
> browser) already exist. This tracker supersedes the old M0–M7 roadmap.

---
---

# PHASE 1 — FOUNDATION  ·  🟡 PARTIAL

**Goal:** rock-solid engine substrate everything else stands on. Mostly built; the
gaps are a real math library, an explicit memory subsystem, and a serialization core.

**Depends on:** nothing.
**Definition of phase done:** an app can boot, log, read/write layered config,
serialize/deserialize arbitrary reflected data, allocate through tracked allocators,
and all of it is covered by tests and builds in all four configs.

### 1.1 Core  ·  ✅ DONE
Application, EngineLoop, LayerStack, Event/Events, Timestep, Types, Version, Assert.
- [x] Application + engine loop + layer stack
- [x] Event system + typed events
- [x] Assertions (Debug/Development) and versioning
- [x] Strong type aliases (`Luma/Core/Types.h`)
- **DoD:** app boots to a loop, layers receive events/updates, asserts fire in Debug. ✅
- **Log:** _pre-existing (M0–M1)._

### 1.2 Math  ·  🟡 PARTIAL
Currently one `Math.h` (Vec2/3/4, Mat4, transform helpers). Needs to become a real lib.
- [x] Vec2/3/4, Mat4, Translate/Rotate/Scale, Radians
- [ ] Quaternions (rotation, slerp) and quaternion↔euler↔matrix conversions
- [ ] Mat3, normal matrices, transpose/inverse, decompose (T/R/S)
- [ ] Frustum, Plane, AABB, Ray, Sphere primitives + intersection tests
- [ ] Common utils: lerp, clamp, smoothstep, remap, min/max, random
- [ ] Split into headers per concern; add `LumaMath` unit tests
- **DoD:** transforms, camera math, culling, and picking all pull from one math lib;
  tests cover matrix/quaternion identities and intersection edge cases.
- **Log:** _(pending)_

### 1.3 Memory  ·  🔲 TODO
No explicit allocator/tracking layer yet.
- [ ] Allocation macros/wrappers with tag + call-site tracking (Debug/Development)
- [ ] Arena/linear allocator (per-frame scratch) and pool allocator
- [ ] Leak reporting on shutdown; peak/current usage counters
- [ ] Hook renderer + assets to report GPU/CPU memory to the profiler (Phase 8 tie-in)
- **DoD:** per-frame scratch allocations cost ~0 and reset each frame; leaks surface
  at shutdown with tag + site.
- **Log:** _(pending)_

### 1.4 Logging  ·  ✅ DONE
`Luma/Core/Log.h` + console sink.
- [x] Severity levels (INFO/DEBUG/TRACE/WARNING/ERROR/FATAL)
- [x] Console output; categories/loggers
- [ ] File sink (rotating) writing to `Saved/Logs/`
- [ ] Structured fields (subsystem, resource) for the editor Console/Problems view
- **DoD:** logs reach console **and** file; editor Console can filter by severity/category.
- **Log:** _core done; file sink + structured fields pending._

### 1.5 File System  ·  🟡 PARTIAL
Platform file handling exists implicitly (Config/Project read files). Needs a real VFS.
- [ ] Path abstraction (project-relative, engine-relative, absolute) + normalization
- [ ] Read/write/exists/iterate helpers; directory watch (for hot reload)
- [ ] Async file I/O on the asset thread (Phase 3/5 tie-in)
- [ ] Well-known roots: `Engine/`, `<Project>/`, `Saved/`, `Intermediate/`
- **DoD:** all subsystems resolve paths through the VFS; a file-watch callback fires
  on change (drives hot reload).
- **Log:** _(pending)_

### 1.6 Platform  ·  ✅ DONE
GLFW window backend (hidden), Process, Cursor.
- [x] Window interface with GLFW backend behind it
- [x] Process spawn/util; cursor control
- [ ] Monitor/DPI enumeration surfaced to Slate (Phase 4 DPI tie-in)
- **DoD:** window create/resize/close, DPI scale query, subprocess launch all work. ✅ (DPI query pending)
- **Log:** _pre-existing (M1)._

### 1.7 Build System  ·  ✅ DONE
CMake ≥3.28, Ninja Multi-Config, 4 configs, presets, PowerShell scripts, Vulkan detect.
- [x] Debug / Development / Release / Shipping configs
- [x] `CMakePresets.json`, Configure/Build/Test/Run/Clean scripts
- [x] Vulkan SDK detection (`D:/VulkanSDK/...`), GLFW+Catch2+EnTT via FetchContent
- [ ] Shader compile step wired into the build (glslc → SPIR-V, cached) — see 2.3
- **DoD:** clean checkout configures + builds + tests + runs in all four configs.
- **Log:** _pre-existing (M0)._

### 1.8 Configuration  ·  🟡 PARTIAL
`Luma/Core/Config.h` + `Config/Engine.ini`.
- [x] INI-style engine config load
- [ ] Config **layering**: Engine → Project → Editor → User → Platform (override order)
- [ ] Write-back / save; typed getters with defaults; versioned config
- **DoD:** a project can override engine defaults; editor prefs persist to `Saved/`.
- **Log:** _engine config done; layering + save pending._

### 1.9 Serialization  ·  🔲 TODO  *(foundational — unblocks Scene/Asset save)*
No general serializer yet (only INI). This is the biggest Phase 1 gap.
- [ ] Reflection/property registry (fields, types, metadata, categories, ranges,
      defaults, editor-visible, read-only) — shared with the Inspector (5.3) and
      the property system (3.2)
- [ ] Binary + text (JSON/YAML-ish) archives; versioned with upgraders
- [ ] Serialize primitives, math types, containers, and reflected structs
- [ ] Object references / IDs (foundation for asset GUIDs, 5.6)
- **DoD:** a component struct round-trips text↔binary; adding a field with a default
  loads old files without breaking (versioning proven by test).
- **Log:** _(pending)_

---
---

# PHASE 2 — RENDERER  ·  🟡 PARTIAL (advanced)

**Goal:** a modern Vulkan renderer behind the RHI that draws PBR meshes, materials,
textures, lights, and shadows, and is structured so post-processing and advanced
features slot in without a rewrite. Substantially built already.

**Depends on:** Phase 1 (Core, Math, Platform).
**Definition of phase done:** meshes with material instances + textures render under
multiple lights with shadows; camera system supports editor + runtime cameras; resize/
vsync/validation clean; a post-process hook point exists.

### 2.1 Vulkan Foundation  ·  ✅ DONE
Instance/validation → device select → queues → surface → swapchain → command pools/
buffers → sync (semaphores/fences) → memory allocator → per-frame management → resize.
- [x] Instance + validation layers (clean), physical/logical device, queues
- [x] Swapchain create/recreate on resize; frames-in-flight
- [x] Command pools/buffers, semaphores, fences
- [x] GPU memory allocator (`VulkanMemory`)
- [x] Dynamic rendering / render pass path; MSAA (4x) on the scene target
- **DoD:** clear+present validation-clean, survives resize/minimize, clean shutdown. ✅
- **Log:** _pre-existing (M2 + later)._

### 2.2 GPU Resources  ·  ✅ DONE
Buffers, images, textures, staging/upload.
- [x] Vertex/index/uniform buffers; staged uploads
- [x] `VulkanTexture` (images, views, samplers)
- [ ] Bindless / descriptor-indexing path (future scale) — architecture note only
- **DoD:** create/update/destroy buffers & textures with correct lifetime; no leaks.
- **Log:** _pre-existing._

### 2.3 Shaders  ·  🟡 PARTIAL
`VulkanShader`; GLSL sources under `Vulkan/shaders/` (scene.vert/frag, grid, sky…).
- [x] Load SPIR-V, create shader modules, build pipelines
- [ ] **Build-time shader compilation** (glslc → SPIR-V) wired into CMake, cached
- [ ] Shader reflection (descriptor layouts auto-derived)
- [ ] Compile errors surfaced to the editor Console/Problems (5.5 tie-in)
- [ ] Hot-reload shaders on file change (1.5 file-watch tie-in)
- **DoD:** editing a `.frag` and saving recompiles + reloads live with errors reported
  in-editor; no manual SPIR-V steps.
- **Log:** _pipeline creation done; compile pipeline + reflection + hot-reload pending._

### 2.4 Materials  ·  🟡 PARTIAL
PBR material params on `MeshRendererComponent` (albedo/metallic/roughness) + sky IBL.
- [x] PBR shading model (metallic-roughness) with image-based lighting from sky
- [ ] **Material** / **Material Instance** / **Shader** / **Texture** / **Parameter**
      object model (data-driven, serialized) — decouple from the component
- [ ] Texture-backed params (albedo/normal/metallic-roughness/AO/emissive maps)
- [ ] Shader permutations by feature set
- **DoD:** materials are assets referenced by meshes; a material instance overrides
  params; textures bind through the material, not hard-coded component fields.
- **Log:** _shading model done; material object model pending (needs 5.6 asset system)._

### 2.5 Meshes  ·  🟡 PARTIAL
`Engine/Rendering/Mesh` — built-in primitives (cube/sphere/plane…) + PBR draw.
- [x] Primitive mesh generation + GPU buffers + draw path
- [ ] Imported static meshes (from glTF/OBJ/FBX via asset pipeline 5.6)
- [ ] Skeletal mesh support (skinning) — architecture for Animation (3.7)
- [ ] Instancing + LOD hooks; mesh streaming architecture
- **DoD:** an imported mesh asset renders identically to a primitive; instanced draw
  path exists for repeated meshes.
- **Log:** _primitives done; imported/skeletal/instancing pending._

### 2.6 Textures  ·  ✅ DONE (core)
- [x] Upload, sampling, mipmaps, sRGB/linear handling in shaders
- [ ] Compressed formats (BCn) + import-time processing (5.5 tie-in)
- **DoD:** textures render correct color space; large textures don't stall the frame.
- **Log:** _core done; compression pending._

### 2.7 Camera  ·  🟡 PARTIAL
Editor fly/orbit camera lives in `VulkanSceneView`; no reusable Camera system yet.
- [x] Editor camera (perspective, orbit/fly, focus) driving the scene view
- [ ] `CameraComponent` (perspective/ortho, FOV, near/far) as a real component
- [ ] Multiple cameras; runtime (play) camera vs editor camera separation
- [ ] Ortho + top/front/side views; view switching (5.1 viewport tie-in)
- **DoD:** cameras are components in the scene; play mode uses the game camera, edit
  mode uses the editor camera; ortho/persp switch works.
- **Log:** _editor camera done; camera component/system pending._

### 2.8 Lighting  ·  ✅ DONE (core)
Multi-light PBR + analytic sky; directional sun shadow mapping (PCF).
- [x] Directional / point / spot lights (multi-light)
- [x] Analytic Preetham sky via `EnvironmentComponent`; sky IBL
- [x] Directional (sun) shadow map with PCF
- [ ] Point/spot shadows; cascaded shadow maps for large scenes
- [ ] Architecture stubs for future GI (documented, not implemented)
- **DoD:** many lights render correctly; sun casts soft shadows; adding point-light
  shadows doesn't require a renderer rewrite.
- **Log:** _lights + sun shadow done; point/spot + cascades pending._

### 2.9 Post-Processing (architecture)  ·  🔲 TODO
- [ ] HDR render target + tone mapping (ACES/Reinhard)
- [ ] Post chain hook point (bloom, color grading, SSAO, AA/TAA) — build the *pipeline*,
      add effects incrementally
- **DoD:** a tone-mapping pass runs on an HDR target; new effects register into the
  chain without touching core render loop.
- **Log:** _(pending)_

---
---

# PHASE 3 — RUNTIME  ·  🟡 PARTIAL

**Goal:** the gameplay substrate — scenes, entities/components, input, physics, audio,
animation — all serializable and editor-integrated, with runtime state isolated from
authoring state.

**Depends on:** Phase 1 (serialization 1.9), Phase 2 (renderer for mesh/camera/light).
**Definition of phase done:** a scene of entities with components saves/loads, plays
(physics + input + audio + animation tick) without mutating the authoring scene, and
stops back to the authored state.

### 3.1 Scene  ·  🟡 PARTIAL
`Engine/Scene` — EnTT-backed world + registry; editor drives outliner/inspector/gizmo from it.
- [x] World/registry, entity create/destroy, iterate
- [x] Editor reads/writes the scene (outliner, inspector, gizmo, viewport)
- [ ] Transform **hierarchy** (parent/child, world vs local transforms)
- [ ] Scene **serialization** (save/load `.luma` scene files) — needs 1.9
- [ ] Scene duplication, instancing, **prefabs**
- [ ] Runtime-state isolation (play copies the scene; stop restores)
- **DoD:** hierarchy reparenting works in the outliner; a scene round-trips to disk;
  entering/exiting play doesn't corrupt the authored scene.
- **Log:** _ECS + editor binding done; hierarchy/serialization/prefabs/play-isolation pending._

### 3.2 Entities & Components  ·  🟡 PARTIAL
Components: Name, Transform, MeshRenderer, Light, Environment.
- [x] Core components above, integrated with rendering
- [ ] `CameraComponent` (2.7), `Collider`/`RigidBody` (3.5), `AudioSource` (3.6),
      `Animator` (3.7), `ScriptComponent`, `Navigation`
- [ ] Reflected properties for every component (drives Inspector 5.3 + serialization 1.9)
- **DoD:** each component is reflected → auto-serializes and auto-appears in the Inspector.
- **Log:** _core components done; property reflection + gameplay components pending._

### 3.3 Input  ·  🟡 PARTIAL
Event-based input exists (`Event.h`/`Events.h`, Slate consumes it). No action mapping.
- [x] Raw keyboard/mouse events through the platform/event system
- [ ] Input **abstraction**: Actions, Axes, Bindings, Contexts, Modifiers
- [ ] Gamepad/controller support
- [ ] Templates consume the input system (not ad-hoc key checks)
- **DoD:** a rebindable "Jump"/"MoveForward" action fires from key or pad; edit vs play
  input contexts don't collide.
- **Log:** _raw events done; action-mapping layer pending._

### 3.4 Physics (abstraction + backend)  ·  🔲 TODO
- [ ] Physics abstraction (don't couple gameplay to one library)
- [ ] Integrate a backend (e.g. Jolt) behind the abstraction
- [ ] Rigid/static/dynamic bodies, colliders, character controller
- [ ] Raycasts, overlap queries, triggers, constraints
- [ ] Integrate with scene/entities; debug draw in viewport
- **DoD:** a dynamic body falls onto static geometry; raycast hits report; character
  controller moves; all visible via physics debug draw.
- **Log:** _(pending)_

### 3.5 Audio  ·  🔲 TODO
- [ ] Audio subsystem + backend; 2D and 3D positional audio
- [ ] Audio assets, sources, buses, volume, spatialization, doppler, reverb (arch)
- [ ] `AudioSource` / `AudioListener` components
- **DoD:** a positioned sound attenuates/pans with the listener; a music bus controls volume.
- **Log:** _(pending)_

### 3.6 Animation  ·  🔲 TODO
- [ ] Skeletal animation (skinning path in the renderer 2.5)
- [ ] Animation clips, graphs, state machines, blend trees
- [ ] Retargeting + animation events architecture
- [ ] `Animator` component
- **DoD:** a skeletal mesh plays a clip; a 2-state machine blends on a parameter.
- **Log:** _(pending)_

---
---

# PHASE 4 — SLATE (Custom UI Framework)  ·  🟡 PARTIAL

**Goal:** elevate Luma Slate from functional to a professional, reusable design system
— sharp Inter typography, DPI-correct, token-driven widgets, docking, theme — used by
the Project Browser, Editor, and every future tool.

**Depends on:** Phase 1 (platform/DPI), Phase 2 (renders through a Vulkan UI pass — exists).
**Definition of phase done:** a documented widget set + design tokens render crisply at
any DPI in a consistent theme; all editor/project-browser UI is built from them.

### 4.1 Slate Core  ·  🟡 PARTIAL
`Context`, `DrawList`, `Types`, immediate-mode-style pipeline; Vulkan UI pass exists.
- [x] Context + draw list + Vulkan UI pass rendering
- [x] Event/input routing into widgets
- [ ] Retained layout/interaction state hardening; perf (don't rebuild UI needlessly)
- **DoD:** UI renders through the RHI, handles input, and stays responsive with many widgets.
- **Log:** _core pipeline done; perf/retained-state hardening pending._

### 4.2 Rendering  ·  🟡 PARTIAL
- [x] Draw list → Vulkan UI pass (batched quads/text/images)
- [ ] Anti-aliased primitives, rounded rects, clipping stack polish
- **DoD:** crisp edges, correct clipping in nested/scrolled panels.
- **Log:** _(partial)_

### 4.3 Text  ·  🟡 PARTIAL
`Font.h`/`Font.cpp`, `Image` glyph atlas.
- [x] Font atlas + text rendering in the draw list
- [ ] Sub-pixel/hinted crisp rendering; per-DPI atlas regeneration
- [ ] Text layout: wrapping, alignment, ellipsis, mixed weights
- **DoD:** text is sharp at 100–200% DPI; long labels ellipsize; multi-weight runs lay out.
- **Log:** _(partial)_

### 4.4 Inter (typography)  ·  🟡 PARTIAL  *(active: `Inter/` just added)*
- [x] Inter font files present in repo (`Inter/`)
- [ ] Load Inter Regular/Medium/SemiBold/Bold into the Slate font system
- [ ] Replace the current UI font everywhere (editor + project browser) with Inter
- [ ] Monospace font wired for Console/logs/code
- [ ] Typography scale/tokens (sizes, weights, line-height) in the theme (4.7)
- **DoD:** every piece of UI text renders in the correct Inter weight; console uses mono.
- **Log:** _Inter files added; loading + rollout pending._

### 4.5 Widgets  ·  🟡 PARTIAL
Have: panels, tabs, dock headers, colored X/Y/Z drag fields, menu bar, cursors.
Target reusable set: `LumaButton, LumaIconButton, LumaInput, LumaDropdown, LumaCheckbox,
LumaToggle, LumaSlider, LumaTab, LumaPanel, LumaTree, LumaProperty, LumaSearchBox,
LumaMenu, LumaContextMenu, LumaTooltip, LumaDialog, LumaProgressBar`.
- [x] Panels, tabs, drag-number fields, menu bar
- [ ] Fill out the full widget catalog above with consistent states
      (hover/active/focus/disabled/selected)
- [ ] Tree, property row, search box, dialog, tooltip, context menu, progress bar
- **DoD:** every listed widget exists, is token-styled, and has all interaction states.
- **Log:** _(partial)_

### 4.6 Layout  ·  🟡 PARTIAL
- [x] Panel layout + splitters
- [ ] Flex/stack/grid layout primitives; spacing/padding tokens; alignment
- **DoD:** panels compose via layout primitives; spacing comes from tokens, not literals.
- **Log:** _(partial)_

### 4.7 Docking  ·  ✅ DONE (core)
`DockSpace` — dockable panels, tabs, drag-to-dock, dock compass, edge pods.
- [x] Dock/undock, tabbing, drag-to-dock with compass indicator, edge dock pods
- [ ] Float windows; save/restore layouts to `Saved/`
- **DoD:** user arranges panels and the layout persists across restarts.
- **Log:** _docking core done; float + layout persistence pending._

### 4.8 Theme (design tokens)  ·  🟡 PARTIAL
Rich theme + SemiBold UI font already in place.
- [x] Base dark theme, accent, panel surfaces
- [ ] Centralized **design tokens**: colors, typography, spacing, sizing, borders,
      radius, shadows, elevation, icons, animation, interaction states — consumed by
      every widget
- [ ] Light/high-contrast variants (accessibility 4.10 tie-in)
- **DoD:** changing a token restyles the whole product consistently; no hard-coded colors.
- **Log:** _theme exists; token system consolidation pending._

### 4.9 Icons  ·  🔲 TODO
- [ ] Consistent crisp icon set (entity types, toolbar, file types) + atlas
- [ ] DPI-correct icon rendering
- **DoD:** all toolbars/outliner/content-browser use one coherent icon set.
- **Log:** _(pending)_

### 4.10 Accessibility (foundation)  ·  🔲 TODO
- [ ] Keyboard navigation + focus management; tooltips
- [ ] Font scaling; color-independent status indicators; high-contrast theme
- **DoD:** the editor is navigable by keyboard; status isn't conveyed by color alone.
- **Log:** _(pending)_

---
---

# PHASE 5 — EDITOR  ·  🟡 PARTIAL

**Goal:** a professional, dockable editor workspace built entirely on Slate — viewport,
outliner, inspector, content browser, console, profiler, transform tools, play/stop.

**Depends on:** Phases 2–4. **Definition of phase done:** author a scene end-to-end
(place/transform entities, edit properties, browse content, read console, play/stop).

### 5.1 Viewport  ·  🟡 PARTIAL
Scene view renders through Vulkan; editor camera; grid; MSAA.
- [x] 3D viewport with editor camera, grid, sky, lit meshes
- [ ] View modes (persp/ortho, top/front/side), focus-selected, snapping, gizmo overlay
- [ ] Viewport toolbar (camera speed, view mode, gizmo mode, snap settings)
- **DoD:** artists frame/focus, switch views, and snap while placing objects.
- **Log:** _rendering + camera done; view modes/toolbar/snapping pending._

### 5.2 World Outliner  ·  🟡 PARTIAL
Driven from the ECS.
- [x] Lists scene entities; selection drives inspector/gizmo
- [ ] Search, multi-select, rename, delete, duplicate, parent/unparent (needs 3.1 hierarchy),
      visibility, lock, context menu, entity type icons
- **DoD:** full hierarchy management from the outliner with a right-click menu.
- **Log:** _list + selection done; the rest pending._

### 5.3 Inspector  ·  🟡 PARTIAL
Shows selected entity's components; colored X/Y/Z transform fields.
- [x] Displays components of the selected entity; editable transform
- [ ] **Reflection-driven** property UI (auto-build from 1.9/3.2 metadata: ranges,
      categories, read-only, tooltips) — no hard-coded panels
- [ ] Add/remove component UI; multi-edit
- **DoD:** adding a reflected field to any component makes it appear+edit in the Inspector
  with no Inspector code changes.
- **Log:** _transform editing done; reflection-driven UI pending (needs 1.9)._

### 5.4 Content Browser  ·  🔲 TODO
- [ ] Folder tree + asset grid/list; thumbnails; search/filter/sort
- [ ] Drag-and-drop into scene/inspector; import; rename/delete/duplicate; asset preview
- [ ] Backed by the Asset Registry (5.6)
- **DoD:** browse project assets, drag a mesh into the viewport, import a new file.
- **Log:** _(pending)_

### 5.5 Console  ·  🟡 PARTIAL
Editor references a console; core Log exists.
- [x] Console surface in the editor
- [ ] Filter by severity/category; search; clickable errors (jump to source/shader)
- [ ] **Problems** view for compile/shader/import errors (2.3/5.6 tie-in)
- **DoD:** logs stream in with filtering; a shader compile error is clickable.
- **Log:** _(partial)_

### 5.6 Asset Tools — Pipeline + Registry  ·  🔲 TODO  *(large, foundational)*
- [ ] **Asset Registry / database**: stable GUID per asset; type, source path, processed
      path, dependencies, import settings, timestamp, version
- [ ] **Import pipeline**: FBX/glTF/GLB/OBJ, PNG/JPG/TGA/HDR, WAV/MP3/OGG, fonts, shaders
      → processed engine-native formats (`Source → Importer → Processed → Registry → Runtime`)
- [ ] Async import on the asset thread (no editor stalls)
- [ ] Hot reload of changed assets
- **DoD:** dropping a glTF into the project imports it, registers a GUID, and it renders
  from the Content Browser; re-importing updates it live.
- **Log:** _(pending)_

### 5.7 Transform Tools & Gizmos  ·  🟡 PARTIAL
`Editor/Gizmo` — translate gizmo only.
- [x] Translate gizmo
- [ ] Rotate + scale gizmos; world/local space; axis constraints
- [ ] Grid/rotation/scale snapping
- **DoD:** move/rotate/scale with axis locks and snapping in world or local space.
- **Log:** _translate done; rotate/scale/snapping pending._

### 5.8 Play / Simulation System  ·  🔲 TODO
- [ ] Edit / Play / Simulate modes; Play/Pause/Stop/Step controls
- [ ] Runtime state isolated from authoring scene (3.1 tie-in)
- **DoD:** press Play → game ticks (physics/input/anim); Stop → authored scene restored.
- **Log:** _(pending)_

### 5.9 Editor Workspace / Layouts  ·  🟡 PARTIAL
Dockable panels + branded menu bar exist.
- [x] Dockable panels, branded menu bar (logo/wordmark), project pill
- [ ] Save/restore named layouts; reset-to-default; per-project layout in `Saved/`
- **DoD:** users rearrange docks and restore layouts across sessions.
- **Log:** _docking + menu bar done; layout persistence pending._

---
---

# PHASE 6 — PROJECT BROWSER  ·  🟡 PARTIAL

**Goal:** a first-class, polished launcher — manage, create, and open projects — sharing
the exact Slate design system with the editor so they feel like one product.

**Depends on:** Phase 4 (Slate/Inter/tokens), Phase 1 (config/serialization for `.luma`).
**Definition of phase done:** create a project from a template, it opens in the editor,
and it reappears in a styled recent-projects list.

### 6.1 Projects (list/manage)  ·  🟡 PARTIAL
`Editor/ProjectBrowser` — list + creation exist.
- [x] Project list + creation flow
- [ ] Rich entries: name, path, thumbnail, last-opened, engine version
- [ ] Search, sort, remove-from-list, reveal in Explorer, project info
- **DoD:** recent projects show thumbnails/metadata and open on click.
- **Log:** _(partial)_

### 6.2 Templates (data-driven)  ·  🟡 PARTIAL
Have 4 thumbnails (`Templates/*.png`); no data-driven template packages yet.
- [x] Template thumbnails (Empty/First Person/Third Person/Top Down)
- [ ] **Template = full init package** (scene, camera, input bindings, controller,
      config), not just a thumbnail — data-driven so new templates need no browser rewrite
- [ ] Empty / FP / TP / TopDown packages; architecture for Vehicle/2D/VR/etc.
- **DoD:** picking "First Person" scaffolds a runnable FP project (controller+camera+input).
- **Log:** _thumbnails done; template packages pending._

### 6.3 Creation Workflow  ·  🟡 PARTIAL
- [x] Basic create flow
- [ ] Professional New-Project UI: name, location, template, render config, target
      platform, project settings — Slate/Inter styled
- **DoD:** the New-Project screen matches editor styling and collects real settings.
- **Log:** _(partial)_

### 6.4 Opening & Recent Projects  ·  🟡 PARTIAL
- [x] Open a project into the editor
- [ ] Recent-projects persistence in user config (1.8); handle moved/missing projects
- **DoD:** recent list persists and self-heals when a project path is gone.
- **Log:** _(partial)_

### 6.5 Project Format & Configuration  ·  🟡 PARTIAL
`Engine/Project` reads/writes `.luma`.
- [x] `.luma` project load/save
- [ ] Full project layout: `Assets/ Config/ Scenes/ Scripts/ Shaders/ Materials/
      Audio/ Source/ Plugins/ Build/ Intermediate/ Saved/` (separate source vs generated)
- [ ] Per-project configuration (1.8 layering)
- **DoD:** creating a project lays out the full folder structure; generated files never
  mix with authored content.
- **Log:** _basic .luma done; full layout + config pending._

---
---

# PHASE 7 — TOOLING  ·  🔲 TODO

**Goal:** dedicated authoring tools built on Slate. Each is its own editor window/module.

**Depends on:** Phases 2 (materials/shaders), 3 (animation), 5 (asset system/content browser).
**Definition of phase done:** each tool creates/edits its asset type and the result is
usable in a scene.

### 7.1 Material Editor  ·  🔲 TODO
- [ ] Visual material graph (or param editor first) → produces material assets (2.4)
- [ ] Live preview sphere; texture/param inputs; save as asset
- **DoD:** author a material visually; assign it to a mesh; it renders.
- **Log:** _(pending)_

### 7.2 Shader Editor  ·  🔲 TODO
- [ ] Edit shader sources with compile (2.3) + errors in Problems; live reload
- [ ] Reflection-driven parameter exposure
- **DoD:** edit a shader in-editor, see errors inline, apply live.
- **Log:** _(pending)_

### 7.3 Animation Editor  ·  🔲 TODO
- [ ] Timeline/clip editing; state machine + blend tree graph (3.7)
- [ ] Preview on a skeletal mesh; animation events
- **DoD:** build a blend tree and preview transitions on a character.
- **Log:** _(pending)_

### 7.4 Terrain Tools  ·  🔲 TODO
- [ ] Heightmap terrain; layers/materials; sculpt + paint; foliage; LOD; collision
- [ ] Viewport + Inspector integration
- **DoD:** sculpt terrain, paint a material layer, and walk on it (collision).
- **Log:** _(pending)_

### 7.5 VFX Tools  ·  🔲 TODO
- [ ] Particle system component + editor; emitters, modules, GPU sim (arch)
- **DoD:** author a particle effect and place it in a scene.
- **Log:** _(pending)_

---
---

# PHASE 8 — PRODUCTION  ·  🔲 TODO

**Goal:** ship real games — package, optimize, profile, report crashes, extend via
plugins, and document the whole platform.

**Depends on:** all prior phases. **Definition of phase done:** a project packages to a
standalone runnable build; profiler + crash reporting work; plugins load; docs exist.

### 8.1 Packaging / Build Tooling  ·  🔲 TODO
- [ ] Cook/package a project into a standalone Shipping build (assets + runtime)
- [ ] Editor/Development/Debug/Shipping separation; incremental build; dependency tracking
- [ ] Build progress/logs/cancellation surfaced in-editor
- **DoD:** "Package" produces a runnable game folder that launches without the editor.
- **Log:** _(pending)_

### 8.2 Optimization  ·  🔲 TODO
- [ ] Evidence-driven: profile first, optimize measured bottlenecks
- [ ] Threading model (main/UI, render, asset, workers) with clear ownership
- [ ] Avoid per-frame allocations / UI rebuilds / render-thread stalls
- **DoD:** frame time stable; imports/shader compiles don't block the UI thread.
- **Log:** _(pending)_

### 8.3 Profiling  ·  🔲 TODO
- [ ] Profiler UI in Slate: CPU/GPU/frame/render/physics/audio/scripts/memory/asset-load
- [ ] Per-frame timing capture; memory counters (1.3 tie-in)
- **DoD:** the profiler shows CPU+GPU frame breakdown live.
- **Log:** _(pending)_

### 8.4 Crash Reporting  ·  🔲 TODO
- [ ] Crash handler → minidump + log to `Saved/Crashes/`; symbolication guidance
- **DoD:** a crash writes a dump + log and the editor can point the user to it.
- **Log:** _(pending)_

### 8.5 Plugin / Module System  ·  🔲 TODO
- [ ] Module architecture so `LumaPhysics/Audio/AI/Terrain/Networking/VR/Tools` build
      and load independently
- [ ] Plugin discovery/enable per project
- **DoD:** a plugin adds a component + editor panel without modifying the core.
- **Log:** _(pending)_

### 8.6 Documentation  ·  🟡 PARTIAL
`docs/Architecture/Foundation.md` + superpowers specs exist.
- [x] Foundation architecture doc; design spec + plan
- [ ] Per-subsystem docs (purpose, API, ownership, lifecycle, dependencies, extension
      points) under `docs/Architecture/{Rendering,Slate,Editor,Projects,Assets,Runtime,
      Scripting,Build,Development}/`
- **DoD:** each shipped subsystem has a doc page kept current with the code.
- **Log:** _foundation doc done; per-subsystem docs pending._

---
---

# Cross-cutting: Scripting (spans Phases 3/5/7)  ·  🔲 TODO

Not tied to one phase — the gameplay scripting layer that binds entities, components,
input, physics, audio, animation, AI, scenes, and serialization.
- [ ] Choose runtime (modular): native modules first, later a scripting language
- [ ] `ScriptComponent` + lifecycle hooks (init/update/events)
- [ ] Editor integration (attach scripts, edit exposed properties)
- **DoD:** a script drives an entity's behavior in play mode using input + physics.

# Cross-cutting: AI & Navigation (spans Phase 3/7)  ·  🔲 TODO
- [ ] Navigation mesh generation + pathfinding; agents; behavior systems; perception
- [ ] AI controllers not coupled to a specific game template
- **DoD:** an agent paths around obstacles on a generated navmesh.

# Cross-cutting: Testing  ·  🟡 PARTIAL
Catch2 via ctest; Core + Project tests exist.
- [x] Core + Project tests
- [ ] Tests for Math, Serialization, Asset Registry, Scene, Renderer resource mgmt,
      Input, Physics abstraction, Slate, Project system, Build system
- **DoD:** critical systems have automated tests that run in CI/ctest.

---

## Suggested near-term order (fills the highest-leverage gaps first)

1. **1.9 Serialization + 3.2 property reflection** — unblocks scene save/load, the
   reflection-driven Inspector, and asset GUIDs. Highest leverage.
2. **4.4 Inter rollout + 4.8 design tokens** — the current active visual objective;
   makes the whole product look professional immediately (Project Browser + Editor).
3. **3.1 Scene hierarchy + serialization** and **5.8 Play/Stop with state isolation**.
4. **5.6 Asset Registry + import pipeline** → **5.4 Content Browser**.
5. **2.3 shader compile + hot reload**, **2.7 Camera component**, **5.7 rotate/scale gizmos**.
6. Then Physics (3.4) → Audio (3.5) → Animation (3.6), and on into Tooling/Production.

> Keep this ordering flexible — but always prefer finishing a foundational gap (Phase 1–3)
> before piling features onto an incomplete base.
