# Luma Engine Foundation (M0+M1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver the first buildable, testable slice of Luma Engine — build infrastructure, the Core module, a Platform/Window layer, and a running application loop that opens a native window.

**Architecture:** A CMake (Ninja Multi-Config) super-build with two static libraries (`Luma::Core`, `Luma::Platform`) and one executable (`LumaEngine.exe`). Core depends only on the C++ std lib; Platform wraps GLFW behind an abstract `Window` interface (GLFW types never leak into public headers). Catch2 tests cover Core and run via ctest.

**Tech Stack:** C++20, MSVC (VS BuildTools 18), CMake 4.4 + Ninja Multi-Config, GLFW 3.4 (FetchContent), Catch2 v3 (FetchContent), Vulkan SDK 1.4.341.1 (detected now, used in M2).

## Global Constraints

- C++20, header-based (no C++ modules). Warnings `/W4 /permissive- /EHsc`; warnings-as-errors in the **Development** config.
- Four build configs: **Debug, Development, Release, Shipping** (Ninja Multi-Config), each defining `LUMA_CONFIG_<NAME>`.
- `namespace Luma`; targets `Luma::Core`, `Luma::Platform`; executable `LumaEngine.exe`.
- No placeholder architecture (Rule 9): create only directories with real code.
- External deps only behind Luma interfaces (GLFW hidden via PImpl/opaque handle).
- Vulkan detected via `find_package(Vulkan)` pinned to `D:/VulkanSDK/1.4.341.1`; not linked until M2.
- Every subsystem gets tests; repo stays buildable after every task.

---

### Task 1: Build system skeleton + Vulkan detection

**Files:** Create `CMakeLists.txt`, `CMakePresets.json`, `cmake/LumaCompilerFlags.cmake`, `cmake/LumaDependencies.cmake`, `Engine/Core/CMakeLists.txt`, `Engine/Core/src/Version.cpp`, `Engine/Core/include/Luma/Core/Version.h`.

**Produces:** Configurable/buildable `Luma::Core` static lib; presets `msvc-{debug,dev,release,shipping}`; `Luma::Vulkan` detected interface (SDK path logged); option `LUMA_BUILD_TESTS`.

- [ ] Top-level CMake: `cmake_minimum_required(VERSION 3.28)`, project `LumaEngine` CXX, C++20, set `CMAKE_CONFIGURATION_TYPES` = Debug;Development;Release;Shipping, include compiler-flag + dependency helpers, `find_package(Vulkan REQUIRED)` with `Vulkan_ROOT` hint to the D: SDK and a `message(STATUS)` reporting it, `add_subdirectory(Engine/Core)`, guarded `Engine/Platform`/`Runtime/Sandbox`/`Tests`.
- [ ] `LumaCompilerFlags.cmake`: interface target `luma_flags` applying `/W4 /permissive- /EHsc /utf-8`, per-config defines `LUMA_CONFIG_DEBUG/DEVELOPMENT/RELEASE/SHIPPING`, `/Od /Zi` for Debug/Dev, `/O2` + `NDEBUG` for Release/Shipping, warnings-as-errors (`/WX`) only for Development.
- [ ] `CMakePresets.json`: one configure preset per config using generator `Ninja Multi-Config`, binaryDir `build`, plus build presets.
- [ ] Minimal `Version.h/.cpp` (`Luma::EngineVersionString()`) so the lib has a real TU.
- [ ] Verify: `cmake --preset msvc-dev` configures (Vulkan STATUS line shows `1.4.341.1`) and `cmake --build --preset msvc-dev` builds `Luma::Core`.
- [ ] Commit: `build: cmake skeleton, presets, Vulkan detection, Core lib target`.

### Task 2: Core Types + Assert (+ Catch2/ctest wiring)

**Files:** Create `Engine/Core/include/Luma/Core/Types.h`, `.../Assert.h`, `Engine/Core/src/Assert.cpp`, `Tests/CMakeLists.txt`, `Tests/Core/AssertTests.cpp`. Modify top `CMakeLists.txt` (enable_testing + Tests subdir), `cmake/LumaDependencies.cmake` (Catch2 FetchContent).

**Produces:** `Luma` fixed-width aliases (`u8..u64,i8..i64,f32,f64,usize`); `LUMA_ASSERT(cond,msg)`, `LUMA_CHECK(cond,msg)`; `Luma::Detail::SetAssertHandler` for testing; ctest runs Catch2.

- [ ] Write failing test `AssertTests.cpp`: install a custom assert handler capturing failures, assert `LUMA_CHECK(false,"x")` invokes it with the message; `LUMA_CHECK(true,...)` does not.
- [ ] Wire Catch2 v3 via FetchContent; `Tests/CMakeLists.txt` builds `LumaTests` linking `Luma::Core` + `Catch2::Catch2WithMain`; `catch_discover_tests`.
- [ ] Run: expect FAIL (symbols missing).
- [ ] Implement Types.h + Assert (handler indirection, debugger break via `__debugbreak`, routes message; `LUMA_ASSERT` compiled out when `LUMA_CONFIG_SHIPPING`).
- [ ] Run `ctest --preset msvc-dev`: PASS.
- [ ] Commit: `feat(core): fixed-width types, assert/check, Catch2 test harness`.

### Task 3: Core Log

**Files:** Create `.../Log.h`, `src/Log.cpp`; `Tests/Core/LogTests.cpp`.

**Interfaces — Produces:** `enum class LogLevel { Trace,Debug,Info,Warning,Error,Fatal }`; `struct ILogSink { virtual void Write(const LogMessage&)=0; }`; `LogMessage{level,category,message,threadId,timestamp}`; `Luma::Log::Init/Shutdown`, `AddSink`, `SetLevel`; macros `LUMA_LOG_TRACE/INFO/WARN/ERROR/FATAL(category, fmt, ...)` using `std::format`. A capturing test sink verifies routing + level filtering.

- [ ] Failing test: add a `VectorSink`, log at Info while level=Warning → not captured; at Error → captured with correct level/category/message.
- [ ] Run: FAIL.
- [ ] Implement Log (thread-safe sink list, `std::format`, timestamp, thread id; colored console sink + file sink implementations).
- [ ] Run tests: PASS.
- [ ] Commit: `feat(core): logging system with pluggable sinks and levels`.

### Task 4: Core Config

**Files:** Create `.../Config.h`, `src/Config.cpp`; `Tests/Core/ConfigTests.cpp`; `Config/Engine.ini`.

**Produces:** `class Config { bool LoadFromString(std::string_view); bool LoadFromFile(path); std::string GetString(key,def); int GetInt(key,def); float GetFloat(key,def); bool GetBool(key,def); }` — `.ini`-style `key=value`, `#`/`;` comments, `[section]` prefixes keys as `section.key`.

- [ ] Failing test: parse a sample string with a section, comment, and int/bool/float/string values; assert typed getters + defaults for missing keys.
- [ ] Run: FAIL.
- [ ] Implement Config parser.
- [ ] Run tests: PASS.
- [ ] Commit: `feat(core): ini-style config with typed getters`.

### Task 5: Core Event + EventDispatcher

**Files:** Create `.../Event.h`, `.../Events.h` (concrete window/key/mouse events), `src/Event.cpp`; `Tests/Core/EventTests.cpp`.

**Produces:** `enum class EventType`; `enum EventCategory` (bitflags); `struct Event { virtual EventType Type() const; virtual int Categories() const; bool Handled=false; }`; concrete `WindowCloseEvent, WindowResizeEvent{u32 w,h}, KeyPressedEvent{i32 key,bool repeat}, KeyReleasedEvent, MouseMovedEvent{f32 x,y}, MouseButtonPressedEvent{i32}, MouseScrolledEvent{f32,f32}`; `class EventDispatcher{ EventDispatcher(Event&); template<class T,class F> bool Dispatch(F);}` sets `Handled` when handler returns true.

- [ ] Failing test: dispatch a `WindowResizeEvent` through `EventDispatcher`; matching handler runs and receives w/h, sets Handled; non-matching type handler not run.
- [ ] Run: FAIL.
- [ ] Implement Event/Events/dispatcher.
- [ ] Run tests: PASS.
- [ ] Commit: `feat(core): strongly-typed event system + dispatcher`.

### Task 6: Core Layer + LayerStack

**Files:** Create `.../Layer.h`, `.../LayerStack.h`, `src/LayerStack.cpp`; `Tests/Core/LayerStackTests.cpp`.

**Interfaces — Consumes:** `Event` (Task 5). **Produces:** `class Layer { virtual ~Layer(); virtual void OnAttach(); OnDetach(); OnUpdate(float dt); OnEvent(Event&); const std::string& Name(); }`; `class LayerStack { void PushLayer(unique_ptr<Layer>); void PushOverlay(unique_ptr<Layer>); begin()/end(); rbegin()/rend(); }` — overlays always after layers; iteration order documented.

- [ ] Failing test: push two layers + one overlay; assert forward order = [L0,L1,Overlay] and that overlay stays last after another PushLayer inserts before it.
- [ ] Run: FAIL.
- [ ] Implement Layer + LayerStack (insert index for layers, append for overlays).
- [ ] Run tests: PASS.
- [ ] Commit: `feat(core): layer stack with overlay ordering`.

### Task 7: Core Application + EngineLoop

**Files:** Create `.../Application.h`, `.../EngineLoop.h`, `src/Application.cpp`; `.../Timestep.h`; `Tests/Core/ApplicationTests.cpp`.

**Interfaces — Consumes:** LayerStack, Log, Event. **Produces:** `struct ApplicationSpec{ std::string name; u32 width,height; }`; `class Application { Application(ApplicationSpec); virtual ~Application; void PushLayer/PushOverlay(...); void OnEvent(Event&); void RunOneFrame(float dt); void Close(); bool IsRunning(); LayerStack& Layers(); }`. Loop iterates layers `OnUpdate(dt)` and dispatches events top-down (rbegin→rend), stopping when `Handled`. `WindowCloseEvent` sets running=false. (Window ownership added in Task 9/10; Application here is window-agnostic and unit-testable.)

- [ ] Failing test: subclass a probe Layer counting OnUpdate calls; `RunOneFrame(0.016)` increments it; dispatching a `WindowCloseEvent` flips `IsRunning()` false; a handling overlay stops propagation to lower layers.
- [ ] Run: FAIL.
- [ ] Implement Application/EngineLoop/Timestep.
- [ ] Run tests: PASS.
- [ ] Commit: `feat(core): application + engine loop (window-agnostic core)`.

### Task 8: Platform Window abstraction + GlfwWindow

**Files:** Create `Engine/Platform/CMakeLists.txt`, `Engine/Platform/include/Luma/Platform/Window.h`, `Engine/Platform/src/GlfwWindow.h`, `.../GlfwWindow.cpp`. Modify top `CMakeLists.txt` (add subdir), `cmake/LumaDependencies.cmake` (GLFW FetchContent).

**Interfaces — Consumes:** Core Event/Events, Log. **Produces:** `struct WindowProps{ std::string title; u32 width=1280,height=720; }`; abstract `class Window { using EventCallback=std::function<void(Event&)>; virtual ~Window(); virtual void PollEvents()=0; virtual bool ShouldClose() const=0; virtual void SetEventCallback(EventCallback)=0; virtual u32 Width()/Height() const=0; virtual void* NativeHandle() const=0; static std::unique_ptr<Window> Create(const WindowProps&); }`. `GlfwWindow` implements it; **no GLFW type in any public header** (opaque `struct GLFWwindow;` fwd only in the .cpp/impl). GLFW callbacks translate into Core events fed to the stored callback. `Create` returns `GlfwWindow` (glfwInit/glfwTerminate refcounted).

- [ ] Add GLFW 3.4 via FetchContent (BUILD examples/tests/docs OFF); link privately to `Luma::Platform`.
- [ ] Implement Window.h interface + GlfwWindow (hint `GLFW_CLIENT_API=GLFW_NO_API` for future Vulkan; wire close/resize/key/mouse callbacks → events).
- [ ] Build `Luma::Platform` (no unit test — exercised by Task 9 manual run; header compiles clean with no glfw include leak — verified by Core-only tests still linking).
- [ ] Commit: `feat(platform): abstract Window + GLFW backend (glfw isolated)`.

### Task 9: Runtime Sandbox — LumaEngine.exe (M1 deliverable)

**Files:** Create `Runtime/Sandbox/CMakeLists.txt`, `Runtime/Sandbox/main.cpp`, `Runtime/Sandbox/SandboxLayer.h`. Modify top `CMakeLists.txt` (add subdir). Create `README.md`, `Scripts/{Configure,Build,Clean,Test,Run}.ps1`.

**Interfaces — Consumes:** Application, Window, Log, Config, Layer.

- [ ] `main.cpp`: `Log::Init` (+console/file sinks), load `Config/Engine.ini`, build `ApplicationSpec`, create `Window`, set its event callback to `Application::OnEvent`, push a `SandboxLayer`; loop `while(app.IsRunning() && !window->ShouldClose()){ window->PollEvents(); app.RunOneFrame(dt); }`; log FPS ~1/sec; clean shutdown + `Log::Shutdown`.
- [ ] `SandboxLayer`: logs OnAttach/OnDetach; OnEvent logs key presses; OnUpdate is a no-op stub.
- [ ] PowerShell scripts wrap the preset commands; README documents build/run/test.
- [ ] Verify (manual): `Scripts/Build.ps1` then run `LumaEngine.exe` → native window opens, title shows, loop runs stably, ESC/close exits cleanly, log file written. `ctest` still green.
- [ ] Commit: `feat(runtime): LumaEngine sandbox opens native window with stable loop`.

### Task 10: Docs + baseline README polish

**Files:** Create `Docs/Architecture/Foundation.md`; modify `README.md`.

- [ ] Document the module map, build configs, how to add a module, and the M0/M1 status vs. the roadmap.
- [ ] Commit: `docs: foundation architecture + getting started`.

## Self-Review

- **Spec coverage:** M0 (repo, CMake, configs, dir structure, logging, assertions, config, tests, Vulkan detection) → Tasks 1–4, 2. M1 (platform, window, events, input foundation, app loop) → Tasks 5–9. Layer stack (§6) → Task 6. Event system (§7) → Task 5. Error handling (§37) → Task 2 assert + error logging. Deferred subsystems explicitly out of scope per spec.
- **Placeholder scan:** none — each task has concrete files, interfaces, and test intent.
- **Type consistency:** `Event`/`EventDispatcher`/`Layer`/`LayerStack`/`Application` signatures defined once (Tasks 5–7) and consumed by later tasks with matching names.
