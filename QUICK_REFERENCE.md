# Luma Engine - Quick Reference & Key Insights

---

## What You Need to Know in 60 Seconds

**Luma Engine** is a modern, production-grade 3D game engine built from the ground up with:

- **Vulkan 1.3** for rendering (isolated behind an RHI interface)
- **C++20** with modern design patterns (ECS, immediate-mode UI)
- **Modular architecture** where each subsystem has a clear API and ownership
- **Test coverage** at every level (Catch2 + ctest)
- **"Always buildable" philosophy** - incremental milestones, never broken

**Current State**: Milestones 0-2 complete (foundation, platform, Vulkan rendering working). Milestone 3+ (renderer features, runtime systems, editor polish) in progress.

**Key Principle**: *No fake implementations—every subsystem delivers real functionality before moving to the next.*

---

## The 5-Level Architecture

### Level 1: Foundation (Luma::Core)
```
Types → Assert → Log → Config → Event → Layer → Application
        ↓
    Memory (Allocators + Leak Tracking)
```
**Purpose**: Everything sits on this. No dependencies except C++ stdlib.

### Level 2: Platform (Luma::Platform)
```
Window (abstract interface)
  ↓
GlfwWindow (implementation, GLFW hidden)
  ↓
Input Events → Application
```
**Purpose**: Abstracted windowing; backend swappable to Win32/SDL later.

### Level 3: Rendering (Luma::RHI + VulkanRenderer)
```
Abstract Renderer interface
  ↓
VulkanRenderer (Vulkan backend, privately owned)
  ├─ Device/Swapchain/Memory
  ├─ Passes: Scene (PBR) | Sky | Grid | UI
  └─ Shaders (precompiled to SPIR-V)
```
**Purpose**: Render abstraction; Vulkan types never escape the backend.

### Level 4: Scene & Data (Luma::Scene + Serialization)
```
ECS Registry (EnTT)
  ├─ Entities (handles)
  └─ Components: Name, Transform, MeshRenderer, Camera, Light, Environment
  
+ Reflection System (TypeInfo, SerialTraits)
+ JSON save/load
```
**Purpose**: World state as pure data; systems query and process.

### Level 5: Tools (Editor, Project Browser, UI Framework)
```
Luma Slate (Immediate-mode UI)
  ├─ EditorScreen (viewport, outliner, inspector, console)
  ├─ ProjectBrowser (project launcher)
  └─ Gizmo (transform tools)
```
**Purpose**: Design and debug scenes; all use the same Slate UI framework.

---

## 10 Most Important Concepts

### 1. **Immediate-Mode UI (Luma Slate)**
You describe the UI every frame from scratch. No retained state in widget code.
```cpp
Context ctx;
ctx.BeginFrame(width, height, dt);
if (ctx.Button(rect, "Save")) { /* handle click */ }
ctx.Panel(rect, color);
ctx.EndFrame();  // Returns UIDrawData → Renderer::DrawUI()
```
**Why**: Rapid iteration, clean code, perfect for editors and tools.

### 2. **ECS (Entity Component System)**
Entities are just handles. Components are pure data. Systems iterate views.
```cpp
Entity e = scene.CreateEntity("Player");
scene.Registry().emplace<MeshRendererComponent>(e);
auto view = scene.Registry().view<Transform, MeshRenderer>();
for (auto entity : view) { /* render */ }
```
**Why**: Flexible, decoupled from rendering/physics/animation.

### 3. **The RHI Principle**
Vulkan is private. Everything talks to an abstract `Renderer` interface.
```
Scene → RHI (abstract) → VulkanRenderer (implementation)
                        └─ Vulkan types stay here
```
**Why**: Swappable backends (D3D12/Metal later) without touching engine.

### 4. **VFS (Virtual File System)**
Paths can be "rooted" to well-known directories or absolute.
```cpp
Path p1(Root::Engine, "Shaders/sky.frag");    // Virtual: "Engine:/Shaders/sky.frag"
Path p2(filesystem::path("C:/data/mesh.fbx")); // Absolute
VFS::Global().ReadText(p1);  // VFS translates to real path
```
**Why**: Project portability, asset management, single source of truth.

### 5. **Reflection-Driven Serialization**
Components registered with `TypeBuilder`; save/load works automatically.
```cpp
TypeBuilder<TransformComponent>::Begin("Transform")
    .Property<Vec3>("position", offsetof(..., position))
    .End();

// Now: SerialTraits<TransformComponent>::Save/Load() work
```
**Why**: Add a component, register it, and the editor UI + save/load are free.

### 6. **Layer Stack (Event & Update Ordering)**
Layers process updates bottom-to-top; events top-to-bottom.
```cpp
app.PushLayer(gameLayer);      // Runs first
app.PushOverlay(uiLayer);      // Runs second (always on top)

// Update: gameLayer → uiLayer
// Event: uiLayer → gameLayer (if not handled)
```
**Why**: UI intercepts input before game; game always knows where events come from.

### 7. **Config Layering** (coming Phase 1.8)
Multiple config files override each other in priority order.
```
Engine (built-in) → Project → Editor → User → Platform
```
**Why**: Different settings per project, per user, per machine.

### 8. **Four Build Configs**
- **Debug**: All checks, no optimization (dev)
- **Development**: Optimized, checks on, validation on (shipping candidate testing)
- **Release**: Optimized, checks off, validation off
- **Shipping**: Optimized, asserts compiled out, no dev features

**Why**: Fine-grained control over performance vs. debugging.

### 9. **Always-Buildable Milestones**
Each phase adds functionality without breaking the previous one.
- M0-M2: Foundation + platform + Vulkan rendering ✅
- M3+: Features (meshes, materials, UI, editor, physics, audio)

**Why**: Never get stuck in a broken state; continuous delivery.

### 10. **Hot Reload Ready**
Architecture designed for:
- Shader hot reload (compile new .spv, swap pipelines)
- Config hot reload (watch Engine.ini, reload on change)
- Asset hot reload (reimport on file change)
- DLL hot reload (swap gameplay code, deferred)

**Why**: Iterate fast without restarting the engine.

---

## File Structure Cheat Sheet

| Folder | What's Inside | Key Files |
|--------|---------------|-----------|
| `Engine/Core/` | Types, logging, config, events, layers, memory | Types.h, Log.h, Config.h, Application.h |
| `Engine/Platform/` | Window abstraction, input | Window.h (GlfwWindow in .cpp only) |
| `Engine/Math/` | 3D math (Vec3, Mat4) | Math.h |
| `Engine/Rendering/` | RHI, Vulkan backend, Grid, Mesh | Renderer.h, VulkanRenderer.h |
| `Engine/Scene/` | ECS, components | Scene.h, Components.h |
| `Engine/UI/Slate/` | Immediate-mode UI | Context.h, Theme.h |
| `Engine/VFS/` | Virtual filesystem | Path.h, VFS.h |
| `Engine/Project/` | Project system (.luma files) | Project.h |
| `Engine/Serialization/` | JSON, reflection | SerialValue.h, Reflection.h |
| `Editor/LumaEditor/` | In-engine editor | EditorScreen.h |
| `Editor/ProjectBrowser/` | Project launcher | (WIP) |
| `Runtime/Sandbox/` | Main loop, entry point | main.cpp |
| `Tests/` | Unit tests (Catch2) | Core/, Scene/, VFS/, etc. |
| `Config/` | Default configuration | Engine.ini |
| `Content/` | Editor assets (fonts, icons) | Fonts/, Icons/ |
| `Scripts/` | Build scripts (PowerShell) | Configure.ps1, Build.ps1, Test.ps1, Run.ps1 |
| `cmake/` | Build system helpers | LumaCompilerFlags.cmake, LumaDependencies.cmake |
| `build/` | Build output (generated) | bin/, lib/, shaders/ |
| `docs/` | Architecture documentation | Architecture/, superpowers/ |

---

## Quick Build Commands

```powershell
# Configure
./Scripts/Configure.ps1

# Build (Development config)
./Scripts/Build.ps1 -Config Development

# Run tests
./Scripts/Test.ps1 -Config Development

# Launch engine
./Scripts/Run.ps1 -Config Development

# Clean
./Scripts/Clean.ps1
```

Or directly with CMake:
```bash
cmake --preset msvc
cmake --build build --config Development
ctest --test-dir build -C Development --output-on-failure
./build/bin/Development/LumaEngine.exe
```

---

## Phase Timeline (High Level)

| Phase | Focus | Status | Key Deliverables |
|-------|-------|--------|-------------------|
| **0-2** | Foundation + Rendering | ✅ | Core, Platform, Vulkan, basic passes |
| **3** | Runtime | 🔲 | Scene save/load, physics, audio, animation |
| **4** | Slate Polish | 🔲 | Widgets, layout, accessibility |
| **5** | Editor | 🔲 | Outliner, Inspector, Content Browser, Console |
| **6** | Project Browser | 🔲 | Launcher, templates, recent projects |
| **7-8** | Tooling & Production | 🔲 | Material/Shader/Animation editors, profiler, packaging |

---

## Most Useful Debugging Techniques

### 1. Check the Log
```cpp
LUMA_LOG_INFO("MySystem", "Entity count: {}", scene.EntityCount());
LUMA_LOG_ERROR("Renderer", "Failed to load texture: {}", path);
```
Logs go to console + `Saved/Logs/Luma.log`.

### 2. Use Asserts
```cpp
LUMA_ASSERT(entity.IsValid());        // Fatal; breaks debugger
LUMA_CHECK(success);                   // Recoverable; always active
```
Asserts break into VS debugger on failure in Development builds.

### 3. Inspect Scene
```cpp
auto view = scene.Registry().view<NameComponent>();
for (auto entity : view) {
    auto& name = view.get<NameComponent>(entity);
    LUMA_LOG_INFO("Scene", "Entity: {}", name.name);
}
```

### 4. Query Components
```cpp
if (auto* transform = scene.Registry().try_get<TransformComponent>(entity)) {
    LUMA_LOG_INFO("Transform", "Position: ({}, {}, {})",
                  transform->position.x, transform->position.y, transform->position.z);
}
```

### 5. Test Serialization
```cpp
SerialValue sv = SerialValue::Object();
SerialTraits<MyComponent>::Save(myComponent, sv);
std::cout << Json::Stringify(sv) << "\n";  // Dump JSON
```

---

## Common Patterns

### Creating an Entity
```cpp
Entity player = scene.CreateEntity("Player");
scene.Registry().emplace<TransformComponent>(player, 
    Vec3{0, 1.7f, 0}, Vec3{0, 0, 0}, Vec3{1, 1, 1});
scene.Registry().emplace<MeshRendererComponent>(player,
    MeshPrimitive::Sphere, /* albedo, metallic, roughness */);
```

### Querying & Updating
```cpp
auto view = scene.Registry().view<TransformComponent, MeshRendererComponent>();
for (auto entity : view) {
    auto& transform = view.get<TransformComponent>(entity);
    auto& mesh = view.get<MeshRendererComponent>(entity);
    transform.position.y += 0.5f * dt;
}
```

### Drawing UI
```cpp
ctx.BeginFrame(displayWidth, displayHeight, dt);

// Panel + label
ctx.Panel(Rect{10, 10, 200, 40}, Color::RGB(50, 50, 50));
ctx.LabelIn(Rect{10, 10, 200, 40}, "My Label", Align::Center);

// Button (check for click)
if (ctx.Button(Rect{10, 60, 80, 30}, "Click Me")) {
    LUMA_LOG_INFO("UI", "Button clicked!");
}

// Image
ctx.Image(textureHandle, Rect{100, 60, 80, 80}, Color::RGB(255, 255, 255));

const UIDrawData& drawData = ctx.EndFrame();
renderer->DrawUI(drawData);
```

### Loading a Config
```cpp
Config cfg;
cfg.LoadFromString(R"(
[Application]
name = MyGame
width = 1920
height = 1080
)");

std::string appName = cfg.GetString("Application.name", "DefaultName");
u32 width = cfg.GetInt("Application.width", 1280);
```

---

## Gotchas & Tips

### ✅ Do This
- Use `Luma::*` types (u32, f32, etc.) for consistency
- Allocate through the engine's allocators (Alloc::Global())
- Log liberally; the log is your best friend
- Write tests for new subsystems
- Keep modules independent; use interfaces, not concrete types
- Mark events as `Handled` to stop propagation

### ❌ Don't Do This
- Include Vulkan types outside `Engine/Rendering/Vulkan/`
- Use GLFW directly (it's hidden in Platform)
- Call `new`/`delete`; use the allocators
- Ignore assertion failures (they mean something's wrong)
- Create circular module dependencies
- Hard-code colors/sizes; use Theme tokens

### 🔍 Where to Look
- **Error in renderer?** Check [Engine/Rendering/Vulkan/](d:\Luma Engine\Engine\Rendering\Vulkan)
- **Scene not loading?** Check [Engine/Serialization/](d:\Luma Engine\Engine\Serialization) + [Engine/VFS/](d:\Luma Engine\Engine\VFS)
- **UI not rendering?** Check [Engine/UI/Slate/](d:\Luma Engine\Engine\UI\Slate) + [Runtime/Sandbox/](d:\Luma Engine\Runtime\Sandbox)
- **Build fails?** Check [cmake/](d:\Luma Engine\cmake) + [CMakeLists.txt](d:\Luma Engine\CMakeLists.txt)
- **Test failing?** Look in [Tests/](d:\Luma Engine\Tests) for the relevant subsystem

---

## Resources

**Read in Order**:
1. [README.md](d:\Luma Engine\README.md) — 5-minute overview
2. [GOAL.md](d:\Luma Engine\GOAL.md) — Roadmap & task tracker
3. [ARCHITECTURE_ANALYSIS.md](d:\Luma Engine\ARCHITECTURE_ANALYSIS.md) — This comprehensive guide (you're reading related content)
4. [DEPENDENCY_GRAPH.md](d:\Luma Engine\DEPENDENCY_GRAPH.md) — Visual module relationships
5. [docs/Architecture/Foundation.md](d:\Luma Engine\docs\Architecture\Foundation.md) — M0-M1 deep dive

**For Specific Topics**:
- **Rendering**: Engine/Rendering/RHI/include/Luma/RHI/Renderer.h + Engine/Rendering/Vulkan/src/
- **ECS**: Engine/Scene/include/ + Tests/Scene/
- **UI**: Engine/UI/Slate/include/Luma/Slate/ + Editor/LumaEditor/ (usage)
- **Build**: cmake/ + Scripts/
- **Testing**: Tests/ + individual test files

---

## Key People & Ownership

> This section would normally list team members. In this case, the Luma Engine is being built with:
> - **Architecture**: Production-grade design (clear APIs, proper lifecycle, testing)
> - **Development Philosophy**: "Always buildable" incremental milestones
> - **Code Quality**: Zero tolerance for silent failures, comprehensive assertions/logging

**Every subsystem is expected to have**:
- ✅ Clear ownership (one module per feature)
- ✅ Defined public API (headers in include/)
- ✅ Proper lifecycle (Init/Shutdown)
- ✅ Error handling (no silent failures)
- ✅ Logging (trace-through debugging)
- ✅ Serialization (save/load support)
- ✅ Tests (critical paths covered)
- ✅ Documentation (in docs/Architecture/)

---

## The Grand Vision

Luma Engine aspires to be:
- **Vulkan-first, modern** (C++20, latest graphics APIs)
- **Production-ready** (not a demo; real tools, real testing)
- **Modular & extensible** (plugin/module system in Phase 8)
- **Editor-integrated** (in-engine tools, not separate)
- **Hot-reload enabled** (iterate fast)
- **Beautiful design** (unified typography, consistent theme)
- **Standalone competitor** to Unreal / Unity (long-term vision)

**Near-term** (Phases 3-5): Complete runtime systems, full editor, asset pipeline.  
**Medium-term** (Phases 6-7): Professional tools (Material/Shader/Animation editors), polished UX.  
**Long-term** (Phase 8+): Production packaging, optimization, profiling, advanced features.

---

## One-Sentence Summary

**Luma Engine is a modular, production-grade 3D game engine built on Vulkan with an ECS scene system, immediate-mode UI framework, and comprehensive testing—designed to be always-buildable and feature-complete by incremental milestones.**

---

**Last Updated**: August 13, 2026  
**Next Documentation Update**: When Phase 3 (Runtime) or Phase 4 (Slate) completes

Happy coding! 🎮
