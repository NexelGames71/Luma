#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include "Luma/VFS/VFS.h"

using namespace Luma;
namespace VFSx = Luma::VFS;
using VFSx::Path;
using VFSx::Root;
namespace fs = std::filesystem;

namespace {

// Creates a unique temporary directory under the system temp and returns it.
// The directory is auto-cleaned when the test exits via the scope guard.
struct TempDir {
    fs::path path;

    TempDir() {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<u64> dist;
        auto base = fs::temp_directory_path();
        for (int i = 0; i < 8; ++i) {
            fs::path candidate = base / ("luma_vfs_" + std::to_string(dist(gen)));
            std::error_code ec;
            if (fs::create_directory(candidate, ec)) {
                path = candidate;
                return;
            }
        }
        // Last-ditch: let the system pick.
        path = base / ("luma_vfs_" + std::to_string(dist(gen)));
        fs::create_directories(path);
    }

    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
};

}  // namespace

TEST_CASE("VFS mount/IsMounted/RootPath", "[vfs][mount]") {
    TempDir tmp;
    VFSx::VFS vfs;
    REQUIRE_FALSE(vfs.IsMounted(Root::Engine));
    vfs.Mount(Root::Engine, tmp.path);
    REQUIRE(vfs.IsMounted(Root::Engine));
    REQUIRE(vfs.RootPath(Root::Engine) == fs::weakly_canonical(tmp.path));
    vfs.Unmount(Root::Engine);
    REQUIRE_FALSE(vfs.IsMounted(Root::Engine));
}

TEST_CASE("VFS resolve virtual path under mounted root", "[vfs][resolve]") {
    TempDir tmp;
    VFSx::VFS vfs;
    vfs.Mount(Root::Project, tmp.path);
    Path p(Root::Project, "Content/Scenes/Main.lscene");
    auto real = vfs.TryResolve(p);
    REQUIRE(real.has_value());
    REQUIRE(*real == fs::weakly_canonical(tmp.path) / "Content" / "Scenes" / "Main.lscene");
}

TEST_CASE("VFS resolve fails when root unmounted", "[vfs][resolve]") {
    VFSx::VFS vfs;
    Path p(Root::Project, "foo.txt");
    REQUIRE_FALSE(vfs.TryResolve(p).has_value());
    REQUIRE_THROWS(vfs.Resolve(p));
}

TEST_CASE("VFS resolve absolute path is pass-through", "[vfs][resolve][absolute]") {
    VFSx::VFS vfs;
    Path p(fs::temp_directory_path() / "luma_vfs_test_abs.txt");
    auto real = vfs.TryResolve(p);
    REQUIRE(real.has_value());
    REQUIRE(*real == fs::temp_directory_path() / "luma_vfs_test_abs.txt");
}

TEST_CASE("VFS escape via '..' is normalized away (no escape)", "[vfs][resolve][escape]") {
    TempDir tmp;
    VFSx::VFS vfs;
    vfs.Mount(Root::Project, tmp.path);
    // ".." gets normalized out of the relative part by Path's ctor, so the
    // resolved path stays inside the project root.
    Path p(Root::Project, "../etc/passwd");
    REQUIRE(p.Relative() == "etc/passwd");
    auto real = vfs.TryResolve(p);
    REQUIRE(real.has_value());
    REQUIRE(*real == fs::weakly_canonical(tmp.path) / "etc" / "passwd");
}

TEST_CASE("VFS write + read file round trip", "[vfs][io]") {
    TempDir tmp;
    VFSx::VFS vfs;
    vfs.Mount(Root::Project, tmp.path);

    const std::string content = "hello luma\n";
    Path out(Root::Project, "Saved/hello.txt");
    REQUIRE(vfs.WriteText(out, content));
    REQUIRE(vfs.IsFile(out));
    REQUIRE(vfs.Exists(out));
    auto read = vfs.ReadText(out);
    REQUIRE(read.has_value());
    REQUIRE(*read == content);
}

TEST_CASE("VFS ReadFile handles binary data", "[vfs][io][binary]") {
    TempDir tmp;
    VFSx::VFS vfs;
    vfs.Mount(Root::Engine, tmp.path);

    std::vector<u8> data{0x00, 0xFF, 0x10, 0x20, 0x30, 0xCA, 0xFE, 0xBA, 0xBE};
    Path bin(Root::Engine, "sub/dir/blob.bin");
    REQUIRE(vfs.WriteFile(bin, data));

    std::vector<u8> read;
    REQUIRE(vfs.ReadFile(bin, read));
    REQUIRE(read == data);
}

TEST_CASE("VFS CreateDirectories mkdir-p", "[vfs][io][mkdir]") {
    TempDir tmp;
    VFSx::VFS vfs;
    vfs.Mount(Root::Project, tmp.path);

    Path dir(Root::Project, "Saved/Logs/sub");
    REQUIRE(vfs.CreateDirectories(dir));
    REQUIRE(vfs.IsDirectory(dir));
}

TEST_CASE("VFS Iterate enumerates direct children", "[vfs][io][iterate]") {
    TempDir tmp;
    VFSx::VFS vfs;
    vfs.Mount(Root::Project, tmp.path);

    // Create three files at the root + one in a subdirectory.
    REQUIRE(vfs.WriteText(Path(Root::Project, "a.txt"), "a"));
    REQUIRE(vfs.WriteText(Path(Root::Project, "b.txt"), "b"));
    REQUIRE(vfs.WriteText(Path(Root::Project, "c.txt"), "c"));
    REQUIRE(vfs.WriteText(Path(Root::Project, "sub/d.txt"), "d"));

    usize count = 0;
    std::vector<std::string> names;
    Path root(Root::Project, "");
    vfs.Iterate(root, [&](const fs::directory_entry& e) {
        ++count;
        names.push_back(e.path().filename().string());
        return true;
    });
    REQUIRE(count == 4);
    // sub/ is the 4th entry; the 3 .txt files are the other 3.
    REQUIRE(std::find(names.begin(), names.end(), "a.txt") != names.end());
    REQUIRE(std::find(names.begin(), names.end(), "b.txt") != names.end());
    REQUIRE(std::find(names.begin(), names.end(), "c.txt") != names.end());
    REQUIRE(std::find(names.begin(), names.end(), "sub") != names.end());
}

TEST_CASE("VFS Iterate callback can stop early by returning false", "[vfs][io][iterate]") {
    TempDir tmp;
    VFSx::VFS vfs;
    vfs.Mount(Root::Project, tmp.path);
    for (int i = 0; i < 5; ++i) {
        REQUIRE(vfs.WriteText(Path(Root::Project, "f" + std::to_string(i) + ".txt"), "x"));
    }
    usize count = 0;
    vfs.Iterate(Path(Root::Project, ""), [&](const fs::directory_entry&) {
        ++count;
        return count < 2;  // stop after 2
    });
    REQUIRE(count == 2);
}

TEST_CASE("DetectEngineRoot finds repo via CMakeLists.txt sentinel", "[vfs][detect]") {
    // We can't easily stub the environment in a portable way, so this test
    // just verifies the algorithm works when a CMakeLists.txt is reachable
    // by walking up from CWD.
    auto root = VFSx::DetectEngineRoot();
    REQUIRE_FALSE(root.empty());
    // The sentinel must exist where we claim the root is.
    REQUIRE(fs::exists(root / "CMakeLists.txt"));
}

TEST_CASE("ProjectRootFromFile returns the file's parent", "[vfs][detect]") {
    auto p = VFSx::ProjectRootFromFile("C:/Foo/Bar/Project.luma");
    REQUIRE(p == "C:/Foo/Bar");
    REQUIRE(VFSx::ProjectRootFromFile("").empty());
}
