#include "Luma/Core/Event.h"

namespace Luma {

const char* ToString(EventType type) {
    switch (type) {
        case EventType::None:                return "None";
        case EventType::WindowClose:         return "WindowClose";
        case EventType::WindowResize:        return "WindowResize";
        case EventType::WindowFocus:         return "WindowFocus";
        case EventType::WindowLostFocus:     return "WindowLostFocus";
        case EventType::KeyPressed:          return "KeyPressed";
        case EventType::KeyReleased:         return "KeyReleased";
        case EventType::KeyTyped:            return "KeyTyped";
        case EventType::MouseButtonPressed:  return "MouseButtonPressed";
        case EventType::MouseButtonReleased: return "MouseButtonReleased";
        case EventType::MouseMoved:          return "MouseMoved";
        case EventType::MouseScrolled:       return "MouseScrolled";
    }
    return "Unknown";
}

}  // namespace Luma
