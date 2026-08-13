// ContentBrowserPanel smoke tests: construction, navigation, filter
// selection. Doesn't drive Slate — we test the model side only.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "Luma/Asset/AssetRegistry.h"
#include "Luma/Editor/Panels/ContentBrowser.h"

using namespace Luma;
using Luma::Editor::Panels::ContentBrowserPanel;
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
