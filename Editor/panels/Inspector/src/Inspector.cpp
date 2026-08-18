#include "Luma/Editor/Panels/Inspector.h"

#include <algorithm>
#include <cstdio>

#include "Luma/Scene/Components.h"

namespace Luma::Editor::Panels {

using Slate::Align;
using Slate::Rect;

void InspectorPanel::Draw(Slate::Context& ui, const Slate::Rect& body,
                          PanelContext& ctx) {
    Slate::Theme& t = ui.theme();

    if (!ctx.scene->IsValid(*ctx.selected)) {
        ui.LabelIn({body.x, body.y + 8, body.w, 22}, "  No selection",
                   t.textDim);
        m_showAddMenu = false;
        return;
    }
    // Selection changed — close popups so they don't target the previous
    // entity's components, and jump the content scroll back to the top.
    if (*ctx.selected != m_lastSelected) {
        ClosePopups();
        m_lastSelected = *ctx.selected;
        m_scroll = 0.0f;
    }
    auto& reg = ctx.scene->Registry();
    f32 x = body.x + 12.0f;
    f32 y = body.y + 12.0f;
    f32 w = body.w - 24.0f;

    ui.Heading({x, y, w - 118.0f, 24}, reg.get<NameComponent>(*ctx.selected).name,
               t.text);
    if (ui.Button(Slate::Context::ID("insp.addcomp"),
                  {body.Right() - 118.0f, y, 106, 24}, "+ Component")) {
        m_showAddMenu = !m_showAddMenu;
    }
    // The header row (name + + Component) stays pinned; the component
    // content below it scrolls. The content region is clipped so scrolled
    // rows can't overlap the pinned header, and the scrollbar is added at
    // the end once the content height is known.
    const f32 contentTop = body.y + 46.0f;
    Rect contentRegion{body.x, contentTop, body.w, body.h - 46.0f};
    ui.PushClip(contentRegion);
    y = contentTop - m_scroll;

    auto sectionHeader = [&](const char* label) {
        ui.GradientRect({body.x, y, body.w, 26}, t.surface3, t.surface2);
        ui.Panel({body.x, y, 3, 26}, t.accent);
        ui.Heading({x, y, w, 26}, label, t.text);
        y += 32.0f;
    };
    auto vec3Row = [&](const char* label, u64 id, f32* v) {
        ui.LabelIn({x, y, 82, 22}, label, t.textDim);
        ui.Vector3Field(id, {x + 82, y, w - 82, 22}, v);
        y += 28.0f;
    };
    auto floatRow = [&](const char* label, u64 id, f32& v) {
        ui.LabelIn({x, y, 118, 22}, label, t.textDim);
        ui.DragFloat(id, {x + 118, y, w - 118, 22}, v, 0.02f);
        y += 28.0f;
    };

    // --- Color property rows -------------------------------------------------
    // Component colors are Math::Vec3 (0..1 floats); the picker works in
    // 8-bit Slate::Color. A color row is a compact swatch strip — clicking it
    // opens a floating ColorPicker panel (drawn by DrawFloatingPickers after
    // the dock) instead of a bulky inline picker.
    auto vec3ToColor = [](const Math::Vec3& v) {
        return Slate::Color::RGB(
            static_cast<u8>(std::clamp(v.x, 0.0f, 1.0f) * 255.0f + 0.5f),
            static_cast<u8>(std::clamp(v.y, 0.0f, 1.0f) * 255.0f + 0.5f),
            static_cast<u8>(std::clamp(v.z, 0.0f, 1.0f) * 255.0f + 0.5f));
    };
    auto colorToVec3 = [](const Slate::Color& c) {
        return Math::Vec3(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f);
    };
    auto colorRow = [&](const char* label, Slate::ColorPickerPopup& popup,
                        Math::Vec3& value) {
        ui.LabelIn({x, y, 82, 22}, label, t.textDim);
        // Wide swatch strip filling the rest of the row; clicking it opens
        // the floating color picker.
        Rect sw{x + 82, y + 2, w - 82, 18};
        // Edits write straight back into the component; the picker follows
        // the component through SetColor below (no-op while editing).
        popup.OnColorChanged = [&](const Slate::Color& c) {
            value = colorToVec3(c);
        };
        popup.SetColor(vec3ToColor(value));
        popup.DrawSwatch(ui, sw);
        y += 28.0f;
    };

    if (reg.all_of<TransformComponent>(*ctx.selected)) {
        sectionHeader("Transform");
        auto& tf = reg.get<TransformComponent>(*ctx.selected);
        vec3Row("Position", Slate::Context::ID("insp.pos"), &tf.position.x);
        vec3Row("Rotation", Slate::Context::ID("insp.rot"),
                &tf.rotationEuler.x);
        vec3Row("Scale", Slate::Context::ID("insp.scl"), &tf.scale.x);
        y += 8.0f;
    }
    if (reg.all_of<MeshRendererComponent>(*ctx.selected)) {
        sectionHeader("Mesh Renderer");
        auto& mr = reg.get<MeshRendererComponent>(*ctx.selected);
        ui.LabelIn({x, y, 82, 22}, "Shape", t.textDim);
        const char* shapes[4] = {"Cube", "Plane", "Sphere", "Cylinder"};
        f32 bw = (w - 82.0f) / 4.0f;
        for (int p = 0; p < 4; ++p) {
            Rect r{x + 82.0f + bw * static_cast<f32>(p), y, bw - 2.0f, 22};
            bool sel = static_cast<int>(mr.primitive) == p;
            if (ui.Tab(Slate::Context::ID(shapes[p]), r, shapes[p], sel)) {
                mr.primitive = static_cast<MeshPrimitive>(p);
            }
        }
        y += 28.0f;
        colorRow("Albedo", m_albedoPicker, mr.albedo);
        floatRow("Metallic", Slate::Context::ID("insp.met"), mr.metallic);
        floatRow("Roughness", Slate::Context::ID("insp.rgh"), mr.roughness);
        y += 8.0f;
    }
    if (reg.all_of<EnvironmentComponent>(*ctx.selected)) {
        sectionHeader("Environment");
        auto& env = reg.get<EnvironmentComponent>(*ctx.selected);

        colorRow("Sky Tint", m_skyTintPicker, env.skyTint);

        // --- Atmosphere ---
        ui.LabelIn({x, y, w, 20}, "Atmosphere", t.accent);
        y += 24.0f;
        ui.Checkbox(Slate::Context::ID("insp.skyon"), {x, y, 18, 18},
                    "Sky Enabled", env.skyEnabled);
        y += 26.0f;
        vec3Row("Rayleigh Scatter", Slate::Context::ID("insp.rayscat"),
                &env.rayleighScattering.x);
        floatRow("Rayleigh Height (m)",
                 Slate::Context::ID("insp.rayh"), env.rayleighScaleHeight);
        floatRow("Mie Scatter", Slate::Context::ID("insp.miescat"),
                 env.mieScattering);
        floatRow("Mie Absorption", Slate::Context::ID("insp.mieabs"),
                 env.mieAbsorption);
        floatRow("Mie Height (m)", Slate::Context::ID("insp.mieh"),
                 env.mieScaleHeight);
        floatRow("Mie Anisotropy", Slate::Context::ID("insp.mieg"),
                 env.mieAnisotropy);
        floatRow("Ozone Scale", Slate::Context::ID("insp.ozone"),
                 env.ozoneScale);

        // --- Sky post-processing ---
        y += 4.0f;
        ui.LabelIn({x, y, w, 20}, "Sky Post", t.accent);
        y += 24.0f;
        floatRow("Sky Intensity", Slate::Context::ID("insp.skyi"),
                 env.skyIntensity);
        floatRow("Saturation", Slate::Context::ID("insp.sats"),
                 env.saturation);
        floatRow("Exposure", Slate::Context::ID("insp.exp"), env.exposure);

        // --- Ground & ambient ---
        y += 4.0f;
        ui.LabelIn({x, y, w, 20}, "Ground & Ambient", t.accent);
        y += 24.0f;
        colorRow("Ground Color", m_groundColorPicker, env.groundColor);
        floatRow("IBL Intensity", Slate::Context::ID("insp.ibl"),
                 env.iblIntensity);
        y += 8.0f;
    }
    if (reg.all_of<LightComponent>(*ctx.selected)) {
        sectionHeader("Light");
        auto& lc = reg.get<LightComponent>(*ctx.selected);
        ui.LabelIn({x, y, 82, 22}, "Type", t.textDim);
        const char* types[4] = {"Directional", "Point", "Spot", "Tube"};
        f32 bw = (w - 82.0f) / 4.0f;
        for (int p = 0; p < 4; ++p) {
            Rect r{x + 82.0f + bw * static_cast<f32>(p), y, bw - 2.0f, 22};
            bool sel = static_cast<int>(lc.type) == p;
            if (ui.Tab(Slate::Context::ID(types[p]), r, types[p], sel)) {
                lc.type = static_cast<LightType>(p);
            }
        }
        y += 28.0f;
        colorRow("Color", m_lightColorPicker, lc.color);
        floatRow("Intensity", Slate::Context::ID("insp.lint"), lc.intensity);
        if (lc.type == LightType::Directional) {
            // --- Sun disk (directional) ---
            y += 4.0f;
            ui.LabelIn({x, y, w, 20}, "Sun Disk", t.accent);
            y += 24.0f;
            floatRow("Disk Size (deg)", Slate::Context::ID("insp.ldisks"),
                     lc.sunDiskSizeDeg);
            floatRow("Disk Intensity", Slate::Context::ID("insp.ldiski"),
                     lc.sunDiskIntensity);
        } else {
            // --- Attenuation (point/spot) ---
            y += 4.0f;
            ui.LabelIn({x, y, w, 20}, "Attenuation", t.accent);
            y += 24.0f;
            floatRow("Radius", Slate::Context::ID("insp.lrad"),
                     lc.attenuationRadius);
            floatRow("Falloff Power", Slate::Context::ID("insp.lpow"),
                     lc.attenuationPower);
            if (lc.type == LightType::Tube) {
                floatRow("Length", Slate::Context::ID("insp.llen"),
                         lc.length);
            }
            if (lc.type == LightType::Spot) {
                floatRow("Inner Angle", Slate::Context::ID("insp.lin"),
                         lc.innerAngleDeg);
                floatRow("Outer Angle", Slate::Context::ID("insp.lout"),
                         lc.outerAngleDeg);
            }
        }

        // --- Shadows ---
        y += 4.0f;
        ui.LabelIn({x, y, w, 20}, "Shadows", t.accent);
        y += 24.0f;
        ui.Checkbox(Slate::Context::ID("insp.lshadows"), {x, y, 18, 18},
                    "Cast Shadows", lc.castShadows);
        y += 26.0f;
        f32 mapSize = static_cast<f32>(lc.shadowMapSize);
        floatRow("Map Size", Slate::Context::ID("insp.lmapsize"), mapSize);
        lc.shadowMapSize = static_cast<i32>(mapSize);
        floatRow("Bias", Slate::Context::ID("insp.lbias"), lc.shadowBias);
        floatRow("Normal Bias", Slate::Context::ID("insp.lnbias"),
                 lc.normalBias);
        floatRow("Softness", Slate::Context::ID("insp.lsoft"),
                 lc.shadowSoftness);
        if (lc.type == LightType::Directional) {
            f32 cascades = static_cast<f32>(lc.cascadeCount);
            floatRow("Cascades", Slate::Context::ID("insp.lcascades"),
                     cascades);
            lc.cascadeCount = static_cast<i32>(cascades);
            floatRow("Shadow Distance", Slate::Context::ID("insp.ldist"),
                     lc.shadowDistance);
            floatRow("Split Lambda", Slate::Context::ID("insp.llambda"),
                     lc.cascadeSplitLambda);
        }
        y += 8.0f;
    }

    ui.PopClip();
    // Scroll region spans the content area below the pinned header; the bar
    // starts right under it and the range matches the visible region (so the
    // thumb reaches the end exactly when the last row is visible).
    f32 contentH = (y - contentTop) + m_scroll;
    m_scroll = ui.VerticalScroll(Slate::Context::ID("insp.scroll"),
                                 contentRegion, contentH, m_scroll);

    if (m_showAddMenu) {
        bool canMesh = !reg.all_of<MeshRendererComponent>(*ctx.selected);
        bool canEnv = !reg.all_of<EnvironmentComponent>(*ctx.selected);
        bool canLight = !reg.all_of<LightComponent>(*ctx.selected);
        int count = (canMesh ? 1 : 0) + (canEnv ? 1 : 0) +
                    (canLight ? 1 : 0);
        f32 mw = 190.0f;
        f32 mx = body.Right() - mw - 12.0f;
        f32 my = body.y + 44.0f;
        f32 rows = static_cast<f32>(count > 0 ? count : 1);
        ui.PanelRoundedBordered(
            {mx - 6, my - 6, mw + 12, rows * 28.0f + 12.0f}, t.surface4,
            t.accent, t.radius.md, t.border.thick);
        if (canMesh &&
            ui.Button(Slate::Context::ID("add.mesh"), {mx, my, mw, 24},
                      "Mesh Renderer")) {
            reg.emplace<MeshRendererComponent>(*ctx.selected);
            m_showAddMenu = false;
        }
        if (canMesh) my += 28.0f;
        if (canLight &&
            ui.Button(Slate::Context::ID("add.light"), {mx, my, mw, 24},
                      "Light")) {
            reg.emplace<LightComponent>(*ctx.selected);
            m_showAddMenu = false;
        }
        if (canLight) my += 28.0f;
        if (canEnv &&
            ui.Button(Slate::Context::ID("add.env"), {mx, my, mw, 24},
                      "Environment")) {
            reg.emplace<EnvironmentComponent>(*ctx.selected);
            m_showAddMenu = false;
        }
        if (count == 0) {
            ui.LabelIn({mx, my, mw, 24}, "  All components added", t.textDim);
        }
    }
}

void InspectorPanel::ClosePopups() {
    m_showAddMenu = false;
    m_albedoPicker.Close();
    m_lightColorPicker.Close();
    m_skyTintPicker.Close();
    m_groundColorPicker.Close();
}

void InspectorPanel::DrawFloatingPickers(Slate::Context& ui,
                                         PanelContext& ctx) {
    // Called after the dock (clip stack unwound) so the floating picker
    // panels can overflow this column. Only draw panels whose component is
    // present on the current selection; OnColorChanged (set per-row in Draw)
    // writes the edits straight into the component.
    if (!ctx.scene->IsValid(*ctx.selected)) return;
    auto& reg = ctx.scene->Registry();
    if (reg.all_of<MeshRendererComponent>(*ctx.selected)) {
        m_albedoPicker.DrawPanel(ui);
    }
    if (reg.all_of<EnvironmentComponent>(*ctx.selected)) {
        m_skyTintPicker.DrawPanel(ui);
        m_groundColorPicker.DrawPanel(ui);
    }
    if (reg.all_of<LightComponent>(*ctx.selected)) {
        m_lightColorPicker.DrawPanel(ui);
    }
}

}  // namespace Luma::Editor::Panels
