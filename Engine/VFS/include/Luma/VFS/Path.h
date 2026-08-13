#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "Luma/Core/Types.h"

// Luma virtual paths. A Path is either:
//   * a virtual address under one of the well-known roots (Engine/Project/
//     Saved/Intermediate), shaped as "Root:/relative/part"
//   * a plain absolute filesystem path (no root prefix)
//
// The VFS service uses the root to translate virtual paths into real ones.
// Code that only wants filesystem behavior can pass an absolute path straight
// through.

namespace Luma::VFS {

// Well-known mount points. Adding a new root here requires also teaching the
// VFS service how to handle it.
enum class Root : u8 {
    Engine,        // <repo>/Engine         (engine source assets: shaders, etc.)
    Config,        // <repo>/Config         (default engine config, base layers)
    Content,       // <repo>/Content        (editor assets: fonts, icons, templates)
    Project,       // <projectRoot>         (per-project content + config)
    Saved,         // <projectRoot>/Saved   (logs, screenshots, user prefs)
    Intermediate,  // <projectRoot>/Saved/Intermediate
    Absolute,      // "unrooted" - the Path stores a real OS path directly
};

const char* ToString(Root r);
Root RootFromString(std::string_view text);

// Total number of named roots. Absolute is included in the count for sizing
// the mount table, but it stores a real OS path directly and is never
// "mounted" the way the others are.
constexpr usize kRootCount = 7;

// A VFS-aware path. The default-constructed Path is empty; treat it as "no
// path" and let VFS operations on it return false / fail gracefully.
class Path {
public:
    Path();
    Path(Root root, std::string relative);
    explicit Path(std::filesystem::path absolute);
    // Parses a string. If `text` matches "Root:/..." the prefix becomes the
    // root and the rest is normalized as a relative part. Otherwise the text
    // is treated as a real OS path (absolute or working-directory relative).
    explicit Path(std::string_view text);
    explicit Path(const char* text);

    bool IsEmpty() const { return m_root == Root::Absolute && m_absolute.empty(); }
    bool IsAbsolute() const { return m_root == Root::Absolute; }

    Root GetRoot() const { return m_root; }
    const std::string& Relative() const { return m_relative; }
    const std::filesystem::path& Absolute() const { return m_absolute; }

    // Canonical rendering: virtual paths become "Root:/..."; absolute paths
    // are returned as their native form.
    std::string ToString() const;

    bool operator==(const Path& o) const;
    bool operator!=(const Path& o) const { return !(*this == o); }

private:
    Root m_root = Root::Absolute;
    std::string m_relative;             // for virtual paths
    std::filesystem::path m_absolute;   // for Absolute root
};

// Normalize a relative path string: forward-slashes only, no trailing slash,
// "." and ".." collapsed, redundant separators removed. Empty stays empty.
std::string NormalizeRelative(std::string_view rel);

// True if `c` is forbidden in a relative path component.
bool IsInvalidPathChar(char c);

}  // namespace Luma::VFS
