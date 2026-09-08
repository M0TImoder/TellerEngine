#pragma once

#include <Base/Input.hpp>
#include <Base/Platform/Gamepad.hpp>
#include <Base/Platform/Keyboard.hpp>

#include <SDL3/SDL.h>

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace TellerEngine::Base::Platform {

// キーボードとゲームパッドをまとめて入力へ流す
class Devices {
public:
    Devices() = default;

    Devices(const Devices &) = delete;
    Devices &operator=(const Devices &) = delete;

    Devices(Devices &&other) noexcept
        : keyboard_(other.keyboard_), pad_(other.pad_), stick_(other.stick_),
          deadZone_(other.deadZone_), pads_(std::move(other.pads_)) {
        other.pads_.clear();
    }

    Devices &operator=(Devices &&other) noexcept {
        if (this != &other) {
            Close();
            keyboard_ = other.keyboard_;
            pad_ = other.pad_;
            stick_ = other.stick_;
            deadZone_ = other.deadZone_;
            pads_ = std::move(other.pads_);
            other.pads_.clear();
        }
        return *this;
    }

    ~Devices() { Close(); }

    // 既に挿さっているものを開く
    void OpenConnected() {
        int count = 0;
        SDL_JoystickID *ids = SDL_GetGamepads(&count);
        if (ids == nullptr) {
            return;
        }
        for (int i = 0; i < count; ++i) {
            Open(ids[i]);
        }
        SDL_free(ids);
    }

    // 溜まったイベントを捌く
    // 閉じる要求が来たらfalseを返す
    bool Pump(Input &input) {
        bool running = true;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            running = Handle(event) && running;
        }

        const std::uint32_t merged = keyboard_ | pad_ | stick_;
        for (std::size_t i = 0; i < kButtonCount; ++i) {
            const auto button = static_cast<Button>(i);
            input.Set(button, (merged & (std::uint32_t{1} << i)) != 0);
        }
        return running;
    }

    std::size_t GamepadCount() const { return pads_.size(); }

    GamepadKind Kind() const {
        if (pads_.empty()) {
            return GamepadKind::Unknown;
        }
        return KindOf(SDL_GetGamepadType(pads_.front()));
    }

    // 開いているパッドに刻印されている文字
    std::string_view LabelOf(Button button) const {
        if (pads_.empty()) {
            return "";
        }
        const std::optional<SDL_GamepadButton> face = FaceButtonOf(button);
        if (!face) {
            return "";
        }
        return LabelText(SDL_GetGamepadButtonLabel(pads_.front(), *face));
    }

    void SetDeadZone(int deadZone) { deadZone_ = deadZone; }
    int DeadZone() const { return deadZone_; }

private:
    static constexpr std::uint32_t Bit(Button button) {
        return std::uint32_t{1} << static_cast<std::uint32_t>(button);
    }

    bool Handle(const SDL_Event &event) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            return false;
        case SDL_EVENT_KEY_DOWN:
            if (!event.key.repeat) {
                Apply(keyboard_, ButtonOf(event.key.scancode), true);
            }
            break;
        case SDL_EVENT_KEY_UP:
            Apply(keyboard_, ButtonOf(event.key.scancode), false);
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
            Open(event.gdevice.which);
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            CloseOne(event.gdevice.which);
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            Apply(pad_, ButtonOf(static_cast<SDL_GamepadButton>(event.gbutton.button)), true);
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            Apply(pad_, ButtonOf(static_cast<SDL_GamepadButton>(event.gbutton.button)), false);
            break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            HandleAxis(event.gaxis);
            break;
        default:
            break;
        }
        return true;
    }

    void HandleAxis(const SDL_GamepadAxisEvent &axis) {
        const int value = axis.value;
        if (axis.axis == SDL_GAMEPAD_AXIS_LEFTX) {
            Set(stick_, Button::Left, value < -deadZone_);
            Set(stick_, Button::Right, value > deadZone_);
        } else if (axis.axis == SDL_GAMEPAD_AXIS_LEFTY) {
            Set(stick_, Button::Up, value < -deadZone_);
            Set(stick_, Button::Down, value > deadZone_);
        }
    }

    static void Apply(std::uint32_t &mask, const std::optional<Button> &button, bool down) {
        if (button) {
            Set(mask, *button, down);
        }
    }

    static void Set(std::uint32_t &mask, Button button, bool down) {
        if (down) {
            mask |= Bit(button);
        } else {
            mask &= ~Bit(button);
        }
    }

    void Open(SDL_JoystickID id) {
        if (SDL_Gamepad *pad = SDL_OpenGamepad(id)) {
            pads_.push_back(pad);
        }
    }

    void CloseOne(SDL_JoystickID id) {
        for (auto it = pads_.begin(); it != pads_.end(); ++it) {
            if (SDL_GetGamepadID(*it) == id) {
                SDL_CloseGamepad(*it);
                pads_.erase(it);
                break;
            }
        }
        if (pads_.empty()) {
            pad_ = 0;
            stick_ = 0;
        }
    }

    void Close() {
        for (SDL_Gamepad *pad : pads_) {
            SDL_CloseGamepad(pad);
        }
        pads_.clear();
    }

    std::uint32_t keyboard_ = 0;
    std::uint32_t pad_ = 0;
    std::uint32_t stick_ = 0;
    int deadZone_ = 8000;
    std::vector<SDL_Gamepad *> pads_;
};

} // namespace TellerEngine::Base::Platform
