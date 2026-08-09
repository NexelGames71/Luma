#pragma once

#include <chrono>

#include "Luma/Core/Timestep.h"
#include "Luma/Core/Types.h"

// Wall-clock frame timer. Tick() returns the elapsed time since the previous
// Tick (or since construction for the first call), clamped to a sane maximum so
// a debugger pause or hitch doesn't produce a huge delta.

namespace Luma {

class FrameClock {
public:
    FrameClock() : m_last(Now()) {}

    Timestep Tick() {
        auto now = Now();
        std::chrono::duration<f32> delta = now - m_last;
        m_last = now;
        f32 seconds = delta.count();
        if (seconds > kMaxDelta) seconds = kMaxDelta;
        if (seconds < 0.0f) seconds = 0.0f;
        return Timestep(seconds);
    }

private:
    using Clock = std::chrono::steady_clock;
    static Clock::time_point Now() { return Clock::now(); }
    static constexpr f32 kMaxDelta = 0.25f;  // 250 ms cap

    Clock::time_point m_last;
};

}  // namespace Luma
