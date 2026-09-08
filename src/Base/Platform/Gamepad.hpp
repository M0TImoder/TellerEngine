#pragma once

#include <Base/Input.hpp>

#include <SDL3/SDL.h>

#include <optional>
#include <string_view>

namespace TellerEngine::Base::Platform {

// ボタンの並びと刻印が変わる系統
enum class GamepadKind {
    Unknown,
    Xbox,
    PlayStation,
    Nintendo,
    Standard,
};

inline GamepadKind KindOf(SDL_GamepadType type) {
    switch (type) {
    case SDL_GAMEPAD_TYPE_XBOX360:
    case SDL_GAMEPAD_TYPE_XBOXONE:
        return GamepadKind::Xbox;
    case SDL_GAMEPAD_TYPE_PS3:
    case SDL_GAMEPAD_TYPE_PS4:
    case SDL_GAMEPAD_TYPE_PS5:
        return GamepadKind::PlayStation;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        return GamepadKind::Nintendo;
    case SDL_GAMEPAD_TYPE_STANDARD:
        return GamepadKind::Standard;
    default:
        return GamepadKind::Unknown;
    }
}

// 割り当ては刻印ではなく位置で決める
inline std::optional<Button> ButtonOf(SDL_GamepadButton button) {
    switch (button) {
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        return Button::Left;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        return Button::Right;
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
        return Button::Up;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        return Button::Down;
    case SDL_GAMEPAD_BUTTON_SOUTH:
        return Button::Confirm;
    case SDL_GAMEPAD_BUTTON_EAST:
        return Button::Cancel;
    case SDL_GAMEPAD_BUTTON_NORTH:
    case SDL_GAMEPAD_BUTTON_START:
        return Button::Menu;
    default:
        return std::nullopt;
    }
}

// 論理的なボタンに対応する位置
inline std::optional<SDL_GamepadButton> FaceButtonOf(Button button) {
    switch (button) {
    case Button::Confirm:
        return SDL_GAMEPAD_BUTTON_SOUTH;
    case Button::Cancel:
        return SDL_GAMEPAD_BUTTON_EAST;
    case Button::Menu:
        return SDL_GAMEPAD_BUTTON_NORTH;
    default:
        return std::nullopt;
    }
}

// 実機に刻印されている文字
inline std::string_view LabelText(SDL_GamepadButtonLabel label) {
    switch (label) {
    case SDL_GAMEPAD_BUTTON_LABEL_A:
        return "A";
    case SDL_GAMEPAD_BUTTON_LABEL_B:
        return "B";
    case SDL_GAMEPAD_BUTTON_LABEL_X:
        return "X";
    case SDL_GAMEPAD_BUTTON_LABEL_Y:
        return "Y";
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS:
        return "Cross";
    case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE:
        return "Circle";
    case SDL_GAMEPAD_BUTTON_LABEL_SQUARE:
        return "Square";
    case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE:
        return "Triangle";
    default:
        return "";
    }
}

} // namespace TellerEngine::Base::Platform
