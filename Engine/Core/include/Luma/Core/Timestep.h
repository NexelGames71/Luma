#pragma once

#include "Luma/Core/Types.h"

// A frame delta, in seconds. Implicitly converts to f32 for convenience.

namespace Luma {

class Timestep {
public:
    Timestep(f32 seconds = 0.0f) : m_seconds(seconds) {}

    f32 Seconds() const { return m_seconds; }
    f32 Milliseconds() const { return m_seconds * 1000.0f; }

    operator f32() const { return m_seconds; }

private:
    f32 m_seconds;
};

}  // namespace Luma
