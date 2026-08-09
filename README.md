# Luma Engine

A production-grade 3D game engine built on Vulkan, targeting Windows x64 first.
Luma is being built incrementally, milestone by milestone, keeping the repository
buildable and tested at every step.

> **Status:** Milestones 0–1 complete — build infrastructure, the `Luma::Core`
> foundation, a GLFW-backed platform window, and a running application loop.
> The Vulkan renderer (Milestone 2) is next. See
> [Docs/Architecture/Foundation.md](Docs/Architecture/Foundation.md) and the
> [roadmap](#roadmap).

## Requirements

| Tool | Version used | Notes |
|------|--------------|-------|
| Windows | 11 x64 | primary target |
| MSVC | VS 2022/BuildTools 18 (toolset 14.5x) | C++20 |
| CMake | ≥ 3.28 (tested 4.4) | Ninja Multi-Config |
| Ninja | ≥ 1.13 | build generator |
| Vulkan SDK | 1.4.341.1 (`D:/VulkanSDK/...`) | detected now, used from M2 |

GLFW 3.4 and Catch2 v3 are fetched automatically by CMake (`FetchContent`).

## Build & run

From a **Developer PowerShell** (or using the scripts, which activate MSVC):

```powershell
# Configure + build + test + run
./Scripts/Configure.ps1
./Scripts/Build.ps1 -Config Development
./Scripts/Test.ps1  -Config Development
./Scripts/Run.ps1   -Config Development
```

Or directly with CMake presets (from an environment where `cl` is on PATH):

```bash
cmake --preset msvc
cmake --build build --config Development
ctest --test-dir build -C Development --output-on-failure
```

Build configurations: **Debug**, **Development** (optimized + asserts + warnings
as errors), **Release**, **Shipping** (asserts/verbose logs compiled out).

## Layout

```
Engine/Core/       Luma::Core   - types, assert, log, config, events, layers, app loop
Engine/Platform/   Luma::Platform - Window interface + GLFW backend (GLFW hidden)
Runtime/Sandbox/   LumaEngine.exe - runtime host that opens the window
Tests/             Catch2 tests (run via ctest)
cmake/             compiler flags + dependency helpers
Config/            default Engine.ini
Scripts/           Configure/Build/Test/Run/Clean (PowerShell)
Docs/              architecture docs
docs/superpowers/  design spec + implementation plan
```

`.cpp` sources within a module are grouped by subsystem, e.g.
`Engine/Core/src/Event/`, `Engine/Core/src/Log/`.

## Roadmap

- [x] **M0** Repo, CMake, configs, Vulkan detection, logging, asserts, config, tests
- [x] **M1** Platform window, events, input foundation, application loop
- [ ] **M2** Vulkan foundation (instance → swapchain → clear screen)
- [ ] **M3** Rendering core (RHI, buffers, textures, shaders, pipelines, mesh)
- [ ] **M4** Scene system (world/entity/components, serialization)
- [ ] **M5** Luma Slate UI framework
- [ ] **M6** Luma Editor
- [ ] **M7+** Asset pipeline, gameplay, advanced rendering, tooling

## Design principles

Modular subsystems behind clear interfaces; Vulkan kept isolated behind an RHI;
production quality over demos; every subsystem tested; hot reload as a first-class
dev feature; incremental, always-buildable milestones. Full brief in
[docs/superpowers/specs](docs/superpowers/specs/).
