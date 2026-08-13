#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Luma/Core/Types.h"
#include "Luma/VFS/Path.h"

// The Luma virtual file system. The VFS owns a small table of mounted roots
// (Engine / Project / Saved / Intermediate) and exposes read/write/exists/
// iterate helpers that take VFS::Path. Code that needs the real on-disk path
// (e.g. to hand to a third-party library) calls Resolve() first.
//
// The VFS does NOT implement async I/O in this revision. Reads block; writes
// are synchronous. Async I/O lives behind a separate AssetService (Phase 3/5).

namespace Luma::VFS {

// One mounted root entry.
struct MountPoint {
    Root root = Root::Absolute;
    std::filesystem::path realPath;  // empty => not mounted
};

class VFS {
public:
    VFS();
    ~VFS();

    VFS(const VFS&) = delete;
    VFS& operator=(const VFS&) = delete;

    // -- Mount management ---------------------------------------------------
    // Mount (or remount) a real directory under a well-known root. Passing an
    // empty path unmounts.
    void Mount(Root root, const std::filesystem::path& realPath);
    void Unmount(Root root);
    bool IsMounted(Root root) const;
    std::filesystem::path RootPath(Root root) const;
    const std::vector<MountPoint>& Mounts() const { return m_mounts; }

    // -- Resolve ------------------------------------------------------------
    // Translates a virtual Path into a real filesystem path. Returns false if
    // the root is not mounted, or if a virtual path resolves to a directory
    // outside its root (e.g. via "..").
    bool Resolve(const Path& vpath, std::filesystem::path& outReal) const;
    std::optional<std::filesystem::path> TryResolve(const Path& vpath) const;
    // Throws std::runtime_error on failure. Prefer TryResolve in hot paths.
    std::filesystem::path Resolve(const Path& vpath) const;

    // -- Read / Write / Query ----------------------------------------------
    // All take virtual paths. Files are read/written as raw bytes; text
    // convenience helpers exist for the common case.
    bool ReadFile(const Path& vpath, std::vector<u8>& out) const;
    bool WriteFile(const Path& vpath, std::span<const u8> data) const;
    std::optional<std::string> ReadText(const Path& vpath) const;
    bool WriteText(const Path& vpath, std::string_view text) const;

    // Query helpers. `CreateDirectories` is the only mutator besides Write*;
    // it mkdir -p's the path's parent (and any missing intermediates).
    bool Exists(const Path& vpath) const;
    bool IsFile(const Path& vpath) const;
    bool IsDirectory(const Path& vpath) const;
    bool CreateDirectories(const Path& vpath) const;

    // Iterate a directory. The callback receives one entry at a time and
    // returns true to keep going. Returns the number of entries visited.
    using DirCallback = std::function<bool(const std::filesystem::directory_entry&)>;
    usize Iterate(const Path& vpath, const DirCallback& fn) const;

    // -- Global singleton ---------------------------------------------------
    // First call constructs; subsequent calls return the same instance. The
    // process-wide instance is used by the runtime/editor to resolve well-
    // known paths without threading a VFS through every API.
    //
    // On construction, the singleton automatically:
    //   * Mounts Engine/ at <exe>/Engine (walking upward a few levels if not
    //     found there) — overridable via the LUMA_ENGINE_ROOT env var
    //   * Mounts Project/ only if --project <file.luma> was passed; in that
    //     case Project/, Saved/, and Intermediate/ are derived from the file's
    //     parent directory
    //   * Mounts Saved/Intermediate regardless (under <exe>/Saved/...) so the
    //     logging / scratch subsystems can write to a predictable place
    //
    // Tests should NOT use Global() — they should construct their own VFS
    // instance and mount temporary directories.
    static VFS& Global();

private:
    std::vector<MountPoint> m_mounts;
};

// -- Engine root detection --------------------------------------------------
// Returns the directory that should contain "Engine/", "Config/", etc.
// Detection order:
//   1. LUMA_ENGINE_ROOT env var
//   2. The grandparent of the current executable (typical "build/bin/<Cfg>/")
//   3. The current working directory
//   4. Walks up from CWD looking for the first directory that contains
//      "Engine/LumaEngineVersion.h" (a unique file only the engine has)
std::filesystem::path DetectEngineRoot();

// -- Project root detection -------------------------------------------------
// Given a `.luma` file path, returns the project root (the file's parent
// directory). Returns an empty path if `lumaFile` is empty.
std::filesystem::path ProjectRootFromFile(const std::filesystem::path& lumaFile);

// Tear down the global singleton. Tests use this; production code should
// let the process exit naturally.
void ResetGlobalVFSForTesting();

}  // namespace Luma::VFS
