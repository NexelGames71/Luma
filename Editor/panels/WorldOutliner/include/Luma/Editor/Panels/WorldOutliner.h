#pragma once

#include <string>

#include "PanelContext.h"
#include "Luma/Scene/Scene.h"
#include "Luma/Slate/Context.h"
#include "Luma/Slate/Types.h"

// World Outliner panel — lists every named entity in the Scene, drives
// selection (which feeds the Inspector + the gizmo via the shared
// PanelContext). Pure UI: owns no scene state. A \"+\" button in the panel
// header opens a Create GameObject menu (Empty / primitives / Light >
// directional, point, spot, tube / Environment) that spawns via the
// PanelContext::onCreateActor hook.

namespace Luma::Editor::Panels {

class WorldOutlinerPanel {
public:
    void Draw(Slate::Context& ui, const Slate::Rect& body, PanelContext& ctx);

    // Draws the open Create GameObject menu as a floating overlay. Called
    // after the dock (clip stack unwound) so the menu renders on top of the
    // panels instead of being scissor-clipped to the outliner column. Also
    // draws the header \"+\" button (the menu's only entry point).
    void DrawFloatingMenu(Slate::Context& ui, PanelContext& ctx);

    // Sets the Create-button texture drawn in the panel header. A zero handle
    // falls back to the procedural plus glyph.
    void SetCreateButtonIcon(TextureHandle icon) { m_createButtonIcon = icon; }

    // Sets the search-glass texture for the menu's search box (Unreal icon
    // pack). A zero handle falls back to the procedural search glyph.
    void SetSearchGlassIcon(TextureHandle icon) { m_texSearchGlass = icon; }

    // Sets the category-row textures (Geometry / Lights / Environment —
    // Unreal-style silhouettes, drawn gray). Zero handles fall back to the
    // procedural glyphs.
    void SetCategoryIcons(TextureHandle geometry, TextureHandle light,
                          TextureHandle environment) {
        m_texCatGeometry = geometry;
        m_texCatLight = light;
        m_texCatEnvironment = environment;
    }

    // Sets the primitive-actor textures for the Geometry submenu rows
    // (Unreal primitive thumbnails, drawn gray). Zero handles fall back to
    // the procedural glyphs.
    void SetPrimitiveIcons(TextureHandle cube, TextureHandle plane,
                           TextureHandle sphere, TextureHandle cylinder) {
        m_texActorCube = cube;
        m_texActorPlane = plane;
        m_texActorSphere = sphere;
        m_texActorCylinder = cylinder;
    }

    // Sets the outliner actor icons (light types + a generic mesh) shown
    // beside entity names in the list. Zero handles fall back to no icon.
    void SetOutlinerActorIcons(TextureHandle dirLight, TextureHandle pointLight,
                               TextureHandle spotLight, TextureHandle mesh) {
        m_texActorDirLight = dirLight;
        m_texActorPointLight = pointLight;
        m_texActorSpotLight = spotLight;
        m_texActorMesh = mesh;
    }

    // Clears the cached selection hint (called after the scene is reloaded
    // so the panel doesn't reference an entity that no longer exists).
    void ClearSelection() {}

private:
    void DrawCreateMenu(Slate::Context& ui, const Slate::Rect& body,
                        PanelContext& ctx, bool menuOpenedThisFrame);

    int m_nextNumber = 1;

    // Create GameObject menu state.
    bool m_createMenuOpen = false;
    int m_openSubmenu = -1;  // category index whose submenu is open (-1 = none)
    f32 m_menuX = 0.0f;
    f32 m_menuY = 0.0f;
    std::string m_searchText;   // filters the menu rows in real time
    std::string m_entityFilter; // filters the outliner entity list by name
    int m_menuFocusIndex = 0;   // keyboard-focused row in the visible list
    u64 m_pressedRow = 0;       // row/button id currently mouse-held
    Slate::Rect m_bodyRect{0, 0, 0, 0};  // outliner content rect (menu clamping)
    bool m_menuOpenedThisFrame = false;  // menu opened by this frame's input
    f32 m_listScroll = 0.0f;             // entity-list vertical scroll offset
    TextureHandle m_createButtonIcon = 0;   // header create-button texture
    TextureHandle m_texSearchGlass = 0;     // search-box leading icon texture
    TextureHandle m_texCatGeometry = 0;     // Geometry category-row icon
    TextureHandle m_texCatLight = 0;        // Lights category-row icon
    TextureHandle m_texCatEnvironment = 0;  // Environment category-row icon
    TextureHandle m_texActorCube = 0;       // Cube primitive row icon
    TextureHandle m_texActorPlane = 0;      // Plane primitive row icon
    TextureHandle m_texActorSphere = 0;     // Sphere primitive row icon
    TextureHandle m_texActorCylinder = 0;   // Cylinder primitive row icon
    TextureHandle m_texActorDirLight = 0;   // Directional-light row icon
    TextureHandle m_texActorPointLight = 0; // Point-light row icon
    TextureHandle m_texActorSpotLight = 0;  // Spot-light row icon
    TextureHandle m_texActorMesh = 0;       // Generic mesh-asset row icon
};

}  // namespace Luma::Editor::Panels
