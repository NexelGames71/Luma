// DrawList primitive tests (no rendering required). Verifies the new
// AddLine / AddCircleFilled / AddRectShadow primitives emit non-degenerate
// geometry and respect the clip stack.

#include <catch2/catch_test_macros.hpp>

#include "Luma/Slate/DrawList.h"

using Luma::Slate::Color;
using Luma::Slate::DrawList;
using Luma::Slate::Rect;
using Luma::Slate::Vec2;

TEST_CASE("DrawList emits a rect-filled with a single command",
          "[slate][draw]") {
    DrawList dl;
    dl.Begin(800, 600);
    dl.AddRectFilled({0, 0, 100, 100}, Color::RGB(255, 0, 0));
    auto& data = dl.Build();
    REQUIRE(data.commandCount == 1);
    REQUIRE(data.indexCount == 6);
    REQUIRE(data.vertexCount == 4);
}

TEST_CASE("AddLine emits two tris for a diagonal stroke", "[slate][draw]") {
    DrawList dl;
    dl.Begin(800, 600);
    dl.AddLine({10, 10}, {50, 50}, Color::RGB(255, 255, 255), 2.0f);
    auto& data = dl.Build();
    REQUIRE(data.indexCount == 6);
    REQUIRE(data.vertexCount == 4);
}

TEST_CASE("AddLine collapses axis-aligned to a single rect", "[slate][draw]") {
    DrawList dl;
    dl.Begin(800, 600);
    dl.AddLine({0, 10}, {100, 10}, Color::RGB(255, 255, 255), 2.0f);
    auto& data = dl.Build();
    // 2 tris per quad = 6 indices, 4 verts.
    REQUIRE(data.indexCount == 6);
    REQUIRE(data.vertexCount == 4);
}

TEST_CASE("AddCircleFilled emits segments - 2 triangles", "[slate][draw]") {
    DrawList dl;
    dl.Begin(800, 600);
    dl.AddCircleFilled({50, 50}, 20.0f, Color::RGB(0, 0, 0), 16);
    auto& data = dl.Build();
    // 14 triangles per fan (16 - 2), 3 indices each.
    REQUIRE(data.indexCount == 14 * 3);
    REQUIRE(data.vertexCount == 16);
}

TEST_CASE("AddRectShadow emits a stack of rounded rects", "[slate][draw]") {
    DrawList dl;
    dl.Begin(800, 600);
    dl.AddRectShadow({10, 10, 100, 50}, 4.0f, 0.5f, 6.0f);
    auto& data = dl.Build();
    // 4 layers, each a rounded-rect fan (~20 verts, ~18 tris).
    REQUIRE(data.commandCount >= 1);
    REQUIRE(data.vertexCount > 4 * 12);
}

TEST_CASE("AddRectShadow with intensity 0 emits nothing",
          "[slate][draw]") {
    DrawList dl;
    dl.Begin(800, 600);
    dl.AddRectShadow({0, 0, 10, 10}, 2.0f, 0.0f, 6.0f);
    auto& data = dl.Build();
    REQUIRE(data.vertexCount == 0);
    REQUIRE(data.commandCount == 0);
}

TEST_CASE("PushClip splits commands when clip rect changes",
          "[slate][draw]") {
    DrawList dl;
    dl.Begin(800, 600);
    dl.AddRectFilled({0, 0, 50, 50}, Color::RGB(255, 0, 0));
    dl.PushClip({100, 100, 50, 50});
    dl.AddRectFilled({0, 0, 50, 50}, Color::RGB(0, 255, 0));
    dl.PopClip();
    auto& data = dl.Build();
    REQUIRE(data.commandCount == 2);
}
