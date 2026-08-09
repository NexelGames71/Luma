#include <catch2/catch_test_macros.hpp>

#include "Luma/Core/Config.h"

namespace {
constexpr const char* kSample = R"(
# Luma sample config
[Window]
title = Luma Editor
width = 1280
height = 720
fullscreen = false
vsync = on

[Renderer]
gpu_index = 0
render_scale = 1.5
)";
}  // namespace

TEST_CASE("Config parses sections into qualified keys", "[core][config]") {
    Luma::Config cfg;
    REQUIRE(cfg.LoadFromString(kSample));

    REQUIRE(cfg.Has("Window.title"));
    REQUIRE(cfg.GetString("Window.title") == "Luma Editor");
    REQUIRE(cfg.GetInt("Window.width") == 1280);
    REQUIRE(cfg.GetInt("Window.height") == 720);
    REQUIRE(cfg.GetBool("Window.fullscreen") == false);
    REQUIRE(cfg.GetBool("Window.vsync") == true);
    REQUIRE(cfg.GetInt("Renderer.gpu_index") == 0);
    REQUIRE(cfg.GetFloat("Renderer.render_scale") == 1.5f);
}

TEST_CASE("Config returns fallbacks for missing or unparseable keys",
          "[core][config]") {
    Luma::Config cfg;
    cfg.LoadFromString("[A]\nname = hello\ncount = notanumber\n");

    REQUIRE(cfg.GetString("A.missing", "default") == "default");
    REQUIRE(cfg.GetInt("A.missing", 42) == 42);
    REQUIRE(cfg.GetInt("A.count", 7) == 7);  // unparseable -> fallback
    REQUIRE(cfg.GetFloat("A.missing", 3.25f) == 3.25f);
    REQUIRE(cfg.GetBool("A.missing", true) == true);
}

TEST_CASE("Config ignores comments and blank lines", "[core][config]") {
    Luma::Config cfg;
    REQUIRE(cfg.LoadFromString("; comment\n\n# another\nx = 1\n"));
    REQUIRE(cfg.Size() == 1);
    REQUIRE(cfg.GetInt("x") == 1);
}
