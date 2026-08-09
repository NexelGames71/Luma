#pragma once

#include "Luma/Core/Types.h"

// Strongly-typed event foundation. Concrete events live in Events.h. Layers
// receive events through Layer::OnEvent and use EventDispatcher to react to the
// specific types they care about, setting Handled to stop propagation.

namespace Luma {

enum class EventType : u16 {
    None = 0,
    WindowClose,
    WindowResize,
    WindowFocus,
    WindowLostFocus,
    KeyPressed,
    KeyReleased,
    MouseButtonPressed,
    MouseButtonReleased,
    MouseMoved,
    MouseScrolled,
};

// Bit flags so an event can belong to several categories.
enum EventCategory : u32 {
    EventCategory_None = 0,
    EventCategory_Application = 1u << 0,
    EventCategory_Input = 1u << 1,
    EventCategory_Keyboard = 1u << 2,
    EventCategory_Mouse = 1u << 3,
    EventCategory_MouseButton = 1u << 4,
    EventCategory_Window = 1u << 5,
};

const char* ToString(EventType type);

class Event {
public:
    virtual ~Event() = default;

    virtual EventType Type() const = 0;
    virtual const char* Name() const = 0;
    virtual u32 Categories() const = 0;

    bool IsInCategory(EventCategory category) const {
        return (Categories() & category) != 0;
    }

    bool Handled = false;
};

// Routes an event to a handler when the runtime type matches T. The handler
// returns bool (true == consumed), which is OR-ed into Event::Handled.
class EventDispatcher {
public:
    explicit EventDispatcher(Event& event) : m_event(event) {}

    template <typename T, typename Fn>
    bool Dispatch(Fn&& handler) {
        if (m_event.Type() == T::StaticType()) {
            m_event.Handled |= handler(static_cast<T&>(m_event));
            return true;
        }
        return false;
    }

private:
    Event& m_event;
};

}  // namespace Luma

#define LUMA_EVENT_CLASS_TYPE(typeName)                                  \
    static ::Luma::EventType StaticType() {                              \
        return ::Luma::EventType::typeName;                              \
    }                                                                    \
    ::Luma::EventType Type() const override { return StaticType(); }     \
    const char* Name() const override { return #typeName; }

#define LUMA_EVENT_CLASS_CATEGORY(categoryFlags)                         \
    ::Luma::u32 Categories() const override { return (categoryFlags); }
