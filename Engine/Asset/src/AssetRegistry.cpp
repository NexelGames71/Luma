#include "Luma/Asset/AssetRegistry.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <system_error>

#include "Luma/Core/Log.h"

namespace Luma {

namespace {
i64 FileTimeToUnixSeconds(std::filesystem::file_time_type ft) {
    using namespace std::chrono;
    // file_time_type's epoch differs by platform (Windows: 1601-01-01,
    // POSIX: 1970-01-01). We don't need exact wall clock — only a stable
    // count for change detection — so we approximate by subtracting a
    // known anchor. For MSVC where file_clock::to_sys isn't available,
    // we use the time-since-epoch of file_clock's own now() and treat
    // it as monotonic.
    static const auto anchor = std::filesystem::file_time_type::clock::now();
    if (ft < anchor) return 0;
    auto dur = ft - anchor;
    return duration_cast<seconds>(dur).count();
}

std::string StemString(const std::filesystem::path& p) {
    auto stem = p.stem().string();
    return stem;
}
}  // namespace

std::filesystem::path AssetRegistry::Normalize(
    const std::filesystem::path& p) const {
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(p, ec);
    if (ec) return p.lexically_normal();
    return canonical;
}

bool AssetRegistry::IsUnderAnyRoot(const std::filesystem::path& p) const {
    auto norm = Normalize(p);
    for (const auto& root : m_roots) {
        auto r = Normalize(root);
        auto rel = norm.lexically_relative(r);
        auto relStr = rel.string();
        if (!relStr.empty() && relStr.front() != '.') return true;
    }
    return false;
}

AssetId AssetRegistry::MakeId(const std::filesystem::path& p) const {
    auto norm = Normalize(p);
    std::string key = m_salt + "|" + norm.string();
    return MakeAssetIdFromKey(key);
}

void AssetRegistry::AddRoot(const std::filesystem::path& root) {
    auto norm = Normalize(root);
    if (std::find(m_roots.begin(), m_roots.end(), norm) == m_roots.end()) {
        m_roots.push_back(norm);
    }
}

void AssetRegistry::Scan() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_byId.clear();
    m_byPath.clear();

    for (const auto& root : m_roots) {
        std::error_code ec;
        if (!std::filesystem::exists(root, ec)) continue;
        std::filesystem::recursive_directory_iterator it(root, ec);
        if (ec) {
            LUMA_LOG_WARN("AssetRegistry", "scan failed for {}: {}",
                          root.string(), ec.message());
            continue;
        }
        std::filesystem::recursive_directory_iterator end;
        std::vector<AssetData> pendingFiles;
        std::vector<AssetData> pendingDirs;

        for (; it != end; it.increment(ec)) {
            if (ec) {
                LUMA_LOG_WARN("AssetRegistry", "walk error: {}", ec.message());
                break;
            }
            const auto& entry = *it;
            auto abs = Normalize(entry.path());
            std::error_code ec2;
            auto status = entry.symlink_status(ec2);
            (void)ec2;
            bool isDir = std::filesystem::is_directory(status);
            std::string ext = LowercaseExtension(abs);

            AssetData data;
            data.id = MakeId(abs);
            data.packagePath = abs;
            data.assetName = StemString(abs);
            data.extension = ext;
            if (entry.exists(ec)) {
                data.mtime = isDir ? 0 : FileTimeToUnixSeconds(entry.last_write_time(ec));
                if (!isDir) data.size = entry.file_size(ec);
            }

            if (isDir) {
                data.type = AssetType::Folder;
                pendingDirs.push_back(std::move(data));
            } else {
                data.type = AssetTypeFromExtension(ext);
                pendingFiles.push_back(std::move(data));
            }
        }

        // Insert folders first so the directory rows show up before files
        // for sorting / tree-building purposes in the Content Browser.
        for (auto& d : pendingDirs) {
            auto id = d.id;
            m_byPath[d.packagePath.string()] = id;
            m_byId.emplace(id, std::move(d));
        }
        for (auto& f : pendingFiles) {
            auto id = f.id;
            m_byPath[f.packagePath.string()] = id;
            m_byId.emplace(id, std::move(f));
        }
    }
}

void AssetRegistry::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_byId.clear();
    m_byPath.clear();
}

usize AssetRegistry::Size() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_byId.size();
}

const AssetData* AssetRegistry::Lookup(const AssetId& id) const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_byId.find(id);
    return it == m_byId.end() ? nullptr : &it->second;
}

const AssetData* AssetRegistry::LookupByPath(
    const std::filesystem::path& path) const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto norm = Normalize(path);
    auto it = m_byPath.find(norm.string());
    if (it == m_byPath.end()) return nullptr;
    auto jt = m_byId.find(it->second);
    return jt == m_byId.end() ? nullptr : &jt->second;
}

std::vector<const AssetData*> AssetRegistry::All() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<const AssetData*> out;
    out.reserve(m_byId.size());
    for (const auto& [id, data] : m_byId) out.push_back(&data);
    return out;
}

std::vector<const AssetData*> AssetRegistry::FilterByType(AssetType type) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<const AssetData*> out;
    for (const auto& [id, data] : m_byId) {
        if (data.type == type) out.push_back(&data);
    }
    return out;
}

std::vector<const AssetData*> AssetRegistry::FilterByDirectory(
    const std::filesystem::path& dir) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto normDir = Normalize(dir);
    std::vector<const AssetData*> out;
    for (const auto& [id, data] : m_byId) {
        if (data.IsFolder()) continue;
        auto rel = data.packagePath.lexically_relative(normDir);
        auto relStr = rel.string();
        if (!relStr.empty() && relStr.front() != '.' &&
            relStr.find("..") == std::string::npos) {
            out.push_back(&data);
        }
    }
    return out;
}

std::vector<const AssetData*> AssetRegistry::FilterByName(
    const std::string& nameSubstring) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<const AssetData*> out;
    if (nameSubstring.empty()) {
        out.reserve(m_byId.size());
        for (const auto& [id, data] : m_byId) out.push_back(&data);
        return out;
    }
    std::string needle = nameSubstring;
    std::transform(needle.begin(), needle.end(), needle.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    for (const auto& [id, data] : m_byId) {
        std::string hay = data.assetName;
        std::transform(hay.begin(), hay.end(), hay.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (hay.find(needle) != std::string::npos) out.push_back(&data);
    }
    return out;
}

std::vector<const AssetData*> AssetRegistry::Filter(
    std::optional<AssetType> type,
    std::optional<std::filesystem::path> dir,
    const std::string& nameSubstring) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<const AssetData*> out;
    std::string needle;
    if (!nameSubstring.empty()) {
        needle = nameSubstring;
        std::transform(needle.begin(), needle.end(), needle.begin(),
                       [](unsigned char c) { return std::tolower(c); });
    }
    auto normDir = dir ? Normalize(*dir) : std::filesystem::path{};
    bool hasDir = dir.has_value();
    for (const auto& [id, data] : m_byId) {
        if (type.has_value() && data.type != *type) continue;
        if (hasDir) {
            auto rel = data.packagePath.lexically_relative(normDir);
            auto relStr = rel.string();
            if (relStr.empty() || relStr.front() == '.') continue;
        }
        if (!needle.empty()) {
            std::string hay = data.assetName;
            std::transform(hay.begin(), hay.end(), hay.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (hay.find(needle) == std::string::npos) continue;
        }
        out.push_back(&data);
    }
    return out;
}

void AssetRegistry::RefreshPath(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!IsUnderAnyRoot(path)) return;
    auto abs = Normalize(path);
    std::error_code ec;
    if (!std::filesystem::exists(abs, ec)) {
        // Path went away — drop any entry under it.
        auto it = m_byPath.find(abs.string());
        if (it != m_byPath.end()) {
            m_byId.erase(it->second);
            m_byPath.erase(it);
        }
        return;
    }
    AssetData data;
    data.id = MakeId(abs);
    data.packagePath = abs;
    data.assetName = StemString(abs);
    data.extension = LowercaseExtension(abs);
    auto status = std::filesystem::status(abs, ec);
    bool isDir = std::filesystem::is_directory(status);
    if (isDir) {
        data.type = AssetType::Folder;
    } else {
        data.type = AssetTypeFromExtension(data.extension);
        data.mtime = FileTimeToUnixSeconds(
            std::filesystem::last_write_time(abs, ec));
        data.size = std::filesystem::file_size(abs, ec);
    }
    auto id = data.id;
    m_byPath[abs.string()] = id;
    m_byId[id] = std::move(data);
}

void AssetRegistry::RemoveUnder(const std::filesystem::path& dir) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto normDir = Normalize(dir);
    std::vector<std::string> pathsToDrop;
    for (auto& [path, id] : m_byPath) {
        std::filesystem::path p(path);
        auto rel = p.lexically_relative(normDir);
        auto relStr = rel.string();
        if (!relStr.empty() && relStr.front() != '.' &&
            relStr.find("..") == std::string::npos) {
            pathsToDrop.push_back(path);
        }
    }
    for (auto& p : pathsToDrop) {
        auto id = m_byPath[p];
        m_byId.erase(id);
        m_byPath.erase(p);
    }
}

}  // namespace Luma
