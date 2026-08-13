#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Luma/Core/Types.h"

// File change notifier. Register a path + callback; call Poll() periodically
// (once per frame, for instance) and the watcher fires callbacks for any
// changes it observed since the last poll.
//
// Implementation: polls each watched path's `last_write_time` + `exists`
// state. This is portable across Windows and POSIX without native APIs and
// has predictable cost (one stat per watched entry per poll). The asset
// subsystem (Phase 3/5) is the expected caller.
//
// This is NOT a recursive directory watcher. Watch a file, or watch a
// directory and treat it as "any direct child changed" (one stat per poll).

namespace Luma::VFS {

enum class FileEvent : u8 {
    Created,    // path did not exist on the previous poll
    Modified,  // path's last_write_time changed
    Removed,   // path no longer exists
};

struct WatchedChange {
    std::filesystem::path path;
    FileEvent event = FileEvent::Modified;
};

using FileWatchCallback = std::function<void(const WatchedChange&)>;

class FileWatcher {
public:
    FileWatcher();
    ~FileWatcher();

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    // Begin watching `path`. If a callback for the same path is already
    // registered it is replaced. Watching a non-existent path is allowed —
    // the watcher will fire Created the first time the file appears.
    void Watch(const std::filesystem::path& path, FileWatchCallback cb);
    // Stop watching `path`. No-op if it wasn't watched.
    void Unwatch(const std::filesystem::path& path);
    void Clear();

    // Scan every watched entry, fire callbacks for any change since the last
    // poll. Each callback runs with the watcher lock RELEASED so the callback
    // is free to call Watch/Unwatch on this same watcher (e.g. reschedule a
    // re-watch after a reload).
    void Poll();

    usize WatchedCount() const;

private:
    struct Entry {
        std::filesystem::path path;
        FileWatchCallback callback;
        std::filesystem::file_time_type lastWrite{};
        bool lastExists = false;
        bool hasBaseline = false;
    };

    mutable std::mutex m_mu;
    std::vector<Entry> m_entries;
};

}  // namespace Luma::VFS
