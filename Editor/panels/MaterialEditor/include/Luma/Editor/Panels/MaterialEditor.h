#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include "Luma/Asset/MaterialThumbnailRenderer.h"
#include "Luma/Material/Material.h"
#include "Luma/Slate/ColorPickerPopup.h"
#include "Luma/Slate/Context.h"
#include "PanelContext.h"

// Material Editor panel — flat property editor for PBR materials.
// Node graph removed; to be rebuilt later.

namespace Luma::Editor::Panels {

class MaterialEditorPanel {
public:
    MaterialEditorPanel() = default;

    bool Open(const std::filesystem::path& path);
    bool Save();
    bool IsOpen() const { return m_open; }
    const std::filesystem::path& Path() const { return m_path; }
    void MarkDirty() { m_dirty = true; m_previewDirty = true; }

    void Draw(Slate::Context& ui, const Slate::Rect& body, PanelContext& ctx);
    void DrawFloatingPickers(Slate::Context& ui);

private:
    void DrawPreviewViewport(Slate::Context& ui, const Slate::Rect& rect,
                             PanelContext& ctx);
    void DrawDetailsPanel(Slate::Context& ui, const Slate::Rect& rect,
                          PanelContext& ctx);
    void DrawPropertyRows(Slate::Context& ui, const Slate::Rect& rect,
                          PanelContext& ctx);

    bool m_open = false;
    bool m_dirty = false;
    Luma::Material::Material m_material;
    std::filesystem::path m_path;

    // Preview viewport state.
    bool m_previewDirty = true;
    Luma::TextureHandle m_previewTexture = 0;
    u32 m_previewW = 0, m_previewH = 0;
    f32 m_previewYaw = 30.0f, m_previewPitch = 15.0f;
    bool m_previewDragging = false;
    Slate::Rect m_previewRect{};

    // Color picker popups (keyed by MaterialProperty).
    std::unordered_map<Luma::Material::MaterialProperty,
                       Slate::ColorPickerPopup>
        m_colorPickers;

    bool m_detailsMaterialOpen = true;
};

}  // namespace Luma::Editor::Panels
