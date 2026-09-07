#include <Base/Error.hpp>
#include <Base/Input.hpp>
#include <Base/Platform/Devices.hpp>
#include <Base/Platform/Keyboard.hpp>
#include <Base/Platform/System.hpp>
#include <Base/Platform/Window.hpp>

#include <doctest/doctest.h>

#include <SDL3/SDL.h>

namespace Base = TellerEngine::Base;
namespace Platform = TellerEngine::Base::Platform;

TEST_CASE("イベントだけの初期化は画面が無くても通る") {
    auto system = Platform::System::Create(SDL_INIT_EVENTS);
    REQUIRE(system.has_value());
}

TEST_CASE("後始末したあとにもう一度初期化できる") {
    {
        auto first = Platform::System::Create(SDL_INIT_EVENTS);
        REQUIRE(first.has_value());
    }
    auto second = Platform::System::Create(SDL_INIT_EVENTS);
    CHECK(second.has_value());
}

TEST_CASE("既定の割り当てが本家の操作に対応している") {
    CHECK(Platform::ButtonOf(SDL_SCANCODE_LEFT) == Base::Button::Left);
    CHECK(Platform::ButtonOf(SDL_SCANCODE_RIGHT) == Base::Button::Right);
    CHECK(Platform::ButtonOf(SDL_SCANCODE_UP) == Base::Button::Up);
    CHECK(Platform::ButtonOf(SDL_SCANCODE_DOWN) == Base::Button::Down);
    CHECK(Platform::ButtonOf(SDL_SCANCODE_Z) == Base::Button::Confirm);
    CHECK(Platform::ButtonOf(SDL_SCANCODE_RETURN) == Base::Button::Confirm);
    CHECK(Platform::ButtonOf(SDL_SCANCODE_X) == Base::Button::Cancel);
    CHECK(Platform::ButtonOf(SDL_SCANCODE_LSHIFT) == Base::Button::Cancel);
    CHECK(Platform::ButtonOf(SDL_SCANCODE_C) == Base::Button::Menu);
    CHECK(Platform::ButtonOf(SDL_SCANCODE_LCTRL) == Base::Button::Menu);
}

TEST_CASE("割り当てのないキーは何にもならない") {
    CHECK_FALSE(Platform::ButtonOf(SDL_SCANCODE_Q).has_value());
    CHECK_FALSE(Platform::ButtonOf(SDL_SCANCODE_F1).has_value());
}

TEST_CASE("キーのイベントが入力へ反映される") {
    auto system = Platform::System::Create(SDL_INIT_EVENTS);
    REQUIRE(system.has_value());

    Base::Input input;
    Platform::Devices devices;

    SDL_Event down{};
    down.type = SDL_EVENT_KEY_DOWN;
    down.key.scancode = SDL_SCANCODE_Z;
    down.key.repeat = false;
    SDL_PushEvent(&down);

    CHECK(devices.Pump(input));
    CHECK(input.Held(Base::Button::Confirm));
    CHECK(input.Pressed(Base::Button::Confirm));

    SDL_Event up{};
    up.type = SDL_EVENT_KEY_UP;
    up.key.scancode = SDL_SCANCODE_Z;
    SDL_PushEvent(&up);

    CHECK(devices.Pump(input));
    CHECK_FALSE(input.Held(Base::Button::Confirm));
    CHECK(input.Released(Base::Button::Confirm));
}

TEST_CASE("閉じる要求で偽が返る") {
    auto system = Platform::System::Create(SDL_INIT_EVENTS);
    REQUIRE(system.has_value());

    Base::Input input;
    Platform::Devices devices;
    SDL_Event quit{};
    quit.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&quit);

    CHECK_FALSE(devices.Pump(input));
}

TEST_CASE("画面があれば窓を開ける") {
#if defined(__EMSCRIPTEN__)
    // ブラウザの外には出す先が無い
    MESSAGE("WASMでは飛ばす");
#else
    auto system = Platform::System::Create(SDL_INIT_VIDEO);
    if (!system.has_value()) {
        MESSAGE("画面が無いので飛ばす: " << system.error().context);
        return;
    }

    auto window = Platform::Window::Create("TellerEngine", 640, 480);
    REQUIRE(window.has_value());
    CHECK(window->Handle() != nullptr);
    CHECK(window->Width() == 640);
    CHECK(window->Height() == 480);
    window->SetTitle("変更後");
#endif
}
