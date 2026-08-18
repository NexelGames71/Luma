#include "Luma/Editor/Panels/WorldOutliner.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <string>
#include <vector>

#include "Luma/Scene/Components.h"
#include "Luma/Slate/Icons.h"

namespace Luma::Editor::Panels {

using Slate::Align;
using Slate::Color;
using Slate::Icon;
using Slate::Rect;

void WorldOutlinerPanel::Draw(Slate::Context& ui, const Slate::Rect& body,
                              PanelContext& ctx) {
    Slate::Theme& t = ui.theme();

    // Remember the content rect for the floating menu overlay (it is drawn
    // after the dock by DrawFloatingMenu, so it can't see `body` again).
    m_bodyRect = body;

    // Whether the menu was already open before this frame's input was
    // processed. Used to keep the outside-click close from immediately
    // swallowing the click that just opened the menu.
    m_menuOpenedThisFrame = false;

    // --- In-panel header: [+ add] [search........] ------------------------
    // The create menu's trigger lives on the LEFT of a header strip inside
    // the panel body (next to the entity search), matching the toolbar style
    // of the Content Browser. Right-click no longer opens the menu.
    const f32 kHeaderH = 30.0f;
    Rect header{body.x, body.y, body.w, kHeaderH};
    ui.GradientRect(header, t.surface2, t.surface1);
    ui.Panel({header.x, header.Bottom() - 1.0f, header.w, 1.0f}, t.separator);

    // "+" add-gameobject button, icon only, left side of the header.
    const f32 kBtn = 20.0f;
    const u64 btnId = Slate::Context::ID("outliner.add");
    Rect btn{header.x + 6.0f, header.y + (kHeaderH - kBtn) * 0.5f, kBtn,
             kBtn};
    {
        bool hovered = btn.Contains(ui.mouse());
        if (hovered) {
            ui.RequestCursor(Luma::CursorShape::Hand);
        }
        if (hovered && ui.mousePressed(0)) m_pressedRow = btnId;
        bool clicked = false;
        if (m_pressedRow == btnId && ui.mouseReleased(0)) {
            if (hovered) clicked = true;
            m_pressedRow = 0;
        }
        f32 hoverT = ui.Animate(btnId ^ 0xA0D0ull, hovered, t.motion.hover);
        if (hoverT > 0.01f) {
            Color fill = t.menu.hoverFill.WithAlpha(
                static_cast<u8>(255.0f * hoverT));
            ui.drawList().AddRectFilledRounded(btn, fill, t.radius.sm);
        }
        if (m_createMenuOpen) {
            ui.drawList().AddRectOutline(btn, t.accent, t.border.hairline);
        }
        if (m_createButtonIcon) {
            ui.Image(m_createButtonIcon, {btn.x + 1.0f, btn.y + 1.0f, 18.0f,
                                          18.0f});
        } else {
            Slate::DrawIcon(ui, btn, Icon::Plus,
                            m_createMenuOpen ? t.accent : t.textDim);
        }
        if (clicked) {
            m_createMenuOpen = !m_createMenuOpen;
            m_openSubmenu = -1;
            m_searchText.clear();
            m_menuFocusIndex = 0;
            m_menuX = btn.x;
            m_menuY = btn.Bottom() + 2.0f;
            m_menuOpenedThisFrame = true;
        }
    }

    // Entity search box filling the rest of the header (filters the list).
    Rect searchR{btn.Right() + 6.0f, header.y + 3.0f,
                 header.Right() - btn.Right() - 12.0f, kHeaderH - 6.0f};
    ui.SearchBox(Slate::Context::ID("outliner.entitysearch"), searchR,
                 m_entityFilter, m_texSearchGlass, "Search...");

    // Actor icon shown beside each entity name (Unreal-pack glyphs): the
    // mesh primitive / light type / environment texture, tinted like the
    // row's label. Returns 0 when the entity has no recognizable actor type.
    auto iconFor = [&](Entity e) -> TextureHandle {
        if (ctx.scene->Registry().all_of<MeshRendererComponent>(e)) {
            const auto& mr =
                ctx.scene->Registry().get<MeshRendererComponent>(e);
            if (mr.UsesMeshAsset()) return m_texActorMesh;
            switch (mr.primitive) {
                case MeshPrimitive::Plane: return m_texActorPlane;
                case MeshPrimitive::Sphere: return m_texActorSphere;
                case MeshPrimitive::Cylinder: return m_texActorCylinder;
                default: return m_texActorCube;
            }
        }
        if (ctx.scene->Registry().all_of<LightComponent>(e)) {
            const auto& lc = ctx.scene->Registry().get<LightComponent>(e);
            switch (lc.type) {
                case LightType::Point: return m_texActorPointLight;
                case LightType::Spot: return m_texActorSpotLight;
                case LightType::Tube:
                    return m_texActorPointLight;  // no tube glyph in the pack
                default: return m_texActorDirLight;
            }
        }
        if (ctx.scene->Registry().all_of<EnvironmentComponent>(e)) {
            return m_texCatEnvironment;
        }
        return 0;
    };

    auto view = ctx.scene->Registry().view<const NameComponent>();
    if (view.begin() == view.end()) {
        ui.LabelIn({body.x, body.y + kHeaderH + 8.0f, body.w, 22},
                   "  (no entities)", t.textDim);
        m_listScroll = 0.0f;
    } else {
        // Filter the list by the header search text (case-insensitive
        // substring on the entity name).
        std::string needle;
        for (char c : m_entityFilter) {
            needle += static_cast<char>(
                std::tolower(static_cast<unsigned char>(c)));
        }
        std::vector<Entity> visible;
        for (Entity e : view) {
            const std::string& name = view.get<const NameComponent>(e).name;
            if (needle.empty()) {
                visible.push_back(e);
                continue;
            }
            std::string ln = name;
            for (char& c : ln) {
                c = static_cast<char>(
                    std::tolower(static_cast<unsigned char>(c)));
            }
            if (ln.find(needle) != std::string::npos) visible.push_back(e);
        }

        if (visible.empty()) {
            ui.LabelIn({body.x, body.y + kHeaderH + 8.0f, body.w, 22},
                       "  No matching entities", t.textDim);
            m_listScroll = 0.0f;
        } else {
            // While the create menu is up, the list is inert: hovering or
            // clicking the floating menu (which overlays this panel) must not
            // drive selection or hover highlights in the outliner underneath.
            const bool menuUp = m_createMenuOpen;
            const f32 kIconS = 16.0f;
            // The list lives below the header; it is clipped to its region so
            // scrolled rows can't overlap the header, and shifted up by the
            // scroll offset. The scrollbar is added after the loop once the
            // content height is known.
            const f32 listTop = body.y + kHeaderH + 2.0f;
            Rect listRegion{body.x, listTop, body.w, body.Bottom() - listTop};
            ui.PushClip(listRegion);
            f32 y = listTop - m_listScroll;
            for (Entity e : visible) {
                const std::string& name =
                    view.get<const NameComponent>(e).name;
                Rect row{body.x + 4, y, body.w - 8, 24};
                bool selected = (e == *ctx.selected);
                const u64 rowId =
                    Slate::Context::ID(name.c_str()) ^ 0x0F0F0F0Full;
                bool hovered = !menuUp && row.Contains(ui.mouse());
                if (hovered) {
                    ui.RequestCursor(Luma::CursorShape::Hand);
                }
                if (!menuUp && hovered && ui.mousePressed(0)) {
                    m_pressedRow = rowId;
                }
                bool clicked = false;
                if (!menuUp && m_pressedRow == rowId &&
                    ui.mouseReleased(0)) {
                    if (hovered) clicked = true;
                    m_pressedRow = 0;
                }
                if (selected) {
                    ui.drawList().AddRectFilledRounded(row, t.selectionBg,
                                                       t.radius.sm);
                } else if (hovered) {
                    ui.drawList().AddRectFilledRounded(row, t.surface2,
                                                       t.radius.sm);
                }
                TextureHandle icon = iconFor(e);
                const f32 textX =
                    icon ? row.x + t.space.xs + kIconS + t.space.sm
                         : row.x + t.space.md + t.space.xs;
                if (icon) {
                    ui.Image(icon,
                             {row.x + t.space.xs,
                              row.y + (row.h - kIconS) * 0.5f, kIconS,
                              kIconS},
                             selected ? t.selectionText : t.text);
                }
                ui.Label({textX,
                          row.y + (row.h - ui.font().LineHeight()) * 0.5f},
                         name, selected ? t.selectionText : t.text);
                if (clicked) *ctx.selected = e;
                y += 26.0f;
            }
            ui.PopClip();
            // Scroll region spans the list area below the header. Content
            // height is recovered from where the row loop ended.
            f32 contentH = (y - listTop) + m_listScroll;
            m_listScroll = ui.VerticalScroll(
                Slate::Context::ID("outliner.list"), listRegion, contentH,
                m_listScroll);
        }
    }

    // The menu itself is drawn as a floating overlay by DrawFloatingMenu
    // (after the dock) so it isn't clipped to this panel's rect.
}

void WorldOutlinerPanel::DrawFloatingMenu(Slate::Context& ui,
                                          PanelContext& ctx) {
    // The header "+" button lives in the panel's in-panel header (drawn in
    // Draw). This floating pass only draws the create-menu overlay, which
    // must render after the dock so it isn't clipped to this panel's rect.
    DrawCreateMenu(ui, m_bodyRect, ctx, m_menuOpenedThisFrame);
}

void WorldOutlinerPanel::DrawCreateMenu(Slate::Context& ui,
                                        const Slate::Rect& body,
                                        PanelContext& ctx,
                                        bool menuOpenedThisFrame) {
    if (!m_createMenuOpen) return;
    Slate::Theme& t = ui.theme();

    // Unreal Place Actors structure with compact sizing: a narrow menu whose
    // category rows (Basic >, Geometry >, ...) open a submenu beside them on
    // hover. Sizes come from the menu tokens.
    const f32 kMenuW = 190.0f;
    const f32 kSubW = 180.0f;
    const f32 kRowH = t.menu.rowH;
    const f32 kHeaderH = t.menu.headerH;
    const f32 kHeaderGap = t.space.md;  // breathing room under the header
    const f32 pad = 6.0f;

    // --- Menu data: PLACE ACTORS -----------------------------------------
    struct ActorItem {
        const char* label;
        int kind;  // CreateActorKind
        Icon icon;          // procedural fallback when `tex` is missing
        TextureHandle tex;  // actor texture (drawn gray) when present
    };
    struct Category {
        const char* label;
        Icon icon;          // procedural fallback when `tex` is missing
        TextureHandle tex;  // silhouette texture (drawn gray) when present
        const ActorItem* items;
        int count;
    };
    static const ActorItem basicItems[] = {
        {"Empty Actor", static_cast<int>(CreateActorKind::Empty), Icon::None,
         0},
    };
    static const ActorItem geoItems[] = {
        {"Cube", static_cast<int>(CreateActorKind::Cube), Icon::Cube,
         m_texActorCube},
        {"Plane", static_cast<int>(CreateActorKind::Plane), Icon::Plane,
         m_texActorPlane},
        {"Sphere", static_cast<int>(CreateActorKind::Sphere), Icon::Sphere,
         m_texActorSphere},
        {"Cylinder", static_cast<int>(CreateActorKind::Cylinder),
         Icon::Cylinder, m_texActorCylinder},
    };
    static const ActorItem lightItems[] = {
        {"Directional Light",
         static_cast<int>(CreateActorKind::DirectionalLight), Icon::Light,
         m_texActorDirLight},
        {"Point Light", static_cast<int>(CreateActorKind::PointLight),
         Icon::Light, m_texActorPointLight},
        {"Spot Light", static_cast<int>(CreateActorKind::SpotLight),
         Icon::Light, m_texActorSpotLight},
        {"Tube Light", static_cast<int>(CreateActorKind::TubeLight),
         Icon::Light, m_texActorPointLight},
    };
    static const ActorItem envItems[] = {
        {"Environment", static_cast<int>(CreateActorKind::Environment),
         Icon::None, m_texCatEnvironment},
    };
    const Category categories[] = {
        {"Basic", Icon::None, 0, basicItems,
         static_cast<int>(std::size(basicItems))},
        {"Geometry", Icon::Cube, m_texCatGeometry, geoItems,
         static_cast<int>(std::size(geoItems))},
        {"Lights", Icon::Light, m_texCatLight, lightItems,
         static_cast<int>(std::size(lightItems))},
        {"Environment", Icon::None, m_texCatEnvironment, envItems,
         static_cast<int>(std::size(envItems))},
    };
    const int kCatCount = static_cast<int>(std::size(categories));

    // --- Search filter ----------------------------------------------------
    // Typing filters rows in real time (case-insensitive substring on labels
    // and category names); a category whose items match auto-opens its
    // submenu so the hit is directly reachable.
    auto lower = [](std::string s) {
        for (char& c : s) {
            c = static_cast<char>(
                std::tolower(static_cast<unsigned char>(c)));
        }
        return s;
    };
    std::string needle = lower(m_searchText);
    auto matches = [&needle, &lower](const char* label) {
        if (needle.empty()) return true;
        return lower(std::string(label)).find(needle) != std::string::npos;
    };

    // Escape: clear the filter first, then dismiss the menu.
    if (ui.keyEscape()) {
        if (!m_searchText.empty()) {
            m_searchText.clear();
            m_menuFocusIndex = 0;
        } else {
            m_createMenuOpen = false;
            m_openSubmenu = -1;
            m_searchText.clear();
            m_menuFocusIndex = 0;
            m_pressedRow = 0;
            return;
        }
    }

    // Visibility: a category shows when its label or any of its items match;
    // its submenu then lists only the matching items.
    bool catVisible[4] = {};
    bool itemVisible[4][4] = {};
    bool anyItemMatch[4] = {};
    int subVis[4] = {};
    int visCount = 0;
    for (int c = 0; c < kCatCount; ++c) {
        const Category& cat = categories[c];
        if (matches(cat.label)) catVisible[c] = true;
        for (int i = 0; i < cat.count; ++i) {
            if (matches(cat.items[i].label)) {
                itemVisible[c][i] = true;
                anyItemMatch[c] = true;
                ++subVis[c];
                catVisible[c] = true;
            }
        }
        if (catVisible[c]) ++visCount;
    }

    // --- Layout & placement ----------------------------------------------
    f32 contentH = t.menu.searchH + pad;
    if (visCount > 0) {
        contentH += t.menu.sectionGap + kHeaderH + kHeaderGap +
                    static_cast<f32>(visCount) * kRowH;
    } else {
        contentH += kRowH;  // "No matches" row
    }

    f32 mx = m_menuX;
    f32 my = m_menuY;
    if (mx + kMenuW > body.Right()) mx = body.Right() - kMenuW - 4.0f;
    if (my + contentH > body.Bottom()) my = body.Bottom() - contentH - 4.0f;
    Rect main{mx - pad, my - pad, kMenuW + pad * 2.0f, contentH + pad * 2.0f};

    // Y of each visible category row (-1 when hidden).
    f32 catY[4] = {-1.0f, -1.0f, -1.0f, -1.0f};
    if (visCount > 0) {
        f32 ry = my + pad + t.menu.searchH + pad;
        ry += t.menu.sectionGap + kHeaderH + kHeaderGap;
        for (int c = 0; c < kCatCount; ++c) {
            if (!catVisible[c]) continue;
            catY[c] = ry;
            ry += kRowH;
        }
    }

    // Flat index -> category for the top-level rows (keyboard nav).
    int flatCat[4] = {-1, -1, -1, -1};
    {
        int k = 0;
        for (int c = 0; c < kCatCount; ++c) {
            if (!catVisible[c]) continue;
            flatCat[k++] = c;
        }
    }

    // --- Open submenu (hover-driven, Unreal-style) -------------------------
    auto subRectFor = [&](int cat, f32& outSx, f32& outSy,
                          f32& outH) -> bool {
        if (cat < 0 || !catVisible[cat] || subVis[cat] == 0 ||
            catY[cat] < 0.0f) {
            return false;
        }
        outH = static_cast<f32>(subVis[cat]) * kRowH + pad * 2.0f;
        outSy = catY[cat];
        if (outSy + outH > body.Bottom()) outSy = body.Bottom() - outH - 4.0f;
        // Always sit flush beside the main menu — never render over it. When
        // the panel is too narrow for both, the submenu overflows the panel's
        // right edge (it's a floating overlay) rather than covering the menu.
        outSx = mx + kMenuW;
        if (outSx + kSubW > body.Right()) {
            f32 clamped = body.Right() - kSubW - 4.0f;
            if (clamped > outSx) outSx = clamped;
        }
        return true;
    };

    int openCat = m_openSubmenu;
    if (openCat >= 0 && (!catVisible[openCat] || subVis[openCat] == 0)) {
        openCat = -1;
    }
    // Hover opens the hovered category's submenu; moving into the submenu (or
    // an active search) keeps it; hovering dead space closes it.
    int hov = -1;
    for (int c = 0; c < kCatCount; ++c) {
        if (catVisible[c] &&
            Rect{mx, catY[c], kMenuW, kRowH}.Contains(ui.mouse())) {
            hov = c;
            break;
        }
    }
    {
        f32 sx, sy, h;
        bool subShown =
            subRectFor(openCat, sx, sy, h) &&
            Rect{sx - pad, sy - pad, kSubW + pad * 2.0f, h + pad * 2.0f}
                .Contains(ui.mouse());
        if (hov >= 0) {
            m_openSubmenu = subVis[hov] > 0 ? hov : -1;
        } else if (!subShown) {
            if (!needle.empty()) {
                // Search: keep the current submenu, or auto-open the first
                // category with matching items so the hit is visible.
                if (m_openSubmenu < 0 || !catVisible[m_openSubmenu] ||
                    subVis[m_openSubmenu] == 0) {
                    m_openSubmenu = -1;
                    for (int c = 0; c < kCatCount; ++c) {
                        if (catVisible[c] && anyItemMatch[c]) {
                            m_openSubmenu = c;
                            break;
                        }
                    }
                }
            } else {
                m_openSubmenu = -1;  // hovered header/empty space, or left menu
            }
        }
    }
    openCat = m_openSubmenu;
    if (openCat >= 0 && (!catVisible[openCat] || subVis[openCat] == 0)) {
        openCat = -1;
    }

    // --- Submenu rect (final) ---------------------------------------------
    // The submenu appears flush beside the main menu — it never slides over
    // the context menu.
    f32 subSx = 0.0f, subSy = 0.0f, subH = 0.0f;
    bool subValid = subRectFor(openCat, subSx, subSy, subH);
    Rect sub{0, 0, 0, 0};
    if (subValid) {
        sub = {subSx - pad, subSy - pad, kSubW + pad * 2.0f,
               subH + pad * 2.0f};
    }

    // --- Keyboard navigation ---------------------------------------------
    int flatCount = visCount + (subValid ? subVis[openCat] : 0);
    if (ui.keyDown() && flatCount > 0) {
        m_menuFocusIndex = m_menuFocusIndex < 0
                               ? 0
                               : std::min(m_menuFocusIndex + 1, flatCount - 1);
    }
    if (ui.keyUp() && flatCount > 0) {
        m_menuFocusIndex = m_menuFocusIndex < 0
                               ? flatCount - 1
                               : std::max(m_menuFocusIndex - 1, 0);
    }
    // Caret keys belong to the search box while it's focused.
    if (!ui.textFieldFocused()) {
        if (ui.keyHome() && flatCount > 0) m_menuFocusIndex = 0;
        if (ui.keyEnd() && flatCount > 0) m_menuFocusIndex = flatCount - 1;
        if (ui.keyRight() && m_menuFocusIndex >= 0 &&
            m_menuFocusIndex < visCount) {
            int c = flatCat[m_menuFocusIndex];
            if (subVis[c] > 0) m_openSubmenu = c;
        }
        if (ui.keyLeft() && m_menuFocusIndex >= visCount && openCat >= 0) {
            // Close the submenu and return focus to the parent category row.
            int parentIdx = -1;
            for (int k = 0; k < visCount; ++k) {
                if (flatCat[k] == openCat) {
                    parentIdx = k;
                    break;
                }
            }
            m_openSubmenu = -1;
            if (parentIdx >= 0) m_menuFocusIndex = parentIdx;
        }
    }
    if (m_menuFocusIndex >= flatCount) {
        m_menuFocusIndex = flatCount > 0 ? flatCount - 1 : -1;
    }

    // --- Panel: matches the docked panels' background, 1px outline --------
    ui.PanelRoundedBordered(main, t.panelBg, t.outline, t.radius.md,
                            t.border.hairline);

    // --- Search box -------------------------------------------------------
    Rect searchRect{mx + pad, my + pad, kMenuW - pad * 2.0f, t.menu.searchH};
    const u64 searchId = Slate::Context::ID("outliner.search");
    // Same field style as the Content Browser's search bar (SearchBox with a
    // texture leading icon — the Unreal pack SearchGlass, not the procedural
    // glyph).
    if (ui.SearchBox(searchId, searchRect, m_searchText, m_texSearchGlass,
                     "Start typing to search")) {
        m_menuFocusIndex = 0;  // re-filter: jump to the first match
    }
    // Auto-focus the search box on the frame the menu opens so typing filters
    // immediately. Must run after SearchBox: the click that opened the menu
    // would otherwise be treated as an outside-click that unfocuses the field.
    if (menuOpenedThisFrame) {
        ui.FocusField(searchId);
    }

    auto closeMenu = [&] {
        m_createMenuOpen = false;
        m_openSubmenu = -1;
        m_searchText.clear();
        m_menuFocusIndex = 0;
        m_pressedRow = 0;
    };

    // --- Section header + category rows ----------------------------------
    f32 y = my + pad + t.menu.searchH + pad;
    if (visCount > 0) {
        y += t.menu.sectionGap;
        ui.MenuSectionHeader({mx, y, kMenuW, kHeaderH}, "PLACE ACTORS");
        y += kHeaderH + kHeaderGap;
    }

    int flatIdx = 0;
    for (int c = 0; c < kCatCount; ++c) {
        if (!catVisible[c]) continue;
        const Category& cat = categories[c];
        Rect row{mx, y, kMenuW, kRowH};
        const u64 rowId = Slate::Context::ID(cat.label) ^ 0xA11CEull;
        bool hovered = row.Contains(ui.mouse());
        if (hovered) {
            ui.RequestCursor(Luma::CursorShape::Hand);
        }
        if (hovered && ui.mousePressed(0)) m_pressedRow = rowId;
        bool clicked = false;
        if (m_pressedRow == rowId && ui.mouseReleased(0)) {
            if (hovered) clicked = true;
            m_pressedRow = 0;
        }
        bool focused = flatIdx == m_menuFocusIndex;
        if (focused && ui.enterPressed()) clicked = true;

        // Unreal-style highlight: saturated fill + light text on the open /
        // hovered / keyboard-focused row.
        bool highlighted = (c == openCat) || hovered || focused;
        if (highlighted) {
            ui.drawList().AddRectFilledRounded(row, t.menu.highlightFill,
                                               t.radius.sm);
            if (focused) {
                ui.drawList().AddRectFilled(
                    {row.x, row.y + 3.0f, 2.5f, row.h - 6.0f}, t.accent);
            }
        }
        // Category silhouette texture (Unreal-style, drawn gray like the
        // procedural icons) with a procedural fallback when it's missing.
        const f32 kIconS = 18.0f;
        Rect iconRect{row.x + t.menu.iconInset,
                      row.y + (row.h - kIconS) * 0.5f, kIconS, kIconS};
        const Color iconTint =
            highlighted ? t.menu.highlightText : t.text;
        if (cat.tex) {
            ui.Image(cat.tex, iconRect, iconTint);
        } else if (cat.icon != Icon::None) {
            Slate::DrawIcon(ui, iconRect, cat.icon, iconTint);
        }
        Slate::DrawIcon(ui, {row.Right() - 22.0f,
                             row.y + (row.h - 16.0f) * 0.5f, 16.0f, 16.0f},
                        Icon::ChevronRight, iconTint);
        const f32 textX = row.x + t.menu.iconInset + kIconS + t.space.sm;
        ui.Label({textX, row.y + (row.h - ui.font().LineHeight()) * 0.5f},
                 cat.label, highlighted ? t.menu.highlightText : t.text);

        if (clicked && subVis[c] > 0) m_openSubmenu = c;
        y += kRowH;
        ++flatIdx;
    }

    if (visCount == 0) {
        Rect row{mx, y, kMenuW, kRowH};
        ui.Label({mx + t.space.lg,
                  row.y + (row.h - ui.font().LineHeight()) * 0.5f},
                 "No matches", t.textDim);
    }

    // --- Submenu (open category's items) ----------------------------------
    if (subValid) {
        const Category& cat = categories[openCat];
        ui.PanelRoundedBordered(sub, t.panelBg, t.outline, t.radius.md,
                                t.border.hairline);
        f32 y2 = subSy;
        for (int i = 0; i < cat.count; ++i) {
            if (!itemVisible[openCat][i]) continue;
            const ActorItem& it = cat.items[i];
            Rect row{subSx, y2, kSubW, kRowH};
            const u64 rowId =
                (Slate::Context::ID(it.label) ^ 0x51AB1Eull) +
                static_cast<u64>(openCat) * 7919ull;
            bool hovered = row.Contains(ui.mouse());
            if (hovered) {
                ui.RequestCursor(Luma::CursorShape::Hand);
            }
            if (hovered && ui.mousePressed(0)) m_pressedRow = rowId;
            bool clicked = false;
            if (m_pressedRow == rowId && ui.mouseReleased(0)) {
                if (hovered) clicked = true;
                m_pressedRow = 0;
            }
            bool focused = flatIdx == m_menuFocusIndex;
            if (focused && ui.enterPressed()) clicked = true;

            bool highlighted = hovered || focused;
            if (highlighted) {
                ui.drawList().AddRectFilledRounded(row, t.menu.highlightFill,
                                                   t.radius.sm);
                if (focused) {
                    ui.drawList().AddRectFilled(
                        {row.x, row.y + 3.0f, 2.5f, row.h - 6.0f}, t.accent);
                }
            }
            // Actor texture (Unreal primitive thumbnails, drawn gray like the
            // procedural icons) with a procedural fallback when missing.
            const f32 kItemIconS = 18.0f;
            Rect itemIconRect{row.x + t.menu.iconInset,
                              row.y + (row.h - kItemIconS) * 0.5f, kItemIconS,
                              kItemIconS};
            const Color itemTint =
                highlighted ? t.menu.highlightText : t.text;
            if (it.tex) {
                ui.Image(it.tex, itemIconRect, itemTint);
            } else if (it.icon != Icon::None) {
                Slate::DrawIcon(ui, itemIconRect, it.icon, itemTint);
            }
            ui.Label({row.x + t.menu.iconInset + kItemIconS + t.space.sm,
                      row.y + (row.h - ui.font().LineHeight()) * 0.5f},
                     it.label, highlighted ? t.menu.highlightText : t.text);

            if (clicked) {
                if (ctx.onCreateActor) {
                    ctx.onCreateActor(static_cast<CreateActorKind>(it.kind));
                }
                closeMenu();
                return;
            }
            y2 += kRowH;
            ++flatIdx;
        }
    }

    // --- Outside-click closes the menu (and the submenu) ------------------
    // A click on the header "+" button in the same frame is not an outside
    // click — it just opened the menu.
    if (!menuOpenedThisFrame && ui.mousePressed(0)) {
        if (!main.Contains(ui.mouse()) && !sub.Contains(ui.mouse())) {
            closeMenu();
        }
    }

    // Release the mouse-held id when the button is no longer down so a drag
    // that ends outside any row can't leave the pressed highlight stuck.
    if (!ui.isMouseDown(0)) m_pressedRow = 0;
}

}  // namespace Luma::Editor::Panels
