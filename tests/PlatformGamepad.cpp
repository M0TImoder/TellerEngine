#include <Base/Input.hpp>
#include <Base/Platform/Devices.hpp>
#include <Base/Platform/Gamepad.hpp>
#include <Base/Platform/System.hpp>

#include <doctest/doctest.h>

#include <SDL3/SDL.h>

namespace Base = TellerEngine::Base;
namespace Platform = TellerEngine::Base::Platform;

namespace {

void PushButton(SDL_GamepadButton button, bool down) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
    event.gbutton.button = static_cast<Uint8>(button);
    SDL_PushEvent(&event);
}

void PushAxis(SDL_GamepadAxis axis, int value) {
    SDL_Event event{};
    event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    event.gaxis.axis = static_cast<Uint8>(axis);
    event.gaxis.value = static_cast<Sint16>(value);
    SDL_PushEvent(&event);
}

} // namespace

TEST_CASE("3系統のコントローラを見分ける") {
    CHECK(Platform::KindOf(SDL_GAMEPAD_TYPE_XBOX360) == Platform::GamepadKind::Xbox);
    CHECK(Platform::KindOf(SDL_GAMEPAD_TYPE_XBOXONE) == Platform::GamepadKind::Xbox);
    CHECK(Platform::KindOf(SDL_GAMEPAD_TYPE_PS4) == Platform::GamepadKind::PlayStation);
    CHECK(Platform::KindOf(SDL_GAMEPAD_TYPE_PS5) == Platform::GamepadKind::PlayStation);
    CHECK(Platform::KindOf(SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO) ==
          Platform::GamepadKind::Nintendo);
    CHECK(Platform::KindOf(SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR) ==
          Platform::GamepadKind::Nintendo);
    CHECK(Platform::KindOf(SDL_GAMEPAD_TYPE_STANDARD) == Platform::GamepadKind::Standard);
    CHECK(Platform::KindOf(SDL_GAMEPAD_TYPE_UNKNOWN) == Platform::GamepadKind::Unknown);
}

TEST_CASE("割り当ては刻印ではなく位置で決まる") {
    CHECK(Platform::ButtonOf(SDL_GAMEPAD_BUTTON_SOUTH) == Base::Button::Confirm);
    CHECK(Platform::ButtonOf(SDL_GAMEPAD_BUTTON_EAST) == Base::Button::Cancel);
    CHECK(Platform::ButtonOf(SDL_GAMEPAD_BUTTON_NORTH) == Base::Button::Menu);
    CHECK(Platform::ButtonOf(SDL_GAMEPAD_BUTTON_START) == Base::Button::Menu);
    CHECK(Platform::ButtonOf(SDL_GAMEPAD_BUTTON_DPAD_LEFT) == Base::Button::Left);
    CHECK(Platform::ButtonOf(SDL_GAMEPAD_BUTTON_DPAD_DOWN) == Base::Button::Down);
    CHECK_FALSE(Platform::ButtonOf(SDL_GAMEPAD_BUTTON_WEST).has_value());
}

TEST_CASE("論理的なボタンから位置へ戻せる") {
    CHECK(Platform::FaceButtonOf(Base::Button::Confirm) == SDL_GAMEPAD_BUTTON_SOUTH);
    CHECK(Platform::FaceButtonOf(Base::Button::Cancel) == SDL_GAMEPAD_BUTTON_EAST);
    CHECK(Platform::FaceButtonOf(Base::Button::Menu) == SDL_GAMEPAD_BUTTON_NORTH);
    CHECK_FALSE(Platform::FaceButtonOf(Base::Button::Left).has_value());
}

TEST_CASE("刻印の文字が系統ごとに変わる") {
    CHECK(Platform::LabelText(SDL_GAMEPAD_BUTTON_LABEL_A) == "A");
    CHECK(Platform::LabelText(SDL_GAMEPAD_BUTTON_LABEL_B) == "B");
    CHECK(Platform::LabelText(SDL_GAMEPAD_BUTTON_LABEL_X) == "X");
    CHECK(Platform::LabelText(SDL_GAMEPAD_BUTTON_LABEL_Y) == "Y");
    CHECK(Platform::LabelText(SDL_GAMEPAD_BUTTON_LABEL_CROSS) == "Cross");
    CHECK(Platform::LabelText(SDL_GAMEPAD_BUTTON_LABEL_CIRCLE) == "Circle");
    CHECK(Platform::LabelText(SDL_GAMEPAD_BUTTON_LABEL_SQUARE) == "Square");
    CHECK(Platform::LabelText(SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE) == "Triangle");
    CHECK(Platform::LabelText(SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN).empty());
}

TEST_CASE("パッドのボタンが入力へ反映される") {
    auto system = Platform::System::Create(SDL_INIT_EVENTS);
    REQUIRE(system.has_value());

    Base::Input input;
    Platform::Devices devices;

    PushButton(SDL_GAMEPAD_BUTTON_SOUTH, true);
    input.BeginFrame();
    CHECK(devices.Pump(input));
    CHECK(input.Pressed(Base::Button::Confirm));

    PushButton(SDL_GAMEPAD_BUTTON_SOUTH, false);
    input.BeginFrame();
    CHECK(devices.Pump(input));
    CHECK(input.Released(Base::Button::Confirm));
}

TEST_CASE("スティックは遊びを超えると方向になる") {
    auto system = Platform::System::Create(SDL_INIT_EVENTS);
    REQUIRE(system.has_value());

    Base::Input input;
    Platform::Devices devices;

    PushAxis(SDL_GAMEPAD_AXIS_LEFTX, 4000);
    input.BeginFrame();
    devices.Pump(input);
    CHECK_FALSE(input.Held(Base::Button::Right));

    PushAxis(SDL_GAMEPAD_AXIS_LEFTX, 20000);
    input.BeginFrame();
    devices.Pump(input);
    CHECK(input.Held(Base::Button::Right));
    CHECK_FALSE(input.Held(Base::Button::Left));

    PushAxis(SDL_GAMEPAD_AXIS_LEFTX, -20000);
    input.BeginFrame();
    devices.Pump(input);
    CHECK(input.Held(Base::Button::Left));
    CHECK_FALSE(input.Held(Base::Button::Right));
}

TEST_CASE("縦のスティックは下向きが正") {
    auto system = Platform::System::Create(SDL_INIT_EVENTS);
    REQUIRE(system.has_value());

    Base::Input input;
    Platform::Devices devices;

    PushAxis(SDL_GAMEPAD_AXIS_LEFTY, 20000);
    input.BeginFrame();
    devices.Pump(input);
    CHECK(input.Held(Base::Button::Down));

    PushAxis(SDL_GAMEPAD_AXIS_LEFTY, -20000);
    input.BeginFrame();
    devices.Pump(input);
    CHECK(input.Held(Base::Button::Up));
}

TEST_CASE("遊びの幅を変えられる") {
    auto system = Platform::System::Create(SDL_INIT_EVENTS);
    REQUIRE(system.has_value());

    Base::Input input;
    Platform::Devices devices;
    CHECK(devices.DeadZone() == 8000);

    devices.SetDeadZone(2000);
    PushAxis(SDL_GAMEPAD_AXIS_LEFTX, 4000);
    input.BeginFrame();
    devices.Pump(input);
    CHECK(input.Held(Base::Button::Right));
}

TEST_CASE("キーボードとパッドはどちらでも同じボタンを押せる") {
    auto system = Platform::System::Create(SDL_INIT_EVENTS);
    REQUIRE(system.has_value());

    Base::Input input;
    Platform::Devices devices;

    SDL_Event key{};
    key.type = SDL_EVENT_KEY_DOWN;
    key.key.scancode = SDL_SCANCODE_Z;
    key.key.repeat = false;
    SDL_PushEvent(&key);
    PushButton(SDL_GAMEPAD_BUTTON_SOUTH, true);
    input.BeginFrame();
    devices.Pump(input);
    CHECK(input.Held(Base::Button::Confirm));

    // 片方を離してももう片方が押されている間は立ったまま
    PushButton(SDL_GAMEPAD_BUTTON_SOUTH, false);
    input.BeginFrame();
    devices.Pump(input);
    CHECK(input.Held(Base::Button::Confirm));

    SDL_Event release{};
    release.type = SDL_EVENT_KEY_UP;
    release.key.scancode = SDL_SCANCODE_Z;
    SDL_PushEvent(&release);
    input.BeginFrame();
    devices.Pump(input);
    CHECK_FALSE(input.Held(Base::Button::Confirm));
}

TEST_CASE("パッドが無ければ刻印も系統も出ない") {
    Platform::Devices devices;
    CHECK(devices.GamepadCount() == 0);
    CHECK(devices.Kind() == Platform::GamepadKind::Unknown);
    CHECK(devices.LabelOf(Base::Button::Confirm).empty());
    CHECK(devices.LabelOf(Base::Button::Left).empty());
}
