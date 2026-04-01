#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace fw::event {

enum class EventType {
    None,
    KeyDown,
    KeyUp,
    MouseMove,
    MouseDown,
    MouseUp,
    Quit
};

struct InputEvent {
    EventType type{EventType::None};
    int32_t key{0};
    int32_t x{0};
    int32_t y{0};
    int32_t button{0};
    bool handled{false};

    std::string_view typeNameView() const noexcept {
        switch (type) {
            case EventType::KeyDown: return "keydown";
            case EventType::KeyUp: return "keyup";
            case EventType::MouseMove: return "mousemove";
            case EventType::MouseDown: return "mousedown";
            case EventType::MouseUp: return "mouseup";
            case EventType::Quit: return "quit";
            default: return "none";
        }
    }

    std::string typeName() const {
        return std::string(typeNameView());
    }
};

} // namespace fw::event

