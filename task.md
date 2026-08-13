Other tasks waiting for you to pick one (the doc's near-term order; checkboxes flipped where the work still applies):
    
    Phase 1 — Foundation gaps
    - 1.3 Memory — allocators, leak reporting, scratch arenas ✅ (landed 2026-08-12)
    - 1.5 File System / VFS
      - 1.5a Path/well-known roots + read/write/exists + poll-based watcher ✅ (landed 2026-08-13)
      - 1.5b Async file I/O on the asset thread (Phase 3/5 tie-in)
      - 1.5c Migrate Config.cpp / Project.cpp to read through VFS (cleanup, not blocking)
    - 1.8 Config layering — Engine/Project/Editor/User/Platform override order
    - 1.9 Serialization (open half) — binary archive, document versioning, object references/IDs
    
    Phase 2 — Renderer
    - 2.3 Shader compile + hot reload — wire glslc into CMake, reflect, report errors in Console
    - 2.4 Material object model — decouple from component, texture-backed params, permutations
    - 2.5 Imported meshes + skeletal path + instancing/LOD
    - 2.7 Camera wiring — renderer + editor consume primary CameraComponent; ortho/top/front/side
    - 2.8 Point/spot shadows + cascaded sun shadows
    - 2.9 Post-process chain (HDR target, tone-map, hook point)
    
    Phase 3 — Runtime
    - 3.1 Scene hierarchy + save/load wiring + prefabs + play isolation
    - 3.3 Input actions (Actions/Axes/Bindings/Contexts) + gamepad
    - 3.4 Physics abstraction + Jolt (or chosen) backend
    - 3.5 Audio subsystem
    - 3.6 Animation (skeletal, clips, state machines, retarget)
    
    Phase 4 — Slate polish
    - 4.1 Retained layout/interaction state + perf
    - 4.2 AA primitives, rounded-rect polish, clipping
    - 4.3 Text layout (wrap, ellipsis, multi-weight) + sub-pixel crispness
    - 4.5 Widget catalog fill-out (Tree, Property, Search, Dialog, Tooltip, ContextMenu, ProgressBar)
    - 4.6 Flex/stack/grid layout primitives
    - 4.7 Float windows + layout persistence
    - 4.8 Token system consolidation (no hard-coded colors anywhere)
    - 4.9 Icon set + DPI-correct atlas
    - 4.10 Accessibility (keyboard nav, focus, scaling, high-contrast)
    
    Phase 5 — Editor
    - 5.1 Viewport view modes (persp/ortho, top/front/side), focus, snapping, toolbar
    - 5.2 Outliner — search, multi-select, rename, delete, hierarchy, context menu
    - 5.3 Inspector — reflection-driven property UI, add/remove component, multi-edit
    - 5.4 Content Browser (folder tree, asset grid, drag-drop, search/sort)
    - 5.5 Console — severity/category filter, search, clickable errors, Problems view
    - 5.6 Asset Registry + import pipeline (FBX/glTF, PNG/JPG/HDR, fonts, shaders, audio)
    - 5.7 Rotate + scale gizmos + snapping
    - 5.8 Play/Stop with state isolation
    - 5.9 Layout persistence
    
    Phase 6 — Project Browser
    - 6.1 Rich entries (thumbnail, last-opened, version)
    - 6.2 Data-driven template packages
    - 6.3 Professional New-Project UI (name, location, template, render config, target platform)
    - 6.4 Recent-projects persistence + missing-project self-heal
    - 6.5 Full project folder layout
    
    Phase 7 — Tooling (all 🔲)
    - 7.1 Material Editor · 7.2 Shader Editor · 7.3 Animation Editor · 7.4 Terrain · 7.5 VFX
    
    Phase 8 — Production (all 🔲)
    - 8.1 Packaging/Build Tooling · 8.2 Optimization · 8.3 Profiler · 8.4 Crash Reporting · 8.5 Plugin/Module System · 8.6 Per-subsystem docs
    
    Cross-cutting
    - Scripting (ScriptComponent, lifecycle, editor integration)
    - AI & Navigation (navmesh, agents, behavior systems)
    - Tests (Math, Serialization, Asset Registry, Scene, Renderer, Input, Physics, Slate, Project, Build)