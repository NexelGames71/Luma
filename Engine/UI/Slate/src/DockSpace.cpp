#include "Luma/Slate/DockSpace.h"

#include <cmath>

namespace Luma::Slate {
namespace {
constexpr f32 kTabBarH = 26.0f;

DockDir ZoneAt(const Rect& r, Vec2 p) {
    f32 fx = (p.x - r.x) / r.w;
    f32 fy = (p.y - r.y) / r.h;
    if (fx > 0.33f && fx < 0.67f && fy > 0.33f && fy < 0.67f) {
        return DockDir::Center;
    }
    f32 dl = fx, dr = 1.0f - fx, dt = fy, db = 1.0f - fy;
    f32 m = std::min(std::min(dl, dr), std::min(dt, db));
    if (m == dl) return DockDir::Left;
    if (m == dr) return DockDir::Right;
    if (m == dt) return DockDir::Up;
    return DockDir::Down;
}

Rect ZoneRect(const Rect& r, DockDir dir) {
    switch (dir) {
        case DockDir::Left:  return {r.x, r.y, r.w * 0.5f, r.h};
        case DockDir::Right: return {r.x + r.w * 0.5f, r.y, r.w * 0.5f, r.h};
        case DockDir::Up:    return {r.x, r.y, r.w, r.h * 0.5f};
        case DockDir::Down:  return {r.x, r.y + r.h * 0.5f, r.w, r.h * 0.5f};
        default:             return r;
    }
}
}  // namespace

void DockSpace::AddPanel(const std::string& id, const std::string& title,
                         PanelDrawer drawer) {
    m_panels[id] = PanelInfo{title, std::move(drawer)};
}

void DockSpace::DockRoot(const std::string& id) {
    m_root = std::make_unique<Node>();
    m_root->isLeaf = true;
    m_root->tabs = {id};
}

DockSpace::Node* DockSpace::FindLeafWithPanel(Node* n,
                                              const std::string& id) const {
    if (!n) return nullptr;
    if (n->isLeaf) {
        for (const auto& t : n->tabs)
            if (t == id) return n;
        return nullptr;
    }
    if (Node* r = FindLeafWithPanel(n->a.get(), id)) return r;
    return FindLeafWithPanel(n->b.get(), id);
}

DockSpace::Node* DockSpace::LeafUnderPoint(Node* n, Vec2 p) const {
    if (!n) return nullptr;
    if (n->isLeaf) return n->rect.Contains(p) ? n : nullptr;
    if (Node* r = LeafUnderPoint(n->a.get(), p)) return r;
    return LeafUnderPoint(n->b.get(), p);
}

std::unique_ptr<DockSpace::Node>* DockSpace::FindSlot(
    std::unique_ptr<Node>& slot, Node* target) {
    if (!slot) return nullptr;
    if (slot.get() == target) return &slot;
    if (slot->isLeaf) return nullptr;
    if (auto* r = FindSlot(slot->a, target)) return r;
    return FindSlot(slot->b, target);
}

void DockSpace::DockWith(const std::string& id, const std::string& targetId,
                         DockDir dir, f32 ratio) {
    Node* target = FindLeafWithPanel(m_root.get(), targetId);
    if (!target) {
        if (!m_root)
            DockRoot(id);
        else
            m_root->tabs.push_back(id);
        return;
    }
    if (dir == DockDir::Center) {
        target->tabs.push_back(id);
        target->active = static_cast<int>(target->tabs.size()) - 1;
        return;
    }
    std::unique_ptr<Node>* slot = FindSlot(m_root, target);
    if (slot) SplitSlot(*slot, dir, id, ratio);
}

void DockSpace::SplitSlot(std::unique_ptr<Node>& slot, DockDir dir,
                          const std::string& panelId, f32 ratio) {
    auto leaf = std::make_unique<Node>();
    leaf->isLeaf = true;
    leaf->tabs = {panelId};

    auto split = std::make_unique<Node>();
    split->isLeaf = false;
    split->horizontal = (dir == DockDir::Left || dir == DockDir::Right);
    bool newFirst = (dir == DockDir::Left || dir == DockDir::Up);
    split->ratio = newFirst ? ratio : (1.0f - ratio);
    if (newFirst) {
        split->a = std::move(leaf);
        split->b = std::move(slot);
    } else {
        split->a = std::move(slot);
        split->b = std::move(leaf);
    }
    slot = std::move(split);
}

bool DockSpace::Cleanup(std::unique_ptr<Node>& slot) {
    if (!slot) return true;
    if (slot->isLeaf) return slot->tabs.empty();
    bool aEmpty = Cleanup(slot->a);
    bool bEmpty = Cleanup(slot->b);
    if (aEmpty && bEmpty) {
        slot.reset();
        return true;
    }
    if (aEmpty) {
        slot = std::move(slot->b);
        return false;
    }
    if (bEmpty) {
        slot = std::move(slot->a);
        return false;
    }
    return false;
}

void DockSpace::RemovePanel(const std::string& id) {
    Node* leaf = FindLeafWithPanel(m_root.get(), id);
    if (!leaf) return;
    for (usize i = 0; i < leaf->tabs.size(); ++i) {
        if (leaf->tabs[i] == id) {
            leaf->tabs.erase(leaf->tabs.begin() + static_cast<isize>(i));
            break;
        }
    }
    if (leaf->active >= static_cast<int>(leaf->tabs.size())) {
        leaf->active = static_cast<int>(leaf->tabs.size()) - 1;
    }
    Cleanup(m_root);
}

void DockSpace::ApplyPending() {
    if (!m_pending.active) return;
    m_pending.active = false;

    // Re-find the target leaf by one of its panels (pointer may be stale after
    // tree edits) - capture a stable target panel id now.
    Node* target = m_pending.target;
    std::string targetPanel;
    if (target && !target->tabs.empty()) targetPanel = target->tabs[0];

    // No-op: dropping a single-tab leaf onto itself.
    if (target && target->tabs.size() == 1 && target->tabs[0] == m_dragPanel) {
        return;
    }

    RemovePanel(m_dragPanel);
    if (!m_root) {
        DockRoot(m_dragPanel);
        return;
    }
    Node* tgt = targetPanel.empty()
                    ? nullptr
                    : FindLeafWithPanel(m_root.get(), targetPanel);
    if (!tgt) return;
    if (m_pending.dir == DockDir::Center) {
        tgt->tabs.push_back(m_dragPanel);
        tgt->active = static_cast<int>(tgt->tabs.size()) - 1;
    } else {
        std::unique_ptr<Node>* slot = FindSlot(m_root, tgt);
        if (slot) SplitSlot(*slot, m_pending.dir, m_dragPanel, 0.5f);
    }
}

void DockSpace::DrawLeaf(Context& ctx, Node* n) {
    Theme& t = ctx.theme();
    Rect rect = n->rect;
    ctx.Panel(rect, t.panelBg);
    ctx.Panel({rect.x, rect.y, rect.w, kTabBarH}, t.header);

    // Tabs.
    f32 tx = rect.x;
    for (int i = 0; i < static_cast<int>(n->tabs.size()); ++i) {
        const std::string& id = n->tabs[static_cast<usize>(i)];
        auto it = m_panels.find(id);
        std::string title = it != m_panels.end() ? it->second.title : id;
        f32 tabW = ctx.font().Measure(title).x + 28.0f;
        Rect tab{tx, rect.y, tabW, kTabBarH};
        bool active = (i == n->active);
        bool hovered = tab.Contains(ctx.mouse());
        if (active) {
            ctx.Panel(tab, t.panelBg);
            ctx.Panel({tab.x, tab.Bottom() - 2.0f, tab.w, 2.0f}, t.accent);
        } else if (hovered) {
            ctx.Panel(tab, t.buttonHover);
        }
        ctx.LabelIn({tab.x + 12.0f, tab.y, tabW - 16.0f, kTabBarH}, title,
                    active ? t.text : t.textDim);
        if (hovered) {
            if (ctx.mousePressed(0)) {
                n->active = i;
                m_dragPanel = id;
                m_dragging = false;
                m_dragStart = ctx.mouse();
            }
            ctx.RequestCursor(CursorShape::Hand);
        }
        tx += tabW;
    }
    ctx.Panel({rect.x, rect.y + kTabBarH, rect.w, 1.0f}, t.panelBorder);

    // Content.
    Rect content{rect.x, rect.y + kTabBarH + 1.0f, rect.w,
                 rect.h - kTabBarH - 1.0f};
    if (n->active >= 0 && n->active < static_cast<int>(n->tabs.size())) {
        auto it = m_panels.find(n->tabs[static_cast<usize>(n->active)]);
        if (it != m_panels.end() && it->second.drawer) {
            ctx.PushClip(content);
            it->second.drawer(ctx, content);
            ctx.PopClip();
        }
    }
}

void DockSpace::DrawNode(Context& ctx, Node* n) {
    if (!n) return;
    if (n->isLeaf) {
        DrawLeaf(ctx, n);
        return;
    }
    Rect aRect, bRect;
    u64 sid = static_cast<u64>(reinterpret_cast<uptr>(n));
    bool dragging;
    if (n->horizontal) {
        dragging = ctx.SplitterV(sid, n->rect, n->ratio, aRect, bRect);
    } else {
        dragging = ctx.SplitterH(sid, n->rect, n->ratio, aRect, bRect);
    }
    n->a->rect = aRect;
    n->b->rect = bRect;
    DrawNode(ctx, n->a.get());
    DrawNode(ctx, n->b.get());

    // Separator drawn on top of the panels (they fill their rects and would
    // otherwise cover the splitter's own line). Accent while hovered/dragging.
    Theme& t = ctx.theme();
    Vec2 m = ctx.mouse();
    if (n->horizontal) {
        f32 sx = bRect.x;
        bool hovered = m.x >= sx - 4.0f && m.x <= sx + 4.0f &&
                       m.y >= n->rect.y && m.y <= n->rect.Bottom();
        ctx.Panel({sx - 0.5f, n->rect.y, 1.0f, n->rect.h},
                  (hovered || dragging) ? t.accent : t.panelBorder);
    } else {
        f32 sy = bRect.y;
        bool hovered = m.y >= sy - 4.0f && m.y <= sy + 4.0f &&
                       m.x >= n->rect.x && m.x <= n->rect.Right();
        ctx.Panel({n->rect.x, sy - 0.5f, n->rect.w, 1.0f},
                  (hovered || dragging) ? t.accent : t.panelBorder);
    }
}

void DockSpace::Draw(Context& ctx, const Rect& area) {
    if (!m_root) return;
    m_root->rect = area;
    DrawNode(ctx, m_root.get());

    // Tab drag: promote to dragging after a small threshold.
    if (!m_dragPanel.empty() && ctx.isMouseDown(0)) {
        Vec2 m = ctx.mouse();
        if (!m_dragging) {
            f32 dx = m.x - m_dragStart.x, dy = m.y - m_dragStart.y;
            if (dx * dx + dy * dy > 36.0f) m_dragging = true;
        }
        if (m_dragging) {
            ctx.RequestCursor(CursorShape::Hand);
            Node* target = LeafUnderPoint(m_root.get(), m);
            if (target) {
                DockDir dir = ZoneAt(target->rect, m);
                Rect zr = ZoneRect(target->rect, dir);
                // Drop-zone overlay.
                ctx.Panel(zr, Color::RGBA(60, 140, 220, 90));
                ctx.PanelBordered(zr, Color::RGBA(0, 0, 0, 0), ctx.theme().accent,
                                  2.0f);
                m_pending = PendingDock{true, m_dragPanel, target, dir};
            } else {
                m_pending.active = false;
            }
            // A floating "ghost" label following the cursor.
            auto it = m_panels.find(m_dragPanel);
            std::string title = it != m_panels.end() ? it->second.title
                                                     : m_dragPanel;
            f32 gw = ctx.font().Measure(title).x + 24.0f;
            Rect ghost{m.x + 12.0f, m.y + 8.0f, gw, 24.0f};
            ctx.Panel(ghost, ctx.theme().button);
            ctx.LabelIn(ghost, title, ctx.theme().text, Align::Center);
        }
    } else {
        // Mouse released.
        if (m_dragging) ApplyPending();
        m_dragPanel.clear();
        m_dragging = false;
        m_pending.active = false;
    }
}

}  // namespace Luma::Slate
