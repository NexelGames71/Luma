#include <catch2/catch_test_macros.hpp>

#include "Luma/Core/Event.h"
#include "Luma/Core/Events.h"

TEST_CASE("EventDispatcher invokes the matching handler", "[core][event]") {
    Luma::WindowResizeEvent event(1920, 1080);
    Luma::EventDispatcher dispatcher(event);

    bool handledResize = false;
    Luma::u32 gotW = 0, gotH = 0;
    bool dispatched = dispatcher.Dispatch<Luma::WindowResizeEvent>(
        [&](Luma::WindowResizeEvent& e) {
            handledResize = true;
            gotW = e.Width();
            gotH = e.Height();
            return true;
        });

    REQUIRE(dispatched);
    REQUIRE(handledResize);
    REQUIRE(gotW == 1920);
    REQUIRE(gotH == 1080);
    REQUIRE(event.Handled);
}

TEST_CASE("EventDispatcher skips non-matching handlers", "[core][event]") {
    Luma::WindowCloseEvent event;
    Luma::EventDispatcher dispatcher(event);

    bool ran = false;
    bool dispatched = dispatcher.Dispatch<Luma::KeyPressedEvent>(
        [&](Luma::KeyPressedEvent&) {
            ran = true;
            return true;
        });

    REQUIRE_FALSE(dispatched);
    REQUIRE_FALSE(ran);
    REQUIRE_FALSE(event.Handled);
}

TEST_CASE("Events report their type and categories", "[core][event]") {
    Luma::KeyPressedEvent key(65, false);
    REQUIRE(key.Type() == Luma::EventType::KeyPressed);
    REQUIRE(key.IsInCategory(Luma::EventCategory_Keyboard));
    REQUIRE(key.IsInCategory(Luma::EventCategory_Input));
    REQUIRE_FALSE(key.IsInCategory(Luma::EventCategory_Mouse));
    REQUIRE(key.Keycode() == 65);

    Luma::MouseButtonPressedEvent mb(1);
    REQUIRE(mb.IsInCategory(Luma::EventCategory_MouseButton));
    REQUIRE(mb.IsInCategory(Luma::EventCategory_Mouse));
}
