# Editor / Project Browser Architecture

**Date:** 2026-08-09
**Status:** Approved (informed by studying Godot 4.2 at `C:\Godot-4.2`)

## Goal

Deliver the launch flow: **start → boot/splash → project browser → create/open a
project → the editor opens with that project.** The project browser and the
editor must be cleanly separated, with our own UI (Luma Slate) and styling.

## How Godot does it (reference)

- **One executable, two modes.** `main/main.cpp` parses args:
  `-e/--editor` → editor mode; `-p/--project-manager` → PM mode. With no project
  found it defaults to the project manager (`project_manager = !found_project`).
- **Connection = self-relaunch.** `ProjectManager::_open_selected_projects()`
  builds args `--path <project> --editor`, calls
  `OS::create_instance(args)` to spawn a **new process of the same binary** in
  editor mode, then `get_tree()->quit()` — the PM process exits.

So "separate" means separate *modes/classes*, not separate executables; the two
are connected by relaunching the same binary with a project path.

## Luma's design (Godot model + our LayerStack)

**One executable: `LumaEditor.exe`.** Its mode is chosen by CLI args and realized
as a **Layer** on the application's `LayerStack`:

| Invocation | Mode | Layer pushed |
|-----------|------|--------------|
| `LumaEditor.exe` (no project) or `--project-manager` | Project Browser | `ProjectBrowserLayer` |
| `LumaEditor.exe --project <path.luma>` | Editor | `EditorLayer` (after splash) |

- **Separation of concerns:** `ProjectBrowserLayer` and `EditorLayer` live in
  separate modules (`Editor/ProjectBrowser`, `Editor/LumaEditor`) and share only
  Core/Platform/Slate/Project. Neither depends on the other.
- **Connection:** the browser, on Create/Open, calls
  `LaunchDetached(ExecutablePath(), {"--project", lumaPath})` (new Platform
  primitive) to start the editor instance, then closes its own window — exactly
  Godot's relaunch-and-quit.
- **Boot/splash:** in editor mode, a borderless `Luma::Splash` window shows the
  logo + "compiling / loading…" progress while subsystems (renderer, project
  load, asset scan) initialize; it closes when the editor window is ready.

## UI: Luma Slate

The browser and editor are drawn with **Luma Slate**, our own immediate-mode UI
built on the Vulkan RHI. Glyph rasterization uses `stb_truetype` (a data library,
not a UI framework — consistent with the pragmatic-deps rule); layout, widgets,
input, theming and styling are ours. This is the start of Milestone 5.

Slate v0 widget set needed for the browser: panel/rect, text, button, text field,
tabs, selectable template cards, dropdown (directory), scroll region.

## Rollout (each stays buildable)

1. **PB-1 (done):** `Luma::Project` — `.luma` create/load/discover + tests.
2. **Platform process utils (done):** `ExecutablePath`, `LaunchDetached`.
3. **PB-2:** Luma Slate v0 — textured/colored quads in the RHI, font atlas,
   immediate-mode widgets.
4. **PB-3:** `Luma::Splash` boot/loading window.
5. **PB-4:** `ProjectBrowserLayer` + `LumaEditor.exe` project-browser mode
   (Cave-style layout: tabs, name/dir, template cards, Create/Open).
6. **PB-5:** `EditorLayer` + mode dispatch + splash + relaunch handoff. The full
   flow works end to end.

## Notes

- The existing `Runtime/Sandbox` (`LumaEngine.exe`) remains the runtime/game host;
  the editor is its own `LumaEditor.exe`. (Later, a packaged game runs the runtime
  without the editor — matching the spec's Runtime vs Editor split.)
