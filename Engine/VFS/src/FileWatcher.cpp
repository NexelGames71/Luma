#include "Luma/VFS/FileWatcher.h"

#include <algorithm>
#include <system_error>
#include <utility>

#include "Luma/Core/Log.h"

namespace Luma::VFS {

FileWatcher::FileWatcher() = default;
FileWatcher::~FileWatcher() = default;

void FileWatcher::Watch(const std::filesystem::path& path, FileWatchCallback cb) {
    if (path.empty() || !cb) return;
    std::lock_guard<std::mutex> lock(m_mu);
    auto it = std::find_if(m_entries.begin(), m_entries.end(),
                           [&](const Entry& e) { return e.path == path; });
    Entry e;
    e.path = path;
    e.callback = std::move(cb);
    std::error_code ec;
    e.lastExists = std::filesystem::exists(path, ec);
    if (e.lastExists) {
        e.lastWrite = std::filesystem::last_write_time(path, ec);
    }
    e.hasBaseline = false;  // first Poll() establishes baseline; no event
    if (it != m_entries.end()) {
        *it = std::move(e);
    } else {
        m_entries.push_back(std::move(e));
    }
}

void FileWatcher::Unwatch(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(m_mu);
    m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(),
                                   [&](const Entry& e) { return e.path == path; }),
                    m_entries.end());
}

void FileWatcher::Clear() {
    std::lock_guard<std::mutex> lock(m_mu);
    m_entries.clear();
}

usize FileWatcher::WatchedCount() const {
    std::lock_guard<std::mutex> lock(m_mu);
    return m_entries.size();
}

void FileWatcher::Poll() {
    // Phase 1: take a snapshot under lock so callbacks (which may reschedule
    // watches) can't trip the iterator.
    std::vector<WatchedChange> changes;
    std::vector<FileWatchCallback> cbs;
    std::vector<std::filesystem::path> changedPaths;
    {
        std::lock_guard<std::mutex> lock(m_mu);
        std::error_code ec;
        for (auto& e : m_entries) {
            bool nowExists = std::filesystem::exists(e.path, ec);
            if (!e.hasBaseline) {
                e.lastExists = nowExists;
                if (nowExists) {
                    e.lastWrite = std::filesystem::last_write_time(e.path, ec);
                }
                e.hasBaseline = true;
                continue;
            }
            if (nowExists != e.lastExists) {
                changes.push_back({e.path,
                                   nowExists ? FileEvent::Created
                                             : FileEvent::Removed});
                cbs.push_back(e.callback);
                changedPaths.push_back(e.path);
                e.lastExists = nowExists;
                if (nowExists) {
                    e.lastWrite = std::filesystem::last_write_time(e.path, ec);
                }
                continue;
            }
            if (nowExists) {
                auto wt = std::filesystem::last_write_time(e.path, ec);
                if (wt != e.lastWrite) {
                    changes.push_back({e.path, FileEvent::Modified});
                    cbs.push_back(e.callback);
                    changedPaths.push_back(e.path);
                    e.lastWrite = wt;
                }
            }
        }
    }
    // Phase 2: fire callbacks outside the lock.
    for (usize i = 0; i < changes.size(); ++i) {
        if (cbs[i]) cbs[i](changes[i]);
    }
    if (!changes.empty()) {
        LUMA_LOG_TRACE("FileWatcher", "poll: {} change(s) fired", changes.size());
    }
}

}  // namespace Luma::VFS
