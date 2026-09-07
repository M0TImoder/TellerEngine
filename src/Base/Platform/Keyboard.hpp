#pragma once

#include <Base/Input.hpp>

#include <SDL3/SDL.h>

#include <optional>

namespace TellerEngine::Base::Platform {

// 既定の割り当て
inline std::optional<Button> ButtonOf(SDL_Scancode scancode) {
    switch (scancode) {
    case SDL_SCANCODE_LEFT:
        return Button::Left;
    case SDL_SCANCODE_RIGHT:
        return Button::Right;
    case SDL_SCANCODE_UP:
        return Button::Up;
    case SDL_SCANCODE_DOWN:
        return Button::Down;
    case SDL_SCANCODE_Z:
    case SDL_SCANCODE_RETURN:
        return Button::Confirm;
    case SDL_SCANCODE_X:
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT:
        return Button::Cancel;
    case SDL_SCANCODE_C:
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL:
        return Button::Menu;
    default:
        return std::nullopt;
    }
}

} // namespace TellerEngine::Base::Platform
