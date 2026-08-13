#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <filesystem>
#include <string>

#include "Luma/Project/Project.h"

namespace fs = std::filesystem;

namespace {

std::atomic<Luma::u64> g_tempCounter{0};

// A unique temp directory for each test, cleaned up on scope exit.
struct TempDir {
    fs::path path;
    TempDir() {
        path = fs::temp_directory_path() /
               ("luma_test_" + std::to_string(g_tempCounter.fetch_add(1)) + "_" +
                std::to_string(reinterpret_cast<Luma::uptr>(this)));
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

}  // namespace

TEST_CASE("Project::Create builds the folder layout and .luma", "[project]") {
    TempDir tmp;
    Luma::ProjectDesc desc;
    desc.name = "AncientSimulation";
    desc.parentDirectory = tmp.path;
    desc.gameTemplate = Luma::GameTemplate::ThirdPerson;

    std::string err;
    auto project = Luma::Project::Create(desc, &err);
    REQUIRE(project.has_value());
    REQUIRE(err.empty());

    REQUIRE(project->Name() == "AncientSimulation");
    REQUIRE(project->Template() == Luma::GameTemplate::ThirdPerson);
    REQUIRE(fs::exists(project->ProjectFile()));
    REQUIRE(project->ProjectFile().filename() == "AncientSimulation.luma");
    REQUIRE(fs::is_directory(project->ContentDir()));
    REQUIRE(fs::is_directory(project->ConfigDir()));
    REQUIRE(fs::is_directory(project->SourceDir()));
    REQUIRE(fs::is_directory(project->SavedDir()));
}

TEST_CASE("Project::Load round-trips a created project", "[project]") {
    TempDir tmp;
    Luma::ProjectDesc desc;
    desc.name = "MyGame";
    desc.parentDirectory = tmp.path;
    desc.gameTemplate = Luma::GameTemplate::TopDown;

    auto created = Luma::Project::Create(desc);
    REQUIRE(created.has_value());

    auto loaded = Luma::Project::Load(created->ProjectFile());
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->Name() == "MyGame");
    REQUIRE(loaded->Template() == Luma::GameTemplate::TopDown);
    REQUIRE_FALSE(loaded->EngineVersion().empty());
}

TEST_CASE("Project::Create rejects duplicates and bad names", "[project]") {
    TempDir tmp;
    Luma::ProjectDesc desc;
    desc.name = "Dup";
    desc.parentDirectory = tmp.path;

    REQUIRE(Luma::Project::Create(desc).has_value());
    std::string err;
    REQUIRE_FALSE(Luma::Project::Create(desc, &err).has_value());  // duplicate
    REQUIRE_FALSE(err.empty());

    Luma::ProjectDesc bad;
    bad.name = "bad/name";
    bad.parentDirectory = tmp.path;
    REQUIRE_FALSE(Luma::Project::Create(bad).has_value());
}

TEST_CASE("DiscoverProjects finds projects one level down", "[project]") {
    TempDir tmp;
    Luma::ProjectDesc a{"Alpha", tmp.path, Luma::GameTemplate::Empty};
    Luma::ProjectDesc b{"Beta", tmp.path, Luma::GameTemplate::FirstPerson};
    REQUIRE(Luma::Project::Create(a).has_value());
    REQUIRE(Luma::Project::Create(b).has_value());

    auto found = Luma::DiscoverProjects(tmp.path);
    REQUIRE(found.size() == 2);
}

TEST_CASE("Project::Create sets up a Scenes dir and startup scene", "[project]") {
    TempDir tmp;
    Luma::ProjectDesc desc;
    desc.name = "SceneGame";
    desc.parentDirectory = tmp.path;

    auto project = Luma::Project::Create(desc);
    REQUIRE(project.has_value());

    REQUIRE(fs::is_directory(project->ScenesDir()));
    REQUIRE(project->ScenesDir() == project->ContentDir() / "Scenes");
    REQUIRE_FALSE(project->StartupScene().empty());
    // The startup scene resolves under the project's Scenes dir.
    fs::path scenePath = project->StartupScenePath();
    REQUIRE(scenePath.extension() == ".lscene");
    REQUIRE(scenePath.parent_path() == project->ScenesDir());
}

TEST_CASE("Project::Load reads back the startup scene", "[project]") {
    TempDir tmp;
    Luma::ProjectDesc desc{"SceneLoad", tmp.path, Luma::GameTemplate::Empty};
    auto created = Luma::Project::Create(desc);
    REQUIRE(created.has_value());

    auto loaded = Luma::Project::Load(created->ProjectFile());
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->StartupScene() == created->StartupScene());
    REQUIRE_FALSE(loaded->StartupScene().empty());
}

TEST_CASE("Project name validation", "[project]") {
    REQUIRE(Luma::IsValidProjectName("Ancient Simulation"));
    REQUIRE(Luma::IsValidProjectName("MyGame"));
    REQUIRE_FALSE(Luma::IsValidProjectName(""));
    REQUIRE_FALSE(Luma::IsValidProjectName("bad/name"));
    REQUIRE_FALSE(Luma::IsValidProjectName("bad:name"));
    REQUIRE_FALSE(Luma::IsValidProjectName(" leadingspace"));
}
