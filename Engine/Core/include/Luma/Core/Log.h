#pragma once

#include <chrono>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "Luma/Core/Types.h"

// Logging system.
//
//   LUMA_LOG_INFO("Renderer", "created {} swapchain images", count);
//
// Messages flow: macro -> Log::Print (level filter) -> Dispatch -> every sink.
// Sinks are pluggable (console, file, in-memory for tests, editor console later).

namespace Luma {

enum class LogLevel : u8 {
    Trace = 0,
    Debug,
    Info,
    Warning,
    Error,
    Fatal,
    Off,  // sentinel: disables all output when set as the active level
};

const char* ToString(LogLevel level);

struct LogRecord {
    LogLevel level;
    std::string_view category;
    std::string_view message;
    std::thread::id thread;
    std::chrono::system_clock::time_point time;
};

// A destination for log records. Implementations must be thread-safe with
// respect to their own state; the logger already serializes calls.
class ILogSink {
public:
    virtual ~ILogSink() = default;
    virtual void Write(const LogRecord& record) = 0;
    virtual void Flush() {}
};

namespace Log {

// Lifecycle. Init installs a default colored console sink; call once at startup.
void Init(LogLevel level = LogLevel::Trace);
void Shutdown();

void AddSink(std::shared_ptr<ILogSink> sink);
void SetLevel(LogLevel level);
LogLevel GetLevel();
void Flush();

// Built-in sinks.
std::shared_ptr<ILogSink> MakeConsoleSink();
std::shared_ptr<ILogSink> MakeFileSink(const std::string& path);

namespace Detail {
LogLevel CurrentLevel();
void Dispatch(LogLevel level, std::string_view category, std::string_view message);
}  // namespace Detail

template <typename... Args>
void Print(LogLevel level, std::string_view category,
           std::format_string<Args...> fmt, Args&&... args) {
    if (static_cast<u8>(level) < static_cast<u8>(Detail::CurrentLevel())) {
        return;
    }
    Detail::Dispatch(level, category,
                     std::format(fmt, std::forward<Args>(args)...));
}

}  // namespace Log
}  // namespace Luma

#define LUMA_LOG_INFO(cat, ...) \
    ::Luma::Log::Print(::Luma::LogLevel::Info, cat, __VA_ARGS__)
#define LUMA_LOG_WARN(cat, ...) \
    ::Luma::Log::Print(::Luma::LogLevel::Warning, cat, __VA_ARGS__)
#define LUMA_LOG_ERROR(cat, ...) \
    ::Luma::Log::Print(::Luma::LogLevel::Error, cat, __VA_ARGS__)
#define LUMA_LOG_FATAL(cat, ...) \
    ::Luma::Log::Print(::Luma::LogLevel::Fatal, cat, __VA_ARGS__)

#if defined(LUMA_CONFIG_SHIPPING)
#define LUMA_LOG_TRACE(cat, ...) ((void)0)
#define LUMA_LOG_DEBUG(cat, ...) ((void)0)
#else
#define LUMA_LOG_TRACE(cat, ...) \
    ::Luma::Log::Print(::Luma::LogLevel::Trace, cat, __VA_ARGS__)
#define LUMA_LOG_DEBUG(cat, ...) \
    ::Luma::Log::Print(::Luma::LogLevel::Debug, cat, __VA_ARGS__)
#endif
