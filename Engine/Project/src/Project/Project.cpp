#include "Luma/Project/Project.h"

#include <fstream>
#include <system_error>

#include "Luma/Core/Config.h"
#include "Luma/Core/Log.h"
#include "Luma/Core/Version.h"

namespace Luma {
namespace fs = std::filesystem;

const char* ToString(GameTemplate tmpl) {
    switch (tmpl) {
        case GameTemplate::Empty:       return "Empty";
        case GameTemplate::FirstPerson: return "FirstPerson";
        case GameTemplate::ThirdPerson: return "ThirdPerson";
        case GameTemplate::TopDown:     return "TopDown";
    }
    return "Empty";
}

GameTemplate GameTemplateFromString(std::string_view text,
                                    GameTemplate fallback) {
    if (text == "Empty") return GameTemplate::Empty;
    if (text == "FirstPerson") return GameTemplate::FirstPerson;
    if (text == "ThirdPerson") return GameTemplate::ThirdPerson;
    if (text == "TopDown") return GameTemplate::TopDown;
    return fallback;
}

bool IsValidProjectName(std::string_view name) {
    if (name.empty() || name.size() > 128) return false;
    for (char c : name) {
        switch (c) {
            case '/':
            case '\\':
            case ':':
            case '*':
            case '?':
            case '"':
            case '<':
            case '>':
            case '|':
                return false;
            default:
                break;
        }
    }
    // No leading/trailing spaces or dots (Windows-hostile).
    if (name.front() == ' ' || name.back() == ' ' || name.back() == '.') {
        return false;
    }
    return true;
}

namespace {

std::string CurrentEngineVersionString() {
    Version v = EngineVersion();
    return std::to_string(v.major) + "." + std::to_string(v.minor) + "." +
           std::to_string(v.patch);
}

void SetError(std::string* out, std::string message) {
    LUMA_LOG_ERROR("Project", "{}", message);
    if (out) *out = std::move(message);
}

}  // namespace

std::optional<Project> Project::Create(const ProjectDesc& desc,
                                       std::string* outError) {
    if (!IsValidProjectName(desc.name)) {
        SetError(outError, "invalid project name: '" + desc.name + "'");
        return std::nullopt;
    }
    if (desc.parentDirectory.empty()) {
        SetError(outError, "no parent directory specified");
        return std::nullopt;
    }

    std::error_code ec;
    fs::path root = desc.parentDirectory / desc.name;
    fs::path lumaFile = root / (desc.name + kExtension);

    if (fs::exists(lumaFile)) {
        SetError(outError, "a project already exists at " + lumaFile.string());
        return std::nullopt;
    }

    for (const char* sub :
         {"", "Content", "Content/Scenes", "Config", "Source", "Saved"}) {
        fs::create_directories(root / sub, ec);
        if (ec) {
            SetError(outError, "failed to create " + (root / sub).string() +
                                   ": " + ec.message());
            return std::nullopt;
        }
    }

    // Default startup scene path, stored project-relative with forward slashes.
    const std::string startupScene =
        std::string("Content/Scenes/Main") + kSceneExtension;

    const std::string version = CurrentEngineVersionString();
    {
        std::ofstream out(lumaFile, std::ios::out | std::ios::trunc);
        if (!out) {
            SetError(outError, "failed to write " + lumaFile.string());
            return std::nullopt;
        }
        out << "# Luma project descriptor\n"
            << "[Project]\n"
            << "name = " << desc.name << "\n"
            << "engine_version = " << version << "\n"
            << "template = " << ToString(desc.gameTemplate) << "\n"
            << "startup_scene = " << startupScene << "\n";
    }

    LUMA_LOG_INFO("Project", "created project '{}' ({}) at {}", desc.name,
                  ToString(desc.gameTemplate), root.string());
    return Load(lumaFile, outError);
}

std::optional<Project> Project::Load(const fs::path& lumaFile,
                                     std::string* outError) {
    std::error_code ec;
    if (!fs::exists(lumaFile, ec)) {
        SetError(outError, "project file not found: " + lumaFile.string());
        return std::nullopt;
    }

    Config config;
    if (!config.LoadFromFile(lumaFile.string())) {
        SetError(outError, "failed to read project file: " + lumaFile.string());
        return std::nullopt;
    }

    Project project;
    project.m_projectFile = fs::absolute(lumaFile, ec);
    project.m_name = config.GetString("Project.name",
                                      lumaFile.stem().string());
    project.m_engineVersion = config.GetString("Project.engine_version", "0.0.0");
    project.m_template = GameTemplateFromString(
        config.GetString("Project.template", "Empty"));
    project.m_startupScene = config.GetString("Project.startup_scene", "");

    LUMA_LOG_INFO("Project", "loaded project '{}' (engine {}, template {})",
                  project.m_name, project.m_engineVersion,
                  ToString(project.m_template));
    return project;
}

std::vector<fs::path> DiscoverProjects(const fs::path& searchRoot) {
    std::vector<fs::path> found;
    std::error_code ec;
    if (!fs::is_directory(searchRoot, ec)) return found;

    for (const auto& entry : fs::directory_iterator(searchRoot, ec)) {
        if (ec) break;
        if (!entry.is_directory()) continue;
        for (const auto& child :
             fs::directory_iterator(entry.path(), ec)) {
            if (child.is_regular_file() &&
                child.path().extension() == Project::kExtension) {
                found.push_back(child.path());
                break;
            }
        }
    }
    return found;
}

}  // namespace Luma
