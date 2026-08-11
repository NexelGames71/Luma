#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Luma/Core/Types.h"
#include "Luma/Slate/Context.h"
#include "Luma/Slate/Types.h"

// Luma::Slate::DockSpace - a dockable panel layout. Panels are registered with a
// draw callback and arranged in a tree of split nodes and tabbed leaves. Tabs
// can be dragged to re-dock (tabify onto another panel, or split a region on any
// side). Its own Slate feature, in its own files.

namespace Luma::Slate {

enum class DockDir { Center, Left, Right, Up, Down };

class DockSpace {
public:
    using PanelDrawer = std::function<void(Context&, const Rect&)>;

    // Registers a panel and its content drawer.
    void AddPanel(const std::string& id, const std::string& title,
                  PanelDrawer drawer);

    // --- Layout builder (also used by drag-docking) ------------------------
    void DockRoot(const std::string& id);
    // Splits the region containing `targetId`, placing `id` on `dir`; `ratio` is
    // the fraction given to the side `id` lands on.
    void DockWith(const std::string& id, const std::string& targetId,
                  DockDir dir, f32 ratio);

    // Lays out + draws the whole dock space (tab bars, splitters, panels) and
    // handles tab drag-docking.
    void Draw(Context& ctx, const Rect& area);

private:
    struct Node {
        bool isLeaf = true;
        // Leaf:
        std::vector<std::string> tabs;
        int active = 0;
        // Split:
        bool horizontal = true;  // true: side-by-side (vertical divider)
        f32 ratio = 0.5f;
        std::unique_ptr<Node> a, b;
        // Computed each frame:
        Rect rect{};
    };

    struct PanelInfo {
        std::string title;
        PanelDrawer drawer;
    };

    struct PendingDock {
        bool active = false;
        std::string panel;
        Node* target = nullptr;
        DockDir dir = DockDir::Center;
        bool edge = false;  // dock against the whole workspace edge (split root)
    };

    Node* FindLeafWithPanel(Node* n, const std::string& id) const;
    Node* LeafUnderPoint(Node* n, Vec2 p) const;
    std::unique_ptr<Node>* FindSlot(std::unique_ptr<Node>& slot, Node* target);
    void RemovePanel(const std::string& id);
    static bool Cleanup(std::unique_ptr<Node>& slot);
    static void SplitSlot(std::unique_ptr<Node>& slot, DockDir dir,
                          const std::string& panelId, f32 ratio);

    void DrawNode(Context& ctx, Node* n);
    void DrawLeaf(Context& ctx, Node* n);
    void ApplyPending();

    std::unordered_map<std::string, PanelInfo> m_panels;
    std::unique_ptr<Node> m_root;

    // Tab drag state.
    std::string m_dragPanel;
    bool m_dragging = false;
    Vec2 m_dragStart;
    PendingDock m_pending;
};

}  // namespace Luma::Slate
