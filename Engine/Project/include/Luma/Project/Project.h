#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Luma/Core/Types.h"

// The Luma project system. A project is a folder containing a `<Name>.luma`
// descriptor plus standard subdirectories. Content lives directly at the
// project root: source assets (meshes, textures, materials, scenes) in
// `Assets/`, scratch/cache data (thumbnails, logs, derived files) in
// `Intermediate/`, plus `Config/` and `Source/`. There is no `Content/` or
// `Saved/` folder inside a project.
// The `.luma` file is an INI-style descriptor (parsed with Luma::Config);
// JSON-style descriptors (projectName/defaultScene) are also accepted on
// load.

namespace Luma {

enum class GameTemplate : u8 {
    Empty = 0,
    FirstPerson,
    ThirdPerson,
    TopDown,
};

const char* ToString(GameTemplate tmpl);
GameTemplate GameTemplateFromString(std::string_view text,
                                    GameTemplate fallback = GameTemplate::Empty);

// Parameters for creating a new project.
struct ProjectDesc {
    std::string name;
    std::filesystem::path parentDirectory;  // project folder is created here
    GameTemplate gameTemplate = GameTemplate::ThirdPerson;
};

// A loaded project descriptor and its on-disk layout.
class Project {
public:
    static constexpr const char* kExtension = ".luma";
    static constexpr const char* kSceneExtension = ".lscene";

    const std::string& Name() const { return m_name; }
    GameTemplate Template() const { return m_template; }
    const std::string& EngineVersion() const { return m_engineVersion; }

    const std::filesystem::path& ProjectFile() const { return m_projectFile; }
    std::filesystem::path RootDir() const { return m_projectFile.parent_path(); }

    // Content root — source assets live directly under the project folder in
    // `Assets/` (meshes, textures, materials, scenes, audio, scripts).
    std::filesystem::path AssetsDir() const { return RootDir() / "Assets"; }
    std::filesystem::path ConfigDir() const { return RootDir() / "Config"; }
    std::filesystem::path SourceDir() const { return RootDir() / "Source"; }
    // Scratch/cache area at the project root: thumbnails, logs, screenshots,
    // derived data. The engine's legacy "Saved" concept maps here too.
    std::filesystem::path IntermediateDir() const { return RootDir() / "Intermediate"; }
    std::filesystem::path SavedDir() const { return IntermediateDir(); }
    std::filesystem::path ScenesDir() const { return AssetsDir() / "Scenes"; }

    // The startup scene, stored in the descriptor either as a project-relative
    // path (e.g. "Assets/Scenes/Main.lscene") or as a bare filename
    // (e.g. "Main.lscene"), which resolves under Assets/Scenes. May be empty
    // on older projects.
    const std::string& StartupScene() const { return m_startupScene; }

    // Absolute path to the startup scene; falls back to Assets/Scenes/Main.lscene
    // when the descriptor does not name one, so there is always a usable target.
    std::filesystem::path StartupScenePath() const {
        if (m_startupScene.empty()) {
            return ScenesDir() / (std::string("Main") + kSceneExtension);
        }
        std::filesystem::path rel(m_startupScene);
        if (rel.has_parent_path()) return RootDir() / rel;
        return ScenesDir() / rel;
    }

    // Creates the project folder, standard subdirectories and the .luma file.
    // Returns the loaded project, or nullopt with *outError set on failure.
    static std::optional<Project> Create(const ProjectDesc& desc,
                                         std::string* outError = nullptr);

    // Loads an existing .luma file.
    static std::optional<Project> Load(const std::filesystem::path& lumaFile,
                                       std::string* outError = nullptr);

private:
    std::string m_name;
    GameTemplate m_template = GameTemplate::Empty;
    std::string m_engineVersion;
    std::string m_startupScene;  // e.g. Assets/Scenes/Main.lscene or Main.lscene
    std::filesystem::path m_projectFile;
};

// Finds `.luma` files one level below `searchRoot` (i.e. each direct child
// directory that contains a project). Returns their full paths.
std::vector<std::filesystem::path> DiscoverProjects(
    const std::filesystem::path& searchRoot);

// True if `name` is usable as a project/folder name (non-empty, no path
// separators or characters illegal on Windows).
bool IsValidProjectName(std::string_view name);

}  // namespace Luma
