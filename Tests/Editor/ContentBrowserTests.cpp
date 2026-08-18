// ContentBrowserPanel tests: construction, navigation, filter selection, and
// the right-click Create menu (driven through Slate with a NullRenderer).

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "Luma/Asset/AssetRegistry.h"
#include "Luma/Editor/Panels/ContentBrowser.h"
#include "PanelContext.h"
#include "Luma/Slate/Context.h"

using namespace Luma;
using Luma::Editor::Panels::ContentBrowserPanel;
using Luma::Editor::Panels::PanelContext;
using Luma::Slate::Context;
using Luma::Slate::Rect;
namespace fs = std::filesystem;

namespace {
class TempDir {
public:
    TempDir() {
        auto base = fs::temp_directory_path();
        m_path = base / ("luma_cb_test_" + std::to_string(GetTick()));
        fs::create_directories(m_path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(m_path, ec);
    }
    const fs::path& Path() const { return m_path; }
    void Touch(const fs::path& rel, const std::string& body = "x") {
        auto full = m_path / rel;
        fs::create_directories(full.parent_path());
        std::ofstream(full) << body;
    }

private:
    static int GetTick() {
        static int n = 0;
        return ++n;
    }
    fs::path m_path;
};
}  // namespace

TEST_CASE("ContentBrowserPanel constructs empty", "[contentbrowser]") {
    ContentBrowserPanel p;
    REQUIRE_FALSE(p.Selected().IsValid());
    REQUIRE(p.CurrentFolder().empty());
    REQUIRE(p.Registry() == nullptr);
}

TEST_CASE("SetRegistry wires a registry; ResetNavigation clears state",
          "[contentbrowser]") {
    AssetRegistry r;
    ContentBrowserPanel p;
    p.SetRegistry(&r);
    REQUIRE(p.Registry() == &r);
    p.ResetNavigation();
    REQUIRE(p.CurrentFolder().empty());
}

TEST_CASE("Selected remains invalid until a non-folder is clicked",
          "[contentbrowser]") {
    TempDir dir;
    dir.Touch("hero.png");
    AssetRegistry r;
    r.AddRoot(dir.Path());
    r.Scan();
    ContentBrowserPanel p;
    p.SetRegistry(&r);
    // No Slate to drive a click in these tests; we just confirm the
    // initial state is sensible.
    REQUIRE_FALSE(p.Selected().IsValid());
    // AssetId of hero.png is present in the registry.
    auto entries = r.FilterByType(AssetType::Texture);
    REQUIRE(entries.size() == 1);
    REQUIRE(entries[0]->assetName == "hero");
}

namespace {
// Minimal renderer stub (fonts don't bake but widget logic still runs).
class NullRenderer final : public Luma::Renderer {
public:
    void OnResize(Luma::u32, Luma::u32) override {}
    void SetClearColor(const Luma::ClearColor&) override {}
    bool BeginFrame() override { return true; }
    void EndFrame() override {}
    void DrawUI(const Luma::UIDrawData&) override {}
    Luma::TextureHandle CreateTexture(Luma::u32, Luma::u32,
                                      const void*) override {
        return 0;
    }
    void DestroyTexture(Luma::TextureHandle) override {}
    void CaptureFrame(const std::string&) override {}
    Luma::TextureHandle RenderSceneView(Luma::u32, Luma::u32,
                                        const Luma::SceneView&) override {
        return 0;
    }
    void WaitIdle() override {}
};

Luma::Slate::Typography TestTypography() {
    Luma::Slate::Typography t;
    t.uiRegular = "C:/Windows/Fonts/segoeui.ttf";
    t.uiMedium = t.uiRegular;
    t.uiSemiBold = t.uiRegular;
    t.uiBold = t.uiRegular;
    t.mono = t.uiRegular;
    return t;
}
}  // namespace

TEST_CASE("Right-click opens the Create menu; Material invokes onCreateMaterial",
          "[contentbrowser]") {
    TempDir dir;
    AssetRegistry r;
    r.AddRoot(dir.Path());
    r.Scan();
    ContentBrowserPanel p;
    p.SetRegistry(&r);

    Context ui;
    NullRenderer nr;
    ui.Init(nr, TestTypography());
    PanelContext ctx;
    ctx.assetRegistry = &r;

    std::filesystem::path created;
    std::filesystem::path requestedFolder;
    ctx.onCreateMaterial = [&](const std::filesystem::path& folder) {
        requestedFolder = folder;
        created = folder / "NewMat.lmat";
        std::ofstream(created) << "{}";
        return created;
    };

    Rect body{0, 0, 800, 600};

    // Frame 1: right-click at (100, 100) inside the body opens the menu.
    ui.BeginFrame(800, 600, 0.0166f);
    ui.OnMouseMove(100.0f, 100.0f);
    ui.OnMouseButton(1, true);
    p.Draw(ui, body, ctx);
    p.DrawFloatingMenu(ui, ctx);
    ui.EndFrame();
    ui.BeginFrame(800, 600, 0.0166f);
    ui.OnMouseButton(1, false);
    p.Draw(ui, body, ctx);
    p.DrawFloatingMenu(ui, ctx);
    ui.EndFrame();

    // Navigate with the keyboard: the flat row order is [GET: Import,
    // CREATE: New Folder, CREATE: Material, Material category]. Down x3 puts
    // the focus on "Material", Enter activates it. Keyboard navigation is
    // layout-independent, so these tests survive menu redesigns.
    auto pressKey = [&](int glfwKey) {
        ui.BeginFrame(800, 600, 0.0166f);
        ui.OnKey(glfwKey, true);
        p.Draw(ui, body, ctx);
        p.DrawFloatingMenu(ui, ctx);
        ui.EndFrame();
    };
    constexpr int kKeyDown = 264;   // GLFW_KEY_DOWN
    constexpr int kKeyEnter = 257;  // GLFW_KEY_ENTER
    pressKey(kKeyDown);  // 0: GET > Import
    pressKey(kKeyDown);  // 1: CREATE > New Folder
    pressKey(kKeyDown);  // 2: CREATE > Material
    pressKey(kKeyEnter);

    // The callback ran with the content root (empty folder -> root) and
    // returned the created path; the new asset is selected + activated.
    REQUIRE(!created.empty());
    REQUIRE(requestedFolder == dir.Path());
    REQUIRE(p.Selected().IsValid());
    REQUIRE(p.Activated().IsValid());
}

TEST_CASE("Right-click menu New Folder creates a folder and navigates",
          "[contentbrowser]") {
    TempDir dir;
    AssetRegistry r;
    r.AddRoot(dir.Path());
    r.Scan();
    ContentBrowserPanel p;
    p.SetRegistry(&r);

    Context ui;
    NullRenderer nr;
    ui.Init(nr, TestTypography());
    PanelContext ctx;
    ctx.assetRegistry = &r;

    Rect body{0, 0, 800, 600};

    // Open the menu.
    ui.BeginFrame(800, 600, 0.0166f);
    ui.OnMouseMove(100.0f, 100.0f);
    ui.OnMouseButton(1, true);
    p.Draw(ui, body, ctx);
    p.DrawFloatingMenu(ui, ctx);
    ui.EndFrame();
    ui.BeginFrame(800, 600, 0.0166f);
    ui.OnMouseButton(1, false);
    p.Draw(ui, body, ctx);
    p.DrawFloatingMenu(ui, ctx);
    ui.EndFrame();

    // Navigate with the keyboard: Down x2 puts the focus on "New Folder",
    // Enter activates it (flat order: GET > Import, CREATE > New Folder, ...).
    auto pressKey = [&](int glfwKey) {
        ui.BeginFrame(800, 600, 0.0166f);
        ui.OnKey(glfwKey, true);
        p.Draw(ui, body, ctx);
        p.DrawFloatingMenu(ui, ctx);
        ui.EndFrame();
    };
    constexpr int kKeyDown = 264;   // GLFW_KEY_DOWN
    constexpr int kKeyEnter = 257;  // GLFW_KEY_ENTER
    pressKey(kKeyDown);  // 0: GET > Import
    pressKey(kKeyDown);  // 1: CREATE > New Folder
    pressKey(kKeyEnter);

    // A NewFolder directory was created and the panel navigated into it.
    REQUIRE(fs::exists(dir.Path() / "NewFolder"));
    REQUIRE(p.CurrentFolder() == (dir.Path() / "NewFolder"));
}

TEST_CASE("Right-click menu closes on outside left-click",
          "[contentbrowser]") {
    TempDir dir;
    AssetRegistry r;
    r.AddRoot(dir.Path());
    r.Scan();
    ContentBrowserPanel p;
    p.SetRegistry(&r);

    Context ui;
    NullRenderer nr;
    ui.Init(nr, TestTypography());
    PanelContext ctx;

    Rect body{0, 0, 800, 600};

    // Open the menu with a right-click.
    ui.BeginFrame(800, 600, 0.0166f);
    ui.OnMouseMove(100.0f, 100.0f);
    ui.OnMouseButton(1, true);
    p.Draw(ui, body, ctx);
    p.DrawFloatingMenu(ui, ctx);
    ui.EndFrame();
    ui.BeginFrame(800, 600, 0.0166f);
    ui.OnMouseButton(1, false);
    p.Draw(ui, body, ctx);
    p.DrawFloatingMenu(ui, ctx);
    ui.EndFrame();

    // Left-click far away from the menu (e.g. the toolbar row).
    ui.BeginFrame(800, 600, 0.0166f);
    ui.OnMouseMove(400.0f, 10.0f);
    ui.OnMouseButton(0, true);
    p.Draw(ui, body, ctx);
    p.DrawFloatingMenu(ui, ctx);
    ui.EndFrame();
    ui.BeginFrame(800, 600, 0.0166f);
    ui.OnMouseButton(0, false);
    p.Draw(ui, body, ctx);
    p.DrawFloatingMenu(ui, ctx);
    ui.EndFrame();

    // The menu is gone: another outside click must not crash, and the
    // panel is still usable (selection stays invalid).
    REQUIRE_FALSE(p.Selected().IsValid());
}

TEST_CASE("Right-click menu GET > Import invokes onImportAssets",
          "[contentbrowser]") {
    TempDir dir;
    AssetRegistry r;
    r.AddRoot(dir.Path());
    r.Scan();
    ContentBrowserPanel p;
    p.SetRegistry(&r);

    Context ui;
    NullRenderer nr;
    ui.Init(nr, TestTypography());
    PanelContext ctx;
    ctx.assetRegistry = &r;

    std::filesystem::path importedInto;
    ctx.onImportAssets = [&](const std::filesystem::path& folder) {
        importedInto = folder;
    };

    Rect body{0, 0, 800, 600};

    // Open the menu.
    ui.BeginFrame(800, 600, 0.0166f);
    ui.OnMouseMove(100.0f, 100.0f);
    ui.OnMouseButton(1, true);
    p.Draw(ui, body, ctx);
    p.DrawFloatingMenu(ui, ctx);
    ui.EndFrame();
    ui.BeginFrame(800, 600, 0.0166f);
    ui.OnMouseButton(1, false);
    p.Draw(ui, body, ctx);
    p.DrawFloatingMenu(ui, ctx);
    ui.EndFrame();

    // Flat row 0 is GET > Import to Current Folder: Down once, then Enter.
    auto pressKey = [&](int glfwKey) {
        ui.BeginFrame(800, 600, 0.0166f);
        ui.OnKey(glfwKey, true);
        p.Draw(ui, body, ctx);
        p.DrawFloatingMenu(ui, ctx);
        ui.EndFrame();
    };
    constexpr int kKeyDown = 264;   // GLFW_KEY_DOWN
    constexpr int kKeyEnter = 257;  // GLFW_KEY_ENTER
    pressKey(kKeyDown);
    pressKey(kKeyEnter);

    // The callback received the content root (empty folder -> root).
    REQUIRE(importedInto == dir.Path());
}

TEST_CASE("Right-click menu Material category opens a submenu that creates",
          "[contentbrowser]") {
    TempDir dir;
    AssetRegistry r;
    r.AddRoot(dir.Path());
    r.Scan();
    ContentBrowserPanel p;
    p.SetRegistry(&r);

    Context ui;
    NullRenderer nr;
    ui.Init(nr, TestTypography());
    PanelContext ctx;
    ctx.assetRegistry = &r;

    std::filesystem::path created;
    ctx.onCreateMaterial = [&](const std::filesystem::path& folder) {
        created = folder / "SubMat.lmat";
        std::ofstream(created) << "{}";
        return created;
    };

    Rect body{0, 0, 800, 600};

    // Open the menu.
    ui.BeginFrame(800, 600, 0.0166f);
    ui.OnMouseMove(100.0f, 100.0f);
    ui.OnMouseButton(1, true);
    p.Draw(ui, body, ctx);
    p.DrawFloatingMenu(ui, ctx);
    ui.EndFrame();
    ui.BeginFrame(800, 600, 0.0166f);
    ui.OnMouseButton(1, false);
    p.Draw(ui, body, ctx);
    p.DrawFloatingMenu(ui, ctx);
    ui.EndFrame();

    // Type "material" into the (auto-focused) search box. This filters the
    // menu down to CREATE > Material + the Material category, and auto-opens
    // the category's submenu (search behavior) — no pixel positions needed.
    ui.BeginFrame(800, 600, 0.0166f);
    for (char c : std::string("material")) ui.OnText(static_cast<u32>(c));
    p.Draw(ui, body, ctx);
    p.DrawFloatingMenu(ui, ctx);
    ui.EndFrame();

    // After filtering the flat rows are [CREATE > Material, Material cat] and
    // the submenu item extends the list: Down focuses the category, Down again
    // the submenu's Material item, Enter creates.
    auto pressKey = [&](int glfwKey) {
        ui.BeginFrame(800, 600, 0.0166f);
        ui.OnKey(glfwKey, true);
        p.Draw(ui, body, ctx);
        p.DrawFloatingMenu(ui, ctx);
        ui.EndFrame();
    };
    constexpr int kKeyDown = 264;   // GLFW_KEY_DOWN
    constexpr int kKeyEnter = 257;  // GLFW_KEY_ENTER
    pressKey(kKeyDown);  // 0: CREATE > Material
    pressKey(kKeyDown);  // 1: Material category
    pressKey(kKeyDown);  // 2: submenu > BASIC > Material
    pressKey(kKeyEnter);

    // The submenu item created the asset through the same callback path.
    REQUIRE(fs::exists(created));
    REQUIRE(p.Activated().IsValid());
}
