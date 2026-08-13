#include "Luma/VFS/Path.h"

#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace Luma::VFS {

// -- Root <-> string --------------------------------------------------------

const char* ToString(Root r) {
    switch (r) {
        case Root::Engine:       return "Engine";
        case Root::Config:       return "Config";
        case Root::Content:      return "Content";
        case Root::Project:      return "Project";
        case Root::Saved:        return "Saved";
        case Root::Intermediate: return "Intermediate";
        case Root::Absolute:     return "Absolute";
    }
    return "Absolute";
}

Root RootFromString(std::string_view text) {
    if (text == "Engine")       return Root::Engine;
    if (text == "Config")       return Root::Config;
    if (text == "Content")      return Root::Content;
    if (text == "Project")      return Root::Project;
    if (text == "Saved")        return Root::Saved;
    if (text == "Intermediate") return Root::Intermediate;
    return Root::Absolute;
}

// -- Path normalization -----------------------------------------------------

bool IsInvalidPathChar(char c) {
    // Forbid control chars and the Windows-illegal set. Path separators
    // ('/' and '\') are NOT forbidden here — they're handled separately by
    // NormalizeRelative (which converts '\' to '/' and splits on '/').
    if (static_cast<unsigned char>(c) < 0x20) return true;
    switch (c) {
        case ':':  case '*':  case '?':
        case '"':  case '<':  case '>':
        case '|':
            return true;
        default: return false;
    }
}

std::string NormalizeRelative(std::string_view rel) {
    if (rel.empty()) return {};

    // 1) Normalize separators: copy and convert every '\' to '/'. We need
    //    this in a writable buffer because string_view can't be mutated.
    std::string buf;
    buf.reserve(rel.size());
    for (char c : rel) {
        buf.push_back(c == '\\' ? '/' : c);
    }

    // 2) Validate: reject any forbidden character up front.
    for (char c : buf) {
        if (IsInvalidPathChar(c)) {
            return {};
        }
    }

    // 3) Split on '/', drop empties, resolve '.' / '..'.
    std::vector<std::string> stack;
    usize i = 0;
    const usize n = buf.size();
    while (i < n) {
        usize j = buf.find('/', i);
        if (j == std::string::npos) j = n;
        std::string_view part(buf.data() + i, j - i);
        if (part == "." || part.empty()) {
            // skip
        } else if (part == "..") {
            if (!stack.empty()) stack.pop_back();
            // ".." above the root => empty. Caller will treat that as
            // out-of-bounds.
        } else {
            stack.emplace_back(part);
        }
        i = j + 1;
    }

    // 4) Rejoin.
    std::string out;
    out.reserve(buf.size());
    for (usize k = 0; k < stack.size(); ++k) {
        if (k > 0) out.push_back('/');
        out.append(stack[k]);
    }
    return out;
}

// -- Path constructors ------------------------------------------------------

Path::Path() = default;

Path::Path(Root root, std::string relative)
    : m_root(root), m_relative(NormalizeRelative(relative)) {}

Path::Path(std::filesystem::path absolute)
    : m_root(Root::Absolute), m_absolute(std::move(absolute)) {
    // Make the absolute path canonical-ish: prefer forward slashes in our
    // string rendering by leaving m_absolute as the OS-native form, but
    // ToString() will render it via string().
}

Path::Path(std::string_view text) {
    if (text.empty()) {
        m_root = Root::Absolute;
        return;
    }
    // Look for a "Root:/" prefix. Order matters: longer / more-specific
    // names first so "Intermediate" is matched before "Internal" (theoretical)
    // and "Config" before "Content" (C-o-n-f vs C-o-n-t — fine here, but
    // listed in a stable order anyway).
    for (Root r : {Root::Intermediate, Root::Engine, Root::Project, Root::Saved,
                   Root::Config, Root::Content}) {
        const char* name = ::Luma::VFS::ToString(r);
        const usize nlen = std::strlen(name);
        if (text.size() > nlen + 2 &&
            text.compare(0, nlen, name) == 0 &&
            text[nlen] == ':' &&
            (text[nlen + 1] == '/' || text[nlen + 1] == '\\')) {
            m_root = r;
            m_relative = NormalizeRelative(text.substr(nlen + 2));
            return;
        }
    }
    // No known prefix => treat as an absolute (or CWD-relative) filesystem
    // path. We deliberately don't NormalizeRelative on this branch because
    // raw OS paths may contain characters we forbid in virtual paths (":",
    // drive letters, etc.).
    m_root = Root::Absolute;
    m_absolute = std::filesystem::path(std::string(text));
}

Path::Path(const char* text) : Path(std::string_view(text ? text : "")) {}

std::string Path::ToString() const {
    if (m_root == Root::Absolute) {
        return m_absolute.string();
    }
    std::string out = ::Luma::VFS::ToString(m_root);
    out += ":/";
    out += m_relative;
    return out;
}

bool Path::operator==(const Path& o) const {
    if (m_root != o.m_root) return false;
    if (m_root == Root::Absolute) return m_absolute == o.m_absolute;
    return m_relative == o.m_relative;
}

}  // namespace Luma::VFS
