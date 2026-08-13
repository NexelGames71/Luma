#include "Luma/Asset/AssetScanner.h"

#include <algorithm>
#include <chrono>
#include <system_error>

namespace Luma {

namespace {
std::string StemString(const std::filesystem::path& p) { return p.stem().string(); }

i64 FileTimeToUnixSeconds(std::filesystem::file_time_type ft) {
    using namespace std::chrono;
    static const auto anchor = std::filesystem::file_time_type::clock::now();
    if (ft < anchor) return 0;
    return duration_cast<seconds>(ft - anchor).count();
}
}  // namespace

bool AssetScanner::ShouldIgnore(const std::filesystem::path& path) noexcept {
    // Any component starting with '.' is ignored (e.g. .git, .DS_Store).
    for (auto it = path.begin(); it != path.end(); ++it) {
        auto s = it->string();
        if (!s.empty() && s.front() == '.') return true;
    }
    // Editor temp files.
    auto name = path.filename().string();
    if (name.ends_with(".luma_temp") || name.ends_with("~")) return true;
    return false;
}

void AssetScanner::ScanStreaming(
    const std::filesystem::path& root,
    const std::function<void(const AssetData&)>& onEntry,
    const ScanOptions& opts) {
    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) return;
    std::error_code ec2;
    auto baseDepth = static_cast<int>(
        std::distance(root.begin(), root.end()));
    if (!root.has_filename() && !root.empty()) {
        // Trailing slash — count one less so files directly inside match
        // depth 1, not 2.
        --baseDepth;
    }

    std::filesystem::recursive_directory_iterator it(
        root, std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec) return;
    std::filesystem::recursive_directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) break;
        const auto& entry = *it;
        if (ShouldIgnore(entry.path())) continue;
        auto depth = static_cast<int>(std::distance(
                          entry.path().begin(), entry.path().end())) -
                      baseDepth;
        if (depth < 0) depth = 0;
        if (opts.maxDepth >= 0 && depth > opts.maxDepth) continue;
        bool isDir = entry.is_directory(ec);
        if (isDir && !opts.includeFolders) continue;
        AssetData data;
        data.id = MakeAssetIdFromKey(entry.path().string());
        data.packagePath = entry.path();
        data.assetName = StemString(entry.path());
        data.extension = LowercaseExtension(entry.path());
        if (isDir) {
            data.type = AssetType::Folder;
        } else {
            data.type = AssetTypeFromExtension(data.extension);
            data.mtime = FileTimeToUnixSeconds(entry.last_write_time(ec));
            data.size = entry.file_size(ec);
        }
        onEntry(data);
    }
    (void)ec2;
}

std::vector<AssetData> AssetScanner::Scan(const std::filesystem::path& root,
                                          const ScanOptions& opts) {
    std::vector<AssetData> out;
    ScanStreaming(root,
                  [&](const AssetData& d) { out.push_back(d); },
                  opts);
    return out;
}

}  // namespace Luma
