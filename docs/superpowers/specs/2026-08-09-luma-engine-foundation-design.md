# Luma Engine — Foundation (Milestone 0 + 1) Design

**Date:** 2026-08-09
**Status:** Approved
**Scope:** First buildable slice of Luma Engine — Milestone 0 (repository, build infra, Core) plus
Milestone 1 (Platform/Window + application loop). This is the first of many spec → plan →
implementation cycles; later milestones (Vulkan, scene, editor, …) get their own specs.

## Context & Environment (verified 2026-08-09)

| Component | Detail |
|---|---|
| Vulkan SDK | `D:/VulkanSDK/1.4.341.1` — `glslc.exe` + `glslangValidator.exe` present |
| Compiler | MSVC via VS BuildTools (`.../Microsoft Visual Studio/18/BuildTools`), VC x64 toolset. `cl` is only on PATH inside a Developer/vcvars environment — CMake presets activate it. Clang to be added as a secondary compiler. |
| CMake | 4.4.0 |
| Ninja | 1.13.2 |
| git | present; working dir `D:/Luma Engine` starts empty (not yet a repo) |
| Platform | Windows 11 x64 |

## Foundational Decisions

1. **Dependency philosophy — Pragmatic.** We write our own foundational modules
   (Core / Math / Containers / Memory) for control and because the master spec lists them as
   first-class modules. We wrap battle-tested libraries for solved GPU/OS problems
   (GLFW for windowing, VMA for Vulkan allocation, glslang/SPIRV-Cross for shaders) — always
   behind Luma interfaces so they remain replaceable. This honors Rule 6 (no unnecessary deps)
   while not reinventing solved problems, and keeps the fastest path to a stable renderer.
2. **First deliverable — Milestone 0 + Milestone 1.** Foundation + a real native window running a
   stable main loop. Vulkan is explicitly deferred to Milestone 2.
3. **Test framework — Catch2 (v3).**
4. **C++ standard — C++20**, header-based (no C++ modules yet; MSVC+CMake module support still
   fiddly). Revisit C++23 later.
5. **Branding — `namespace Luma`**, targets `Luma::Core` / `Luma::Platform`, executable
   `LumaEngine.exe`.

## Repository Layout (M0+M1 subset of the full engine tree)

Per Rule 9 (no placeholder architecture) we create ONLY directories with real, implemented code.
The remaining subsystem directories from the master spec are created when implemented.

```
Luma Engine/
├── CMakeLists.txt              # top-level: C++20, config types, Vulkan detect, options
├── CMakePresets.json           # msvc-{debug,dev,release,shipping} via Ninja Multi-Config + cl
├── cmake/                      # LumaCompilerFlags, LumaDependencies, Vulkan detection helper
├── Engine/
│   ├── Core/                   # target Luma::Core  (depends only on C++ std lib)
│   │   ├── include/Luma/Core/  # Types, Assert, Log, Config, Event, Layer/LayerStack,
│   │   └── src/                #   Application, EngineLoop
│   └── Platform/               # target Luma::Platform (Core + GLFW; GLFW hidden via PImpl)
│       ├── include/Luma/Platform/  # Window (abstract) + input event glue
│       └── src/                    # GlfwWindow impl
├── Runtime/
│   └── Sandbox/                # LumaEngine.exe : wires Application+Window+LayerStack, runs loop
├── Tests/                      # Catch2 tests for Core (run via ctest)
├── Scripts/                    # setup/build/clean/run PowerShell scripts
├── Config/                     # default engine .ini
├── Docs/                       # (docs/ already holds specs)
└── README.md
```

## Module Boundaries (the isolation contract)

### `Luma::Core` (depends only on the C++ std lib)
- `Types.h` — fixed-width aliases (`u8/u16/u32/u64`, `i8…i64`, `f32/f64`, `usize`), `LUMA_API`/attr macros.
- `Assert.h` — `LUMA_ASSERT(cond, msg)` (fatal, compiled out in Shipping), `LUMA_CHECK(cond, msg)`
  (always-on recoverable check), debugger-break integration, routes through Log.
- `Log` — levels Trace→Fatal, named categories, timestamp + thread-id + source location, pluggable
  sinks (`ILogSink`): colored console sink + rotating-ish file sink. `LUMA_LOG_*` macros.
- `Config` — load key/value pairs from an `.ini`-style file; typed getters with defaults.
- `Event` + `EventDispatcher` — strongly-typed event base (master spec §7), category/type,
  handled flag, dispatcher that routes by type.
- `Layer` / `LayerStack` — master spec §6 lifecycle (`OnAttach/OnDetach/OnUpdate/OnEvent`, plus
  render/UI hooks as marked stubs); overlays vs layers ordering.
- `Application` / `EngineLoop` — master spec §5 tick pipeline (Initialize → PollEvents → Input →
  Update → (Physics/Anim/World/Render/UI stubs) → Present → Shutdown), high-res delta time.

### `Luma::Platform` (depends on Core + GLFW)
- Abstract `Window` interface (create/destroy, `ShouldClose`, `PollEvents`, size, title,
  event callback sink).
- `GlfwWindow` implementation. **All GLFW types stay out of public headers** (opaque handle /
  PImpl) so GLFW is replaceable and the OS layer stays isolated (Rule 3 principle extended to OS).
- Translates GLFW input/window callbacks into `Luma::Core` events dispatched to the app.

### `LumaEngine.exe` (Runtime/Sandbox) — the M1 deliverable
Constructs the `Application`, creates a `Window`, pushes a demo `Layer`, runs the loop, logs FPS,
closes cleanly on window-close.

### `Tests` (Catch2)
Covers Core: log formatting/levels, config parse + typed getters + defaults, event dispatch by
type + handled propagation, LayerStack push/pop/overlay ordering, assert/check semantics. Platform
window is manually run in M1 (headless smoke test deferred).

## Build System

- **Top-level CMake:** C++20; warnings `/W4 /permissive- /EHsc`; warnings-as-errors in **Dev**.
  Four configs mapped to **Debug / Development / Release / Shipping** via **Ninja Multi-Config**,
  each defining a `LUMA_CONFIG_*` macro and appropriate optimization/debug/`NDEBUG`-style flags.
  `find_package(Vulkan)` pinned to the D: SDK and verified (wired now, first used in M2).
  Option `LUMA_BUILD_TESTS` (default ON).
- **`CMakePresets.json`:** configure + build presets per config (Ninja Multi-Config + MSVC),
  auto-activating the MSVC environment.
- **Dependencies:** GLFW (3.4) and Catch2 (v3) via CMake `FetchContent`, version-pinned; fallback
  to vendored `ThirdParty/` if offline. VMA/glslang added in M2.
- **Scripts (PowerShell):** `Setup`, `Configure`, `Build`, `Clean`, `Test`, `Run` wrappers.

## Main Loop (master spec §5, M1 form)

```
Application::Run
  Initialize   (Log -> Config -> Window -> attach Layers)
  while !window.ShouldClose():
      PollEvents        (Platform -> Core events -> dispatched top-down through LayerStack)
      per-layer OnUpdate(dt)
      render / UI hooks  (stubs until M2/M5)
      Present            (no-op until M2)
  Shutdown     (detach Layers, destroy Window)
```
`dt` from a high-resolution clock. Recoverable init failures (e.g. window creation) return error
codes → log → graceful exit. Fatal invariants assert. Vulkan validation integration deferred to M2.

## Error Handling
- Assertions: `LUMA_ASSERT` (fatal, Debug/Dev), `LUMA_CHECK` (always-on).
- Recoverable init returns error codes / a small `Result`-style type; log then shut down gracefully.
- Never silently ignore critical errors (master spec §37).

## Definition of Done (first commit)
1. `cmake --preset msvc-dev` configures successfully (Vulkan SDK detected).
2. Build succeeds for the Dev config.
3. `ctest` — all Core tests green.
4. `LumaEngine.exe` opens a native window, runs a stable main loop, logs to console + file, and
   closes cleanly.
5. Repo initialized (`git init`) and architectural baseline committed.

## Explicitly Out of Scope (for this slice)
Vulkan/RHI, rendering, ECS/scene, editor/Luma Slate, asset pipeline, physics/audio/animation,
scripting, reflection codegen, networking, profiling UI, hot reload. Each is a later milestone with
its own spec.
