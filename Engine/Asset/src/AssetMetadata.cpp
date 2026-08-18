#include "Luma/Asset/AssetMetadata.h"

#include <fstream>
#include <sstream>

#include "Luma/Core/Log.h"
#include "Luma/Serialization/Json.h"
#include "Luma/Serialization/SerialValue.h"

namespace Luma {

bool AssetMetadata::SourceChanged(i64 currentMtime, const std::string& currentHash) const {
    return (currentMtime != sourceMtime) || (currentHash != sourceHash);
}

std::string MeshImportSettings::ToJson() const {
    SerialValue obj = SerialValue::MakeObject();
    obj["generateNormals"] = generateNormals;
    obj["generateTangents"] = generateTangents;
    obj["flipUVs"] = flipUVs;
    obj["optimizeMesh"] = optimizeMesh;
    obj["optimizeVertexCache"] = optimizeVertexCache;
    obj["optimizeVertexFetch"] = optimizeVertexFetch;
    obj["scale"] = scale;
    obj["importSkeleton"] = importSkeleton;
    obj["importAnimations"] = importAnimations;
    return WriteJson(obj, false);  // Compact JSON
}

MeshImportSettings MeshImportSettings::FromJson(const std::string& json) {
    MeshImportSettings settings = GetDefaults();
    
    std::string error;
    auto value = ParseJson(json, &error);
    if (!value || !value->IsObject()) {
        LUMA_LOG_WARN("AssetMetadata", "Failed to parse mesh import settings: {}", error);
        return settings;
    }
    
    const SerialValue& obj = *value;
    if (auto v = obj.Find("generateNormals"); v && v->IsBool())
        settings.generateNormals = v->AsBool(settings.generateNormals);
    if (auto v = obj.Find("generateTangents"); v && v->IsBool())
        settings.generateTangents = v->AsBool(settings.generateTangents);
    if (auto v = obj.Find("flipUVs"); v && v->IsBool())
        settings.flipUVs = v->AsBool(settings.flipUVs);
    if (auto v = obj.Find("optimizeMesh"); v && v->IsBool())
        settings.optimizeMesh = v->AsBool(settings.optimizeMesh);
    if (auto v = obj.Find("optimizeVertexCache"); v && v->IsBool())
        settings.optimizeVertexCache = v->AsBool(settings.optimizeVertexCache);
    if (auto v = obj.Find("optimizeVertexFetch"); v && v->IsBool())
        settings.optimizeVertexFetch = v->AsBool(settings.optimizeVertexFetch);
    if (auto v = obj.Find("scale"); v && v->IsNumber())
        settings.scale = static_cast<f32>(v->AsFloat(settings.scale));
    if (auto v = obj.Find("importSkeleton"); v && v->IsBool())
        settings.importSkeleton = v->AsBool(settings.importSkeleton);
    if (auto v = obj.Find("importAnimations"); v && v->IsBool())
        settings.importAnimations = v->AsBool(settings.importAnimations);
    
    return settings;
}

MeshImportSettings MeshImportSettings::GetDefaults() {
    return MeshImportSettings{};  // Default-initialized values are the defaults
}

std::string TextureImportSettings::ToJson() const {
    SerialValue obj = SerialValue::MakeObject();
    obj["generateMipmaps"] = generateMipmaps;
    obj["sRGB"] = sRGB;
    obj["compress"] = compress;
    obj["normalMap"] = normalMap;
    obj["maxTextureSize"] = maxTextureSize;
    obj["desiredSize"] = desiredSize;
    obj["scale"] = scale;
    obj["filterType"] = filterType;
    obj["preserveAlpha"] = preserveAlpha;
    obj["flipY"] = flipY;
    obj["premultiplyAlpha"] = premultiplyAlpha;
    obj["alphaThreshold"] = alphaThreshold;
    obj["packChannels"] = packChannels;
    obj["redChannel"] = redChannel;
    obj["greenChannel"] = greenChannel;
    obj["blueChannel"] = blueChannel;
    obj["alphaChannel"] = alphaChannel;
    return WriteJson(obj, false);  // Compact JSON
}

TextureImportSettings TextureImportSettings::FromJson(const std::string& json) {
    TextureImportSettings settings = GetDefaults();
    
    std::string error;
    auto value = ParseJson(json, &error);
    if (!value || !value->IsObject()) {
        LUMA_LOG_WARN("AssetMetadata", "Failed to parse texture import settings: {}", error);
        return settings;
    }
    
    const SerialValue& obj = *value;
    if (auto v = obj.Find("generateMipmaps"); v && v->IsBool())
        settings.generateMipmaps = v->AsBool(settings.generateMipmaps);
    if (auto v = obj.Find("sRGB"); v && v->IsBool())
        settings.sRGB = v->AsBool(settings.sRGB);
    if (auto v = obj.Find("compress"); v && v->IsBool())
        settings.compress = v->AsBool(settings.compress);
    if (auto v = obj.Find("normalMap"); v && v->IsBool())
        settings.normalMap = v->AsBool(settings.normalMap);
    if (auto v = obj.Find("maxTextureSize"); v && v->IsNumber())
        settings.maxTextureSize = static_cast<i32>(v->AsInt(settings.maxTextureSize));
    if (auto v = obj.Find("desiredSize"); v && v->IsNumber())
        settings.desiredSize = static_cast<i32>(v->AsInt(settings.desiredSize));
    if (auto v = obj.Find("scale"); v && v->IsNumber())
        settings.scale = static_cast<f32>(v->AsFloat(settings.scale));
    if (auto v = obj.Find("filterType"); v && v->IsNumber())
        settings.filterType = static_cast<i32>(v->AsInt(settings.filterType));
    if (auto v = obj.Find("preserveAlpha"); v && v->IsBool())
        settings.preserveAlpha = v->AsBool(settings.preserveAlpha);
    if (auto v = obj.Find("flipY"); v && v->IsBool())
        settings.flipY = v->AsBool(settings.flipY);
    if (auto v = obj.Find("premultiplyAlpha"); v && v->IsBool())
        settings.premultiplyAlpha = v->AsBool(settings.premultiplyAlpha);
    if (auto v = obj.Find("alphaThreshold"); v && v->IsNumber())
        settings.alphaThreshold = static_cast<i32>(v->AsInt(settings.alphaThreshold));
    if (auto v = obj.Find("packChannels"); v && v->IsBool())
        settings.packChannels = v->AsBool(settings.packChannels);
    if (auto v = obj.Find("redChannel"); v && v->IsNumber())
        settings.redChannel = static_cast<i32>(v->AsInt(settings.redChannel));
    if (auto v = obj.Find("greenChannel"); v && v->IsNumber())
        settings.greenChannel = static_cast<i32>(v->AsInt(settings.greenChannel));
    if (auto v = obj.Find("blueChannel"); v && v->IsNumber())
        settings.blueChannel = static_cast<i32>(v->AsInt(settings.blueChannel));
    if (auto v = obj.Find("alphaChannel"); v && v->IsNumber())
        settings.alphaChannel = static_cast<i32>(v->AsInt(settings.alphaChannel));
    
    return settings;
}

TextureImportSettings TextureImportSettings::GetDefaults() {
    return TextureImportSettings{};  // Default-initialized values are the defaults
}

namespace AssetMetadataIO {

std::optional<AssetMetadata> Read(const std::filesystem::path& metaPath) {
    std::ifstream file(metaPath);
    if (!file.is_open()) {
        return std::nullopt;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    std::string error;
    auto value = ParseJson(content, &error);
    if (!value || !value->IsObject()) {
        LUMA_LOG_WARN("AssetMetadata", "Failed to parse metadata file {}: {}", 
                      metaPath.string(), error);
        return std::nullopt;
    }
    
    const SerialValue& obj = *value;
    AssetMetadata meta;
    
    // Read GUID
    if (auto v = obj.Find("guid"); v && v->IsString()) {
        meta.guid = FromString(v->AsString());
    }
    
    // Read type
    if (auto v = obj.Find("type"); v && v->IsString()) {
        std::string typeStr = v->AsString();
        // Convert string to AssetType (simplified - could be enhanced)
        if (typeStr == "Mesh") meta.type = AssetType::Mesh;
        else if (typeStr == "Texture") meta.type = AssetType::Texture;
        else if (typeStr == "Material") meta.type = AssetType::Material;
        else if (typeStr == "Shader") meta.type = AssetType::Shader;
        else if (typeStr == "Audio") meta.type = AssetType::Sound;
        else meta.type = AssetType::Unknown;
    }
    
    // Read paths
    if (auto v = obj.Find("sourcePath"); v && v->IsString())
        meta.sourcePath = v->AsString();
    if (auto v = obj.Find("nativePath"); v && v->IsString())
        meta.nativePath = v->AsString();
    
    // Read importer info
    if (auto v = obj.Find("importerVersion"); v && v->IsString())
        meta.importerVersion = v->AsString();
    
    // Read timestamps and hash
    if (auto v = obj.Find("sourceMtime"); v && v->IsNumber())
        meta.sourceMtime = static_cast<i64>(v->AsInt());
    if (auto v = obj.Find("sourceHash"); v && v->IsString())
        meta.sourceHash = v->AsString();
    if (auto v = obj.Find("importTime"); v && v->IsNumber())
        meta.importTime = static_cast<i64>(v->AsInt());
    
    // Read state
    if (auto v = obj.Find("isImporting"); v && v->IsBool())
        meta.isImporting = v->AsBool();
    if (auto v = obj.Find("lastError"); v && v->IsString())
        meta.lastError = v->AsString();
    
    // Read import settings
    if (auto v = obj.Find("importSettings"); v && v->IsString())
        meta.importSettings = v->AsString();
    
    return meta;
}

bool Write(const std::filesystem::path& metaPath, const AssetMetadata& meta) {
    SerialValue obj = SerialValue::MakeObject();
    
    obj["guid"] = ToString(meta.guid);
    obj["type"] = AssetTypeName(meta.type);
    obj["sourcePath"] = meta.sourcePath.string();
    obj["nativePath"] = meta.nativePath.string();
    obj["importerVersion"] = meta.importerVersion;
    obj["sourceMtime"] = meta.sourceMtime;
    obj["sourceHash"] = meta.sourceHash;
    obj["importTime"] = meta.importTime;
    obj["isImporting"] = meta.isImporting;
    obj["lastError"] = meta.lastError;
    obj["importSettings"] = meta.importSettings;
    
    std::string json = WriteJson(obj, true);  // Pretty JSON for readability
    
    std::ofstream file(metaPath);
    if (!file.is_open()) {
        LUMA_LOG_ERROR("AssetMetadata", "Failed to write metadata file: {}", 
                       metaPath.string());
        return false;
    }
    
    file << json;
    return true;
}

std::filesystem::path MetaPathForSource(const std::filesystem::path& sourcePath) {
    return sourcePath.string() + ".meta";
}

std::filesystem::path NativePathForSource(const std::filesystem::path& sourcePath, AssetType type) {
    std::string extension;
    switch (type) {
        case AssetType::Mesh:
            extension = ".lmesh";
            break;
        case AssetType::Texture:
            extension = ".ltex";
            break;
        case AssetType::Material:
            extension = ".lmat";
            break;
        case AssetType::Shader:
            extension = ".lshader";
            break;
        case AssetType::Sound:
            extension = ".laudio";
            break;
        default:
            extension = ".lasset";
            break;
    }
    
    // Replace source extension with native extension
    auto stem = sourcePath.stem();
    auto parent = sourcePath.parent_path();
    return parent / (stem.string() + extension);
}

std::string ComputeFileHash(const std::filesystem::path& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    
    // Simple hash using file size and first/last 1KB for quick change detection
    // For production, should use proper hash like SHA-256
    std::stringstream hash;
    
    file.seekg(0, std::ios::end);
    usize fileSize = file.tellg();
    hash << fileSize << "|";
    
    // Read first 1KB
    constexpr usize kSampleSize = 1024;
    std::vector<char> buffer(kSampleSize);
    
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), std::min(fileSize, kSampleSize));
    for (usize i = 0; i < std::min(fileSize, kSampleSize); ++i) {
        hash << std::hex << static_cast<int>(static_cast<unsigned char>(buffer[i]));
    }
    
    hash << "|";
    
    // Read last 1KB if file is large enough
    if (fileSize > kSampleSize) {
        file.seekg(-static_cast<std::streamoff>(kSampleSize), std::ios::end);
        file.read(buffer.data(), kSampleSize);
        for (usize i = 0; i < kSampleSize; ++i) {
            hash << std::hex << static_cast<int>(static_cast<unsigned char>(buffer[i]));
        }
    }
    
    return hash.str();
}

}  // namespace AssetMetadataIO

}  // namespace Luma
