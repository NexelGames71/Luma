#include "Luma/Core/Assert.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>

namespace Luma {
namespace {

void DefaultAssertHandler(const AssertInfo& info) {
    std::fprintf(stderr,
                 "[Luma] %s failed: (%s) %s\n         at %s:%d in %s\n",
                 info.fatal ? "ASSERT" : "CHECK", info.condition,
                 info.message ? info.message : "", info.file, info.line,
                 info.function);
    std::fflush(stderr);
    if (info.fatal) {
        LUMA_DEBUGBREAK();
        std::abort();
    }
}

std::atomic<AssertHandler> g_handler{&DefaultAssertHandler};

}  // namespace

namespace Detail {

AssertHandler SetAssertHandler(AssertHandler handler) {
    return g_handler.exchange(handler ? handler : &DefaultAssertHandler);
}

AssertHandler GetAssertHandler() { return g_handler.load(); }

void ReportAssert(const AssertInfo& info) {
    AssertHandler handler = g_handler.load();
    handler(info);
}

}  // namespace Detail
}  // namespace Luma
