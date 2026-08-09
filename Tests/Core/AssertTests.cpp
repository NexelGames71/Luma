#include <catch2/catch_test_macros.hpp>

#include <string>

#include "Luma/Core/Assert.h"
#include "Luma/Core/Types.h"

namespace {

struct Capture {
    bool invoked = false;
    std::string condition;
    std::string message;
    bool fatal = false;
};

Capture* g_capture = nullptr;

void CapturingHandler(const Luma::AssertInfo& info) {
    if (g_capture) {
        g_capture->invoked = true;
        g_capture->condition = info.condition;
        g_capture->message = info.message ? info.message : "";
        g_capture->fatal = info.fatal;
    }
}

// RAII guard so a captured/aborting handler never leaks into another test.
struct HandlerGuard {
    Luma::AssertHandler previous;
    explicit HandlerGuard(Capture& cap) {
        g_capture = &cap;
        previous = Luma::Detail::SetAssertHandler(&CapturingHandler);
    }
    ~HandlerGuard() {
        Luma::Detail::SetAssertHandler(previous);
        g_capture = nullptr;
    }
};

}  // namespace

TEST_CASE("LUMA_CHECK invokes the handler when the condition fails",
          "[core][assert]") {
    Capture cap;
    {
        HandlerGuard guard(cap);
        LUMA_CHECK(false, "boom");
    }
    REQUIRE(cap.invoked);
    REQUIRE(cap.message == "boom");
    REQUIRE(cap.condition == "false");
    REQUIRE_FALSE(cap.fatal);
}

TEST_CASE("LUMA_CHECK stays silent when the condition holds",
          "[core][assert]") {
    Capture cap;
    {
        HandlerGuard guard(cap);
        LUMA_CHECK(1 + 1 == 2, "should not fire");
    }
    REQUIRE_FALSE(cap.invoked);
}

TEST_CASE("Fixed-width type sizes are exact", "[core][types]") {
    REQUIRE(sizeof(Luma::u8) == 1);
    REQUIRE(sizeof(Luma::u16) == 2);
    REQUIRE(sizeof(Luma::u32) == 4);
    REQUIRE(sizeof(Luma::u64) == 8);
    REQUIRE(sizeof(Luma::f32) == 4);
    REQUIRE(sizeof(Luma::f64) == 8);
}
