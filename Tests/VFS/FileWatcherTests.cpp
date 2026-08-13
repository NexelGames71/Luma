#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <thread>

#include "Luma/VFS/FileWatcher.h"

using namespace Luma;
using namespace Luma::VFS;
namespace fs = std::filesystem;

namespace {

struct TempFile {
    fs::path path;

    TempFile() {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<u64> dist;
        auto base = fs::temp_directory_path() /
                    ("luma_fw_" + std::to_string(dist(gen)) + ".txt");
        path = base;
        // Touch the file so it exists with a known mtime.
        std::ofstream f(path);
        f << "init";
    }

    ~TempFile() {
        std::error_code ec;
        fs::remove(path, ec);
    }

    void WriteAndTouch(const std::string& body) {
        std::ofstream f(path, std::ios::trunc);
        f << body;
        f.close();
        // Bump mtime forward so the file_time comparison is unambiguous on
        // filesystems with low-resolution timestamps.
        auto now = fs::file_time_type::clock::now() + std::chrono::seconds(1);
        fs::last_write_time(path, now);
    }

    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;
};

}  // namespace

TEST_CASE("FileWatcher fires Created when a missing file appears", "[vfs][watch]") {
    TempFile tmp;
    fs::remove(tmp.path);

    FileWatcher watcher;
    std::atomic<int> hits{0};
    FileEvent lastEvent = FileEvent::Modified;
    watcher.Watch(tmp.path, [&](const WatchedChange& c) {
        ++hits;
        lastEvent = c.event;
    });

    // First poll establishes the baseline.
    watcher.Poll();
    REQUIRE(hits.load() == 0);

    // Now create the file.
    std::ofstream f(tmp.path);
    f << "hello";
    f.close();
    watcher.Poll();
    REQUIRE(hits.load() == 1);
    REQUIRE(lastEvent == FileEvent::Created);
}

TEST_CASE("FileWatcher fires Modified when file content changes", "[vfs][watch]") {
    TempFile tmp;

    FileWatcher watcher;
    std::atomic<int> hits{0};
    watcher.Watch(tmp.path, [](const WatchedChange&) {});  // baseline suppress
    std::atomic<int> eventHits{0};
    FileEvent lastEvent = FileEvent::Modified;
    // Replace the watcher callback with one that records after we Poll() once.
    // Simpler: register a second watcher entry for the same path with a real cb.
    // FileWatcher dedup-replaces by path, so we re-register after the baseline.
    watcher.Unwatch(tmp.path);
    watcher.Watch(tmp.path, [&](const WatchedChange& c) {
        ++eventHits;
        lastEvent = c.event;
    });

    watcher.Poll();  // baseline
    REQUIRE(eventHits.load() == 0);

    tmp.WriteAndTouch("changed");
    watcher.Poll();
    REQUIRE(eventHits.load() == 1);
    REQUIRE(lastEvent == FileEvent::Modified);
}

TEST_CASE("FileWatcher fires Removed when file is deleted", "[vfs][watch]") {
    TempFile tmp;

    FileWatcher watcher;
    std::atomic<int> hits{0};
    FileEvent lastEvent = FileEvent::Modified;
    watcher.Watch(tmp.path, [&](const WatchedChange& c) {
        ++hits;
        lastEvent = c.event;
    });

    watcher.Poll();  // baseline (file exists)
    REQUIRE(hits.load() == 0);

    fs::remove(tmp.path);
    watcher.Poll();
    REQUIRE(hits.load() == 1);
    REQUIRE(lastEvent == FileEvent::Removed);
}

TEST_CASE("FileWatcher does not fire when nothing changed", "[vfs][watch]") {
    TempFile tmp;
    FileWatcher watcher;
    std::atomic<int> hits{0};
    watcher.Watch(tmp.path, [&](const WatchedChange&) { ++hits; });

    for (int i = 0; i < 3; ++i) {
        watcher.Poll();
    }
    REQUIRE(hits.load() == 0);
}

TEST_CASE("FileWatcher Unwatch and Clear work", "[vfs][watch][mgmt]") {
    TempFile tmp;
    FileWatcher watcher;
    REQUIRE(watcher.WatchedCount() == 0);
    watcher.Watch(tmp.path, [](const WatchedChange&) {});
    REQUIRE(watcher.WatchedCount() == 1);
    watcher.Unwatch(tmp.path);
    REQUIRE(watcher.WatchedCount() == 0);

    watcher.Watch(tmp.path, [](const WatchedChange&) {});
    watcher.Watch(fs::temp_directory_path() / "another.txt",
                  [](const WatchedChange&) {});
    REQUIRE(watcher.WatchedCount() == 2);
    watcher.Clear();
    REQUIRE(watcher.WatchedCount() == 0);
}

TEST_CASE("FileWatcher can re-register a callback for the same path", "[vfs][watch]") {
    TempFile tmp;
    FileWatcher watcher;
    std::atomic<int> oldHits{0};
    std::atomic<int> newHits{0};
    watcher.Watch(tmp.path, [&](const WatchedChange&) { ++oldHits; });
    watcher.Watch(tmp.path, [&](const WatchedChange&) { ++newHits; });
    REQUIRE(watcher.WatchedCount() == 1);

    watcher.Poll();  // baseline
    REQUIRE(oldHits.load() == 0);
    REQUIRE(newHits.load() == 0);

    tmp.WriteAndTouch("new content");
    watcher.Poll();
    REQUIRE(oldHits.load() == 0);
    REQUIRE(newHits.load() == 1);
}
