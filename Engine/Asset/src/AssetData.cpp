#include "Luma/Asset/AssetData.h"

#include <algorithm>
#include <cctype>

namespace Luma {

std::string LowercaseExtension(const std::filesystem::path& p) {
    auto ext = p.extension().string();
    if (ext.empty()) return {};
    if (ext.front() == '.') ext.erase(0, 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return ext;
}

}  // namespace Luma
