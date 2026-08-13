#include "Luma/VFS/VFS.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <system_error>

#include "Luma/Core/Log.h"
#include "Luma/Core/Types.h"

namespace Luma::VFS {

namespace {

// Locate a known sentinel file by walking up from `start`. Returns the
// directory that contains it, or empty if not found within `maxLevels`.
std::filesystem::path WalkUpFor(
    const std::filesystem::path& start,
    const std::string& sentinel,
    int maxLevels) {
    std::error_code ec;
    std::filesystem::path p = start;
    for (int i = 0; i <= maxLevels; ++i) {
        if (std::filesystem::exists(p / sentinel, ec)) {
            return p;
        }
        if (p.has_parent_path() && p != p.parent_path()) {
            p = p.parent_path();
        } else {
            break;
        }
    }
    return {};
}

const char* kEnvEngineRoot = "LUMA_ENGINE_ROOT";

std::filesystem::path RealEnvEngineRoot() {
    const char* env = std::getenv(kEnvEngineRoot);
    if (env && *env) {
        std::error_code ec;
        std::filesystem::path p(env);
        if (std::filesystem::is_directory(p, ec)) return p;
    }
    return {};
}

}  // namespace

// -- Engine root detection --------------------------------------------------

std::filesystem::path DetectEngineRoot() {
    auto env = RealEnvEngineRoot();
    if (!env.empty()) return env;

    std::error_code ec;
    // Try the directory containing the running executable. The build places
    // binaries at <root>/build/bin/<Cfg>/<exe>, so two levels up is the
    // repo root. We look for CMakeLists.txt as the sentinel because it's
    // always present at the repo root.
    try {
        auto cwd = std::filesystem::current_path(ec);
        auto found = WalkUpFor(cwd, "CMakeLists.txt", 6);
        if (!found.empty()) return found;
    } catch (...) {
    }

    // Fall back to CWD itself.
    return std::filesystem::current_path(ec);
}

std::filesystem::path ProjectRootFromFile(const std::filesystem::path& lumaFile) {
    if (lumaFile.empty()) return {};
    return lumaFile.parent_path();
}

// -- VFS --------------------------------------------------------------------

VFS::VFS() {
    m_mounts.resize(kRootCount);  // one slot per Root value
}

VFS::~VFS() = default;

void VFS::Mount(Root root, const std::filesystem::path& realPath) {
    const usize idx = static_cast<usize>(root);
    if (idx >= m_mounts.size()) return;
    std::error_code ec;
    if (realPath.empty()) {
        m_mounts[idx] = MountPoint{root, {}};
        return;
    }
    // We store the canonical absolute form so Resolve() can do an
    // "is this still under the root?" check reliably.
    auto canon = std::filesystem::weakly_canonical(realPath, ec);
    m_mounts[idx] = MountPoint{root, ec ? realPath : canon};
}

void VFS::Unmount(Root root) {
    Mount(root, {});
}

bool VFS::IsMounted(Root root) const {
    const usize idx = static_cast<usize>(root);
    if (idx >= m_mounts.size()) return false;
    return !m_mounts[idx].realPath.empty();
}

std::filesystem::path VFS::RootPath(Root root) const {
    const usize idx = static_cast<usize>(root);
    if (idx >= m_mounts.size()) return {};
    return m_mounts[idx].realPath;
}

bool VFS::Resolve(const Path& vpath, std::filesystem::path& outReal) const {
    if (vpath.IsEmpty()) return false;
    if (vpath.GetRoot() == Root::Absolute) {
        outReal = vpath.Absolute();
        return true;
    }
    const usize idx = static_cast<usize>(vpath.GetRoot());
    if (idx >= m_mounts.size() || m_mounts[idx].realPath.empty()) {
        return false;
    }
    const std::string& rel = vpath.Relative();
    if (rel.empty()) {
        outReal = m_mounts[idx].realPath;
        return true;
    }
    // Concatenate. NormalizeRelative already collapsed '..' so we can't
    // escape via the relative part.
    outReal = m_mounts[idx].realPath / rel;
    return true;
}

std::optional<std::filesystem::path> VFS::TryResolve(const Path& vpath) const {
    std::filesystem::path p;
    if (!Resolve(vpath, p)) return std::nullopt;
    return p;
}

std::filesystem::path VFS::Resolve(const Path& vpath) const {
    auto p = TryResolve(vpath);
    if (!p) {
        throw std::runtime_error("VFS: cannot resolve " + vpath.ToString());
    }
    return *p;
}

bool VFS::ReadFile(const Path& vpath, std::vector<u8>& out) const {
    auto real = TryResolve(vpath);
    if (!real) return false;
    std::ifstream f(*real, std::ios::binary | std::ios::ate);
    if (!f) return false;
    auto end = f.tellg();
    if (end < 0) return false;
    f.seekg(0, std::ios::beg);
    out.resize(static_cast<usize>(end));
    if (out.empty()) return true;
    f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
    return f.good() || f.eof();
}

bool VFS::WriteFile(const Path& vpath, std::span<const u8> data) const {
    auto real = TryResolve(vpath);
    if (!real) return false;
    auto parent = real->parent_path();
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    std::ofstream f(*real, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    if (!data.empty()) {
        f.write(reinterpret_cast<const char*>(data.data()),
                static_cast<std::streamsize>(data.size()));
    }
    return f.good();
}

std::optional<std::string> VFS::ReadText(const Path& vpath) const {
    std::vector<u8> bytes;
    if (!ReadFile(vpath, bytes)) return std::nullopt;
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

bool VFS::WriteText(const Path& vpath, std::string_view text) const {
    return WriteFile(vpath,
                     std::span<const u8>(reinterpret_cast<const u8*>(text.data()),
                                         text.size()));
}

bool VFS::Exists(const Path& vpath) const {
    auto real = TryResolve(vpath);
    if (!real) return false;
    std::error_code ec;
    return std::filesystem::exists(*real, ec);
}

bool VFS::IsFile(const Path& vpath) const {
    auto real = TryResolve(vpath);
    if (!real) return false;
    std::error_code ec;
    return std::filesystem::is_regular_file(*real, ec);
}

bool VFS::IsDirectory(const Path& vpath) const {
    auto real = TryResolve(vpath);
    if (!real) return false;
    std::error_code ec;
    return std::filesystem::is_directory(*real, ec);
}

bool VFS::CreateDirectories(const Path& vpath) const {
    auto real = TryResolve(vpath);
    if (!real) return false;
    std::error_code ec;
    std::filesystem::create_directories(*real, ec);
    return !ec;
}

usize VFS::Iterate(const Path& vpath, const DirCallback& fn) const {
    auto real = TryResolve(vpath);
    if (!real) return 0;
    std::error_code ec;
    if (!std::filesystem::is_directory(*real, ec)) return 0;
    usize count = 0;
    for (auto it = std::filesystem::directory_iterator(*real, ec);
         !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
        ++count;
        if (fn) {
            if (!fn(*it)) break;
        }
    }
    return count;
}

// -- Global singleton --------------------------------------------------------

namespace {

VFS* g_vfs = nullptr;

void BootstrapGlobalVFS(VFS& vfs) {
    auto repoRoot = DetectEngineRoot();
    if (repoRoot.empty()) return;

    // Engine, Config, Content are siblings at the repo root.
    vfs.Mount(Root::Engine, repoRoot / "Engine");
    vfs.Mount(Root::Config, repoRoot / "Config");
    vfs.Mount(Root::Content, repoRoot / "Content");

    // Saved / Intermediate default to the repo's own Saved when no project
    // is loaded (sandbox / no-arg run). The editor remounts them under the
    // active project.
    vfs.Mount(Root::Saved, repoRoot / "Saved");
    vfs.Mount(Root::Intermediate, repoRoot / "Saved" / "Intermediate");

    // Project is left unmounted until --project <file> is processed. Tests
    // don't go through this path.
}

}  // namespace

VFS& VFS::Global() {
    if (!g_vfs) {
        g_vfs = new VFS();
        BootstrapGlobalVFS(*g_vfs);
        LUMA_LOG_INFO("VFS", "repo root: {}",
                      g_vfs->IsMounted(Root::Engine)
                          ? g_vfs->RootPath(Root::Engine).parent_path().string()
                          : "<unmounted>");
    }
    return *g_vfs;
}

// Allow tests / shutdown paths to release the singleton cleanly.
void ResetGlobalVFSForTesting() {
    delete g_vfs;
    g_vfs = nullptr;
}

}  // namespace Luma::VFS
