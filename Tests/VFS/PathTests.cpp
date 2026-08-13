#include <catch2/catch_test_macros.hpp>

#include <string>

#include "Luma/VFS/Path.h"

using namespace Luma;
using namespace Luma::VFS;

TEST_CASE("Path::Path() is empty", "[vfs][path]") {
    Path p;
    REQUIRE(p.IsEmpty());
    REQUIRE(p.ToString().empty());
}

TEST_CASE("NormalizeRelative collapses slashes, '.', '..'", "[vfs][path][normalize]") {
    REQUIRE(NormalizeRelative("") == "");
    REQUIRE(NormalizeRelative(".") == "");
    REQUIRE(NormalizeRelative("./") == "");
    REQUIRE(NormalizeRelative("a/b/c") == "a/b/c");
    REQUIRE(NormalizeRelative("a//b///c") == "a/b/c");
    REQUIRE(NormalizeRelative("a/./b/./c") == "a/b/c");
    REQUIRE(NormalizeRelative("a/b/../c") == "a/c");
    REQUIRE(NormalizeRelative("a/b/c/../../d") == "a/d");
    REQUIRE(NormalizeRelative("..") == "");  // "above root" normalizes to empty
    REQUIRE(NormalizeRelative("../a") == "a");
    REQUIRE(NormalizeRelative("a/b/../..") == "");  // above root
    REQUIRE(NormalizeRelative("a\\b\\c") == "a/b/c");
}

TEST_CASE("NormalizeRelative rejects invalid characters", "[vfs][path][normalize]") {
    REQUIRE(NormalizeRelative("a/b:c") == "");   // ':' forbidden
    REQUIRE(NormalizeRelative("a/b*") == "");    // '*' forbidden
    REQUIRE(NormalizeRelative("a/b?") == "");    // '?' forbidden
    REQUIRE(NormalizeRelative("a/b<c") == "");   // '<' forbidden
    REQUIRE(NormalizeRelative("a/b|d") == "");   // '|' forbidden
    REQUIRE(NormalizeRelative("a/\"b") == "");   // '"' forbidden
    REQUIRE(NormalizeRelative(std::string("a/\x01b")) == "");  // control char
}

TEST_CASE("Path virtual address parses with root prefix", "[vfs][path][parse]") {
    Path p(std::string_view("Engine:/Rendering/Vulkan/shaders/scene.frag"));
    REQUIRE(p.GetRoot() == Root::Engine);
    REQUIRE(p.Relative() == "Rendering/Vulkan/shaders/scene.frag");
    REQUIRE(p.ToString() == "Engine:/Rendering/Vulkan/shaders/scene.frag");
    REQUIRE_FALSE(p.IsAbsolute());
}

TEST_CASE("Path virtual address accepts backslash separator", "[vfs][path][parse]") {
    Path p(std::string_view("Project:\\Content\\Scenes\\Main.lscene"));
    REQUIRE(p.GetRoot() == Root::Project);
    REQUIRE(p.Relative() == "Content/Scenes/Main.lscene");
}

TEST_CASE("Path without root prefix is absolute", "[vfs][path][parse]") {
    Path p(std::string_view("C:/foo/bar.txt"));
    REQUIRE(p.IsAbsolute());
    REQUIRE(p.GetRoot() == Root::Absolute);
#ifdef _WIN32
    REQUIRE(p.Absolute().string() == "C:/foo/bar.txt");
#endif
}

TEST_CASE("Path(Root, relative) normalizes on construction", "[vfs][path][ctor]") {
    Path p(Root::Saved, "Logs/./Engine.log");
    REQUIRE(p.GetRoot() == Root::Saved);
    REQUIRE(p.Relative() == "Logs/Engine.log");
}

TEST_CASE("Path equality and inequality", "[vfs][path][eq]") {
    Path a(Root::Project, "Content/Foo.lscene");
    Path b(Root::Project, "Content/Foo.lscene");
    Path c(Root::Project, "Content/Bar.lscene");
    Path d(Root::Saved, "Content/Foo.lscene");
    REQUIRE(a == b);
    REQUIRE_FALSE(a == c);
    REQUIRE_FALSE(a == d);
    REQUIRE(a != c);
}

TEST_CASE("Root <-> string round trip", "[vfs][path][root]") {
    for (Root r : {Root::Engine, Root::Config, Root::Content, Root::Project,
                   Root::Saved, Root::Intermediate, Root::Absolute}) {
        auto s = ToString(r);
        REQUIRE(RootFromString(s) == r);
    }
    REQUIRE(RootFromString("nope") == Root::Absolute);
}
