#pragma once

#include "Luma/Core/Types.h"

// Assertions and checks.
//
//   LUMA_ASSERT(cond, msg) - fatal invariant. Compiled OUT in Shipping builds.
//                            On failure the active handler is invoked; the
//                            default handler breaks into the debugger + aborts.
//   LUMA_CHECK(cond, msg)  - always compiled. Reports a failure through the
//                            active handler but is recoverable (default handler
//                            logs and returns without aborting).
//
// The handler is pluggable so tests can capture failures instead of aborting,
// and so the logging system can route asserts once it is initialized.

namespace Luma {

struct AssertInfo {
    const char* condition;
    const char* message;
    const char* file;
    const char* function;
    int line;
    bool fatal;
};

using AssertHandler = void (*)(const AssertInfo&);

namespace Detail {

// Installs a new handler and returns the previous one. Passing nullptr restores
// the default handler.
AssertHandler SetAssertHandler(AssertHandler handler);
AssertHandler GetAssertHandler();

// Invokes the currently installed handler.
void ReportAssert(const AssertInfo& info);

}  // namespace Detail
}  // namespace Luma

#define LUMA_INTERNAL_REPORT(condStr, msg, isFatal)                        \
    ::Luma::Detail::ReportAssert(::Luma::AssertInfo{                       \
        (condStr), (msg), __FILE__, static_cast<const char*>(__func__),   \
        __LINE__, (isFatal)})

#define LUMA_CHECK(cond, msg)                                              \
    do {                                                                   \
        if (!(cond)) {                                                     \
            LUMA_INTERNAL_REPORT(#cond, (msg), false);                     \
        }                                                                  \
    } while (0)

#if defined(LUMA_CONFIG_SHIPPING)
#define LUMA_ASSERT(cond, msg) ((void)0)
#else
#define LUMA_ASSERT(cond, msg)                                             \
    do {                                                                   \
        if (!(cond)) {                                                     \
            LUMA_INTERNAL_REPORT(#cond, (msg), true);                      \
        }                                                                  \
    } while (0)
#endif
