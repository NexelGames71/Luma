#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

#include "Luma/Core/Log.h"

namespace {

// Captures records into memory so tests can assert on routing/filtering.
struct CapturedRecord {
    Luma::LogLevel level;
    std::string category;
    std::string message;
};

class VectorSink final : public Luma::ILogSink {
public:
    void Write(const Luma::LogRecord& record) override {
        records.push_back({record.level, std::string(record.category),
                           std::string(record.message)});
    }
    std::vector<CapturedRecord> records;
};

// Resets global logger state around a test and installs a single vector sink.
struct LogFixture {
    std::shared_ptr<VectorSink> sink = std::make_shared<VectorSink>();
    LogFixture() {
        Luma::Log::Init(Luma::LogLevel::Trace);
        // Replace the default console sink with only our capturing sink.
        Luma::Log::Shutdown();
        Luma::Log::SetLevel(Luma::LogLevel::Trace);
        Luma::Log::AddSink(sink);
    }
    ~LogFixture() { Luma::Log::Shutdown(); }
};

}  // namespace

TEST_CASE("Log routes formatted messages to sinks", "[core][log]") {
    LogFixture fx;
    LUMA_LOG_INFO("Renderer", "created {} images", 3);
    REQUIRE(fx.sink->records.size() == 1);
    REQUIRE(fx.sink->records[0].level == Luma::LogLevel::Info);
    REQUIRE(fx.sink->records[0].category == "Renderer");
    REQUIRE(fx.sink->records[0].message == "created 3 images");
}

TEST_CASE("Log filters messages below the active level", "[core][log]") {
    LogFixture fx;
    Luma::Log::SetLevel(Luma::LogLevel::Warning);

    LUMA_LOG_INFO("Core", "this is filtered out");
    REQUIRE(fx.sink->records.empty());

    LUMA_LOG_ERROR("Core", "this gets through");
    REQUIRE(fx.sink->records.size() == 1);
    REQUIRE(fx.sink->records[0].level == Luma::LogLevel::Error);
}

TEST_CASE("Log level names round-trip", "[core][log]") {
    REQUIRE(std::string(Luma::ToString(Luma::LogLevel::Warning)) == "WARN");
    REQUIRE(std::string(Luma::ToString(Luma::LogLevel::Fatal)) == "FATAL");
}
