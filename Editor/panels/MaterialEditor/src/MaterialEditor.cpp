#include "Luma/Editor/Panels/MaterialEditor.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "Luma/Asset/AssetRegistry.h"
#include "Luma/Core/Log.h"
#include "Luma/Material/MaterialSerializer.h"
#include "Luma/RHI/Renderer.h"
#include "Luma/Slate/Icons.h"

namespace Luma::Editor::Panels {

using Slate::Color;
using Slate::Icon;
using Slate::Rect;
using Luma::Material::MaterialSerializer;

// ---------------------------------------------------------------------------
// Open / Save
// ---------------------------------------------------------------------------

bool MaterialEditorPanel::Open(const std::filesystem::path& path) {
    std::string err;
    if (!MaterialSerializer::LoadFromFile(m_material, path, &err)) {
        LUMA_LOG_ERROR("MaterialEditor", "failed to load {}: {}", path.string(), err);
        return false;
    }
    m_path = path;
    m_open = true;
    m_dirty = false;
    m_previewDirty = true;
    return true;
}

bool MaterialEditorPanel::Save() {
    if (m_path.empty()) return false;
    std::string err;
    if (!MaterialSerializer::SaveToFile(m_material, m_path, &err)) {
        LUMA_LOG_ERROR("MaterialEditor", "failed to save {}: {}", m_path.string(), err);
        return false;
    }
    m_dirty = false;
    return true;
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

void MaterialEditorPanel::Draw(Slate::Context& ui, const Slate::Rect& body,
                               PanelContext& ctx) {
    if (!m_open) {
        ui.LabelIn(body, "No material open.", ui.theme().textDim);
        return;
    }
    Slate::Theme& t = ui.theme();

    // --- Toolbar -----------------------------------------------------------
    const f32 kToolbarH = 32.0f;
    Rect toolbar{body.x, body.y, body.w, kToolbarH};
    ui.GradientRect(toolbar, t.surface2, t.surface1);
    ui.Panel({toolbar.x, toolbar.Bottom() - 1.0f, toolbar.w, 1.0f}, t.separator);

    // Save button.
    {
        Rect r{toolbar.x + 6.0f, toolbar.y + 6.0f, 50.0f, 20.0f};
        bool hovered = r.Contains(ui.mouse());
        if (hovered) ui.RequestCursor(Luma::CursorShape::Hand);
        if (hovered) ui.drawList().AddRectFilledRounded(r, t.surface2, t.radius.sm);
        ui.Label({r.x + 4.0f, r.y + (r.h - ui.font().LineHeight()) * 0.5f},
                 "Save", hovered ? t.text : t.textDim);
        if (hovered && ui.mouseReleased(0)) Save();
    }

    // Material name.
    ui.Heading({toolbar.Right() - 200.0f, toolbar.y, 190.0f, kToolbarH},
               m_dirty ? (m_material.name.empty() ? "Untitled *"
                                                  : m_material.name + " *")
                       : (m_material.name.empty() ? "Untitled" : m_material.name),
               t.text, Slate::Align::Right);

    // --- Two-column layout: preview (left) + properties (right) ------------
    const f32 colW = body.w * 0.4f;
    Rect leftCol{body.x, toolbar.Bottom() + 2.0f, colW, body.Bottom() - toolbar.Bottom() - 2.0f};
    Rect rightCol{leftCol.Right(), toolbar.Bottom() + 2.0f,
                  body.w - colW, body.Bottom() - toolbar.Bottom() - 2.0f};

    DrawPreviewViewport(ui, leftCol, ctx);
    DrawDetailsPanel(ui, rightCol, ctx);
    DrawPropertyRows(ui, rightCol, ctx);
}

// ---------------------------------------------------------------------------
// Preview viewport (renders material on a sphere)
// ---------------------------------------------------------------------------

void MaterialEditorPanel::DrawPreviewViewport(Slate::Context& ui,
                                              const Slate::Rect& rect,
                                              PanelContext& /*ctx*/) {
    Slate::Theme& t = ui.theme();
    ui.PanelRoundedBordered(rect, t.panelBg, t.outline, t.radius.sm, t.border.hairline);
    ui.Label({rect.x + 8.0f, rect.y + 4.0f}, "Preview", t.textDim);

    Rect vp{rect.x + 4.0f, rect.y + 22.0f, rect.w - 8.0f, rect.h - 26.0f};
    if (m_previewTexture) {
        ui.Image(m_previewTexture, vp);
    } else {
        ui.LabelIn(vp, "(no preview)", t.textDim);
    }

    // Orbit on drag.
    if (vp.Contains(ui.mouse()) && ui.mousePressed(0)) {
        m_previewDragging = true;
    }
    if (m_previewDragging) {
        if (ui.isMouseDown(0)) {
            Slate::Vec2 delta = {ui.mouseDelta().x, ui.mouseDelta().y};
            m_previewYaw += delta.x * 0.01f;
            m_previewPitch += delta.y * 0.01f;
            m_previewPitch = std::clamp(m_previewPitch, -1.5f, 1.5f);
            m_previewDirty = true;
        } else {
            m_previewDragging = false;
        }
    }
}

// ---------------------------------------------------------------------------
// Details panel (name, blend mode, texture slots)
// ---------------------------------------------------------------------------

void MaterialEditorPanel::DrawDetailsPanel(Slate::Context& ui,
                                           const Slate::Rect& rect,
                                           PanelContext& /*ctx*/) {
    Slate::Theme& t = ui.theme();
    const f32 kRowH = 22.0f;
    f32 y = rect.y + 4.0f;

    // Name.
    ui.Label({rect.x + 6.0f, y}, "Name", t.textDim);
    y += 18.0f;
    Rect nameR{rect.x + 6.0f, y, rect.w - 12.0f, kRowH};
    if (ui.TextField(Slate::Context::ID("mat.name"), nameR, m_material.name)) {
        MarkDirty();
    }
    y += kRowH + 8.0f;

    // Blend mode.
    ui.Label({rect.x + 6.0f, y}, "Blend Mode", t.textDim);
    y += 18.0f;
    static const char* kBlendNames[] = {"Opaque", "Masked", "Translucent"};
    for (int i = 0; i < 3; ++i) {
        Rect btn{rect.x + 6.0f + i * 68.0f, y, 64.0f, kRowH};
        bool active = (static_cast<int>(m_material.blendMode) == i);
        bool hovered = btn.Contains(ui.mouse());
        if (active) {
            ui.drawList().AddRectFilledRounded(btn, t.accent, t.radius.sm);
        } else if (hovered) {
            ui.drawList().AddRectFilledRounded(btn, t.surface2, t.radius.sm);
        }
        ui.Label({btn.x + 4.0f, btn.y + (btn.h - ui.font().LineHeight()) * 0.5f},
                 kBlendNames[i], active ? Color{1,1,1,1} : t.text);
        if (hovered && ui.mouseReleased(0)) {
            m_material.blendMode = static_cast<Luma::Material::BlendMode>(i);
            MarkDirty();
        }
    }
    y += kRowH + 8.0f;

    // Texture slots.
    auto drawTexSlot = [&](const char* label, AssetId& slot) {
        ui.Label({rect.x + 6.0f, y}, label, t.textDim);
        y += 18.0f;
        Rect slotR{rect.x + 6.0f, y, rect.w - 12.0f, kRowH};
        std::string texName = slot.IsValid() ? ToString(slot) : "(none)";
        ui.Label({slotR.x + 4.0f, slotR.y + (slotR.h - ui.font().LineHeight()) * 0.5f},
                 texName, slot.IsValid() ? t.text : t.textDim);
        if (slotR.Contains(ui.mouse()) && ui.mouseReleased(0)) {
            // Toggle off if assigned.
            if (slot.IsValid()) {
                slot = AssetId{};
                MarkDirty();
            }
        }
        y += kRowH + 4.0f;
    };
    drawTexSlot("Base Color Map", m_material.baseColorMap);
    drawTexSlot("Normal Map", m_material.normalMap);
    drawTexSlot("Roughness Map", m_material.roughnessMap);
    drawTexSlot("Metallic Map", m_material.metallicMap);
}

// ---------------------------------------------------------------------------
// Property rows (scalar/vector values for each MaterialProperty)
// ---------------------------------------------------------------------------

void MaterialEditorPanel::DrawPropertyRows(Slate::Context& ui,
                                           const Slate::Rect& rect,
                                           PanelContext& /*ctx*/) {
    Slate::Theme& t = ui.theme();
    const f32 kRowH = 22.0f;

    // The properties section starts below the details panel.  We draw it
    // at the bottom of rightCol (details panel takes the top portion).
    // Simple: just draw after a fixed offset.
    f32 startY = rect.y + 320.0f;  // below details
    if (startY >= rect.Bottom()) return;

    ui.Label({rect.x + 6.0f, startY}, "Material Properties", t.textDim);
    startY += 20.0f;

    struct PropRow {
        Luma::Material::MaterialProperty prop;
        const char* label;
        bool isVector;  // true = RGB color, false = scalar
    };
    static const PropRow rows[] = {
        {Luma::Material::MaterialProperty::BaseColor, "Base Color", true},
        {Luma::Material::MaterialProperty::Metallic, "Metallic", false},
        {Luma::Material::MaterialProperty::Roughness, "Roughness", false},
        {Luma::Material::MaterialProperty::Specular, "Specular", false},
        {Luma::Material::MaterialProperty::EmissiveColor, "Emissive Color", true},
        {Luma::Material::MaterialProperty::Opacity, "Opacity", false},
        {Luma::Material::MaterialProperty::AmbientOcclusion, "AO", false},
    };

    f32 y = startY;
    for (const auto& row : rows) {
        if (y + kRowH > rect.Bottom()) break;
        ui.Label({rect.x + 6.0f, y + 2.0f}, row.label, t.textDim);

        if (row.isVector) {
            Math::Vec3 v = m_material.VectorValue(row.prop);
            // Color swatch using ColorPickerPopup.
            Rect swatch{rect.Right() - 40.0f, y, 34.0f, kRowH};
            auto& picker = m_colorPickers[row.prop];
            picker.SetColor(Color::RGB(
                static_cast<u8>(v.x * 255.0f),
                static_cast<u8>(v.y * 255.0f),
                static_cast<u8>(v.z * 255.0f)));
            picker.DrawSwatch(ui, swatch);
            picker.OnColorChanged = [this, prop = row.prop](const Color& c) {
                m_material.SetVectorValue(prop, Math::Vec3{c.r / 255.0f, c.g / 255.0f, c.b / 255.0f});
                MarkDirty();
            };
        } else {
            // Scalar field.
            f32 val = m_material.ScalarValue(row.prop);
            Rect field{rect.x + 80.0f, y, rect.w - 90.0f, kRowH};
            std::string str = std::to_string(val);
            if (ui.TextField(Slate::Context::ID("mat.scalar"), field, str)) {
                try { m_material.SetScalarValue(row.prop, std::stof(str)); MarkDirty(); }
                catch (...) {}
            }
        }
        y += kRowH + 4.0f;
    }
}

// ---------------------------------------------------------------------------
// Floating color pickers
// ---------------------------------------------------------------------------

void MaterialEditorPanel::DrawFloatingPickers(Slate::Context& ui) {
    for (auto& [prop, picker] : m_colorPickers) {
        if (!picker.IsOpen()) continue;
        picker.DrawPanel(ui);
    }
}

}  // namespace Luma::Editor::Panels
