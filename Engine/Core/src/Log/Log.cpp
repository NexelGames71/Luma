#include "Luma/Core/Log.h"

#include <atomic>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <mutex>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace Luma {

const char* ToString(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:   return "TRACE";
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error:   return "ERROR";
        case LogLevel::Fatal:   return "FATAL";
        case LogLevel::Off:     return "OFF";
    }
    return "?";
}

namespace {

std::mutex g_mutex;
std::vector<std::shared_ptr<ILogSink>> g_sinks;
std::atomic<LogLevel> g_level{LogLevel::Trace};

// Formats "HH:MM:SS.mmm" from a time_point.
std::string FormatTime(std::chrono::system_clock::time_point tp) {
    using namespace std::chrono;
    auto t = system_clock::to_time_t(tp);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    auto ms = duration_cast<milliseconds>(tp.time_since_epoch()) % 1000;
    return std::format("{:02}:{:02}:{:02}.{:03}", tm.tm_hour, tm.tm_min,
                       tm.tm_sec, static_cast<int>(ms.count()));
}

const char* AnsiColor(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:   return "\x1b[90m";        // bright black
        case LogLevel::Debug:   return "\x1b[36m";        // cyan
        case LogLevel::Info:    return "\x1b[32m";        // green
        case LogLevel::Warning: return "\x1b[33m";        // yellow
        case LogLevel::Error:   return "\x1b[31m";        // red
        case LogLevel::Fatal:   return "\x1b[97m\x1b[41m";// white on red
        default:                return "\x1b[0m";
    }
}

class ConsoleSink final : public ILogSink {
public:
    ConsoleSink() {
#if defined(_WIN32)
        // Enable ANSI escape sequences on the Windows console.
        HANDLE handles[] = {GetStdHandle(STD_OUTPUT_HANDLE),
                            GetStdHandle(STD_ERROR_HANDLE)};
        for (HANDLE h : handles) {
            if (h == INVALID_HANDLE_VALUE || h == nullptr) continue;
            DWORD mode = 0;
            if (GetConsoleMode(h, &mode)) {
                SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }
#endif
    }

    void Write(const LogRecord& record) override {
        std::FILE* out =
            record.level >= LogLevel::Error ? stderr : stdout;
        std::fprintf(out, "%s[%s] %-5s %.*s: %.*s\x1b[0m\n",
                     AnsiColor(record.level),
                     FormatTime(record.time).c_str(), ToString(record.level),
                     static_cast<int>(record.category.size()),
                     record.category.data(),
                     static_cast<int>(record.message.size()),
                     record.message.data());
    }

    void Flush() override {
        std::fflush(stdout);
        std::fflush(stderr);
    }
};

class FileSink final : public ILogSink {
public:
    explicit FileSink(const std::string& path)
        : m_stream(path, std::ios::out | std::ios::trunc) {}

    void Write(const LogRecord& record) override {
        if (!m_stream.is_open()) return;
        m_stream << '[' << FormatTime(record.time) << "] " << ToString(record.level)
                 << ' ' << record.category << ": " << record.message << '\n';
        // Flush each record so logs survive a crash or forced termination.
        m_stream.flush();
    }

    void Flush() override {
        if (m_stream.is_open()) m_stream.flush();
    }

private:
    std::ofstream m_stream;
};

}  // namespace

namespace Log {

void Init(LogLevel level) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_level.store(level);
    g_sinks.clear();
    g_sinks.push_back(std::make_shared<ConsoleSink>());
}

void Shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto& sink : g_sinks) sink->Flush();
    g_sinks.clear();
}

void AddSink(std::shared_ptr<ILogSink> sink) {
    if (!sink) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_sinks.push_back(std::move(sink));
}

void SetLevel(LogLevel level) { g_level.store(level); }
LogLevel GetLevel() { return g_level.load(); }

void Flush() {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto& sink : g_sinks) sink->Flush();
}

std::shared_ptr<ILogSink> MakeConsoleSink() {
    return std::make_shared<ConsoleSink>();
}

std::shared_ptr<ILogSink> MakeFileSink(const std::string& path) {
    return std::make_shared<FileSink>(path);
}

namespace Detail {

LogLevel CurrentLevel() { return g_level.load(); }

void Dispatch(LogLevel level, std::string_view category,
              std::string_view message) {
    LogRecord record{level, category, message, std::this_thread::get_id(),
                     std::chrono::system_clock::now()};
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto& sink : g_sinks) sink->Write(record);
}

}  // namespace Detail
}  // namespace Log
}  // namespace Luma
