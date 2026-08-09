# Foundation Architecture (Milestones 0–1)

This document describes the foundational layers of Luma Engine as they exist
after Milestones 0 and 1. It is the baseline every later subsystem builds on.

## Module graph

```
LumaEngine.exe (Runtime/Sandbox)
        │  depends on
        ▼
   Luma::Platform ──► GLFW (private, hidden)
        │
        ▼
     Luma::Core ──► C++ standard library only
        │
        ▼
     Luma::Flags (interface target: warnings, C++20, per-config defines)
```

- **Luma::Core** depends on nothing but the standard library.
- **Luma::Platform** depends on Core and links GLFW **privately** — no GLFW type
  appears in any header Core or the runtime can see.
- **LumaEngine.exe** links Core + Platform and owns the real timing loop.
- Vulkan is *detected* by CMake now but not linked until Milestone 2.

## Luma::Core

Sources are grouped by subsystem under `Engine/Core/src/<Subsystem>/`; public
headers live under `Engine/Core/include/Luma/Core/` and are included as
`Luma/Core/<Name>.h`.

| Subsystem | Header(s) | Responsibility |
|-----------|-----------|----------------|
| Foundation | `Types.h`, `Assert.h`, `Version.h` | fixed-width scalar types; `LUMA_ASSERT` (fatal, out in Shipping) / `LUMA_CHECK` (recoverable) with a pluggable handler; engine version |
| Log | `Log.h` | levels Trace→Fatal, `std::format` macros, thread-safe dispatch to pluggable sinks (colored console, flushing file, in-memory for tests) |
| Config | `Config.h` | INI-style parse (`[Section]` → `Section.key`), typed getters with fallbacks |
| Event | `Event.h`, `Events.h` | strongly-typed `Event` base + `EventDispatcher`; concrete window/key/mouse events via `LUMA_EVENT_CLASS_*` macros |
| Layer | `Layer.h`, `LayerStack.h` | layer lifecycle; stack where overlays stay above layers; forward = update order, reverse = event order |
| Application | `Application.h`, `EngineLoop.h`, `Timestep.h` | window-agnostic app core: owns the layer stack, routes events, `RunOneFrame(dt)`; `FrameClock` produces a bounded delta |

### Assertion / error model

The macro only tests the condition and calls the active handler on failure; the
handler decides what to do. The default handler logs to stderr and, for fatal
asserts, breaks into the debugger and aborts. Tests install a capturing handler
so a "failure" is observable without killing the process. `LUMA_ASSERT` is
compiled out in Shipping; `LUMA_CHECK` is always present.

## Luma::Platform

`Window` is an abstract interface (`PollEvents`, `ShouldClose`,
`SetEventCallback`, size, native handle) created via `Window::Create`. The
`GlfwWindow` backend lives entirely in `src/Window/` and includes `<GLFW/...>`
only in its `.cpp`. GLFW window/input callbacks are translated into `Luma`
events and forwarded to the callback the application installed.

Keeping GLFW private means the windowing backend can later be swapped (native
Win32, SDL, …) without touching the rest of the engine — the same isolation rule
the renderer will apply to Vulkan through the RHI.

## The main loop

The runtime (`Runtime/Sandbox/main.cpp`) wires everything together:

```
Log::Init + sinks
   → load Config/Engine.ini
   → Application(spec)
   → Window::Create(props); window.SetEventCallback(app.OnEvent)
   → app.PushLayer(SandboxLayer)
   → while running && !window.ShouldClose():
         window.PollEvents()          // GLFW → Luma events → app → layers
         dt = clock.Tick()
         app.RunOneFrame(dt)          // layers OnUpdate bottom-to-top
         (render / present: TODO M2)
   → shutdown (layers detached, window destroyed, logs flushed)
```

Events flow **into** the app from the window callback; `Application::OnEvent`
consumes `WindowClose` (stopping the loop) and otherwise delivers events
top-to-bottom through the layer stack until one marks them handled.

## Build configurations

Ninja Multi-Config with four configs. `Luma::Flags` applies `/W4 /permissive-`,
defines `LUMA_CONFIG_<NAME>`, and sets custom flag sets for Development
(optimized, asserts on, `/WX`) and Shipping (`/O2 /DNDEBUG`).

## What's intentionally absent

No renderer/RHI, ECS, editor/UI, asset pipeline, physics/audio/animation,
scripting, reflection, or networking yet. Each is a later milestone with its own
spec under `docs/superpowers/specs/`. This keeps the foundation small, correct,
and fully tested before scaling up.
