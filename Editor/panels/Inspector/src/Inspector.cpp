#include "Luma/Editor/Panels/Inspector.h"

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
    y += 34.0f;

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
        vec3Row("Albedo", Slate::Context::ID("insp.alb"), &mr.albedo.x);
        floatRow("Metallic", Slate::Context::ID("insp.met"), mr.metallic);
        floatRow("Roughness", Slate::Context::ID("insp.rgh"), mr.roughness);
        y += 8.0f;
    }
    if (reg.all_of<EnvironmentComponent>(*ctx.selected)) {
        sectionHeader("Environment");
        auto& env = reg.get<EnvironmentComponent>(*ctx.selected);
        ui.Checkbox(Slate::Context::ID("insp.skyon"), {x, y, 18, 18},
                    "Sky Enabled", env.skyEnabled);
        y += 26.0f;
        vec3Row("Sun Dir", Slate::Context::ID("insp.sundir"),
                &env.sunDirection.x);
        vec3Row("Sun Color", Slate::Context::ID("insp.suncol"),
                &env.sunColor.x);
        vec3Row("Ground", Slate::Context::ID("insp.ground"),
                &env.groundColor.x);
        floatRow("Turbidity", Slate::Context::ID("insp.turb"), env.turbidity);
        floatRow("Sun Intensity", Slate::Context::ID("insp.suni"),
                 env.sunIntensity);
        floatRow("Sky Intensity", Slate::Context::ID("insp.skyi"),
                 env.skyIntensity);
        floatRow("Sun Size", Slate::Context::ID("insp.suns"),
                 env.sunSizeDegrees);
        y += 8.0f;
    }
    if (reg.all_of<LightComponent>(*ctx.selected)) {
        sectionHeader("Light");
        auto& lc = reg.get<LightComponent>(*ctx.selected);
        ui.LabelIn({x, y, 82, 22}, "Type", t.textDim);
        const char* types[3] = {"Directional", "Point", "Spot"};
        f32 bw = (w - 82.0f) / 3.0f;
        for (int p = 0; p < 3; ++p) {
            Rect r{x + 82.0f + bw * static_cast<f32>(p), y, bw - 2.0f, 22};
            bool sel = static_cast<int>(lc.type) == p;
            if (ui.Tab(Slate::Context::ID(types[p]), r, types[p], sel)) {
                lc.type = static_cast<LightType>(p);
            }
        }
        y += 28.0f;
        vec3Row("Color", Slate::Context::ID("insp.lcol"), &lc.color.x);
        floatRow("Intensity", Slate::Context::ID("insp.lint"), lc.intensity);
        if (lc.type != LightType::Directional) {
            floatRow("Range", Slate::Context::ID("insp.lrng"), lc.range);
        }
        if (lc.type == LightType::Spot) {
            floatRow("Inner Angle", Slate::Context::ID("insp.lin"),
                     lc.innerAngleDeg);
            floatRow("Outer Angle", Slate::Context::ID("insp.lout"),
                     lc.outerAngleDeg);
        }
        y += 8.0f;
    }

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

}  // namespace Luma::Editor::Panels
