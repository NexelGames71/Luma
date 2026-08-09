#pragma once

#include "Luma/Core/Event.h"
#include "Luma/Core/Types.h"

// Concrete event types produced by the platform layer and consumed by layers.

namespace Luma {

class WindowCloseEvent final : public Event {
public:
    LUMA_EVENT_CLASS_TYPE(WindowClose)
    LUMA_EVENT_CLASS_CATEGORY(EventCategory_Window | EventCategory_Application)
};

class WindowResizeEvent final : public Event {
public:
    WindowResizeEvent(u32 width, u32 height)
        : m_width(width), m_height(height) {}

    u32 Width() const { return m_width; }
    u32 Height() const { return m_height; }

    LUMA_EVENT_CLASS_TYPE(WindowResize)
    LUMA_EVENT_CLASS_CATEGORY(EventCategory_Window | EventCategory_Application)

private:
    u32 m_width;
    u32 m_height;
};

class WindowFocusEvent final : public Event {
public:
    LUMA_EVENT_CLASS_TYPE(WindowFocus)
    LUMA_EVENT_CLASS_CATEGORY(EventCategory_Window | EventCategory_Application)
};

class WindowLostFocusEvent final : public Event {
public:
    LUMA_EVENT_CLASS_TYPE(WindowLostFocus)
    LUMA_EVENT_CLASS_CATEGORY(EventCategory_Window | EventCategory_Application)
};

class KeyPressedEvent final : public Event {
public:
    KeyPressedEvent(i32 keycode, bool isRepeat)
        : m_keycode(keycode), m_isRepeat(isRepeat) {}

    i32 Keycode() const { return m_keycode; }
    bool IsRepeat() const { return m_isRepeat; }

    LUMA_EVENT_CLASS_TYPE(KeyPressed)
    LUMA_EVENT_CLASS_CATEGORY(EventCategory_Keyboard | EventCategory_Input)

private:
    i32 m_keycode;
    bool m_isRepeat;
};

class KeyReleasedEvent final : public Event {
public:
    explicit KeyReleasedEvent(i32 keycode) : m_keycode(keycode) {}

    i32 Keycode() const { return m_keycode; }

    LUMA_EVENT_CLASS_TYPE(KeyReleased)
    LUMA_EVENT_CLASS_CATEGORY(EventCategory_Keyboard | EventCategory_Input)

private:
    i32 m_keycode;
};

class MouseButtonPressedEvent final : public Event {
public:
    explicit MouseButtonPressedEvent(i32 button) : m_button(button) {}

    i32 Button() const { return m_button; }

    LUMA_EVENT_CLASS_TYPE(MouseButtonPressed)
    LUMA_EVENT_CLASS_CATEGORY(EventCategory_MouseButton | EventCategory_Mouse |
                              EventCategory_Input)

private:
    i32 m_button;
};

class MouseButtonReleasedEvent final : public Event {
public:
    explicit MouseButtonReleasedEvent(i32 button) : m_button(button) {}

    i32 Button() const { return m_button; }

    LUMA_EVENT_CLASS_TYPE(MouseButtonReleased)
    LUMA_EVENT_CLASS_CATEGORY(EventCategory_MouseButton | EventCategory_Mouse |
                              EventCategory_Input)

private:
    i32 m_button;
};

class MouseMovedEvent final : public Event {
public:
    MouseMovedEvent(f32 x, f32 y) : m_x(x), m_y(y) {}

    f32 X() const { return m_x; }
    f32 Y() const { return m_y; }

    LUMA_EVENT_CLASS_TYPE(MouseMoved)
    LUMA_EVENT_CLASS_CATEGORY(EventCategory_Mouse | EventCategory_Input)

private:
    f32 m_x;
    f32 m_y;
};

class MouseScrolledEvent final : public Event {
public:
    MouseScrolledEvent(f32 xOffset, f32 yOffset)
        : m_xOffset(xOffset), m_yOffset(yOffset) {}

    f32 OffsetX() const { return m_xOffset; }
    f32 OffsetY() const { return m_yOffset; }

    LUMA_EVENT_CLASS_TYPE(MouseScrolled)
    LUMA_EVENT_CLASS_CATEGORY(EventCategory_Mouse | EventCategory_Input)

private:
    f32 m_xOffset;
    f32 m_yOffset;
};

}  // namespace Luma
