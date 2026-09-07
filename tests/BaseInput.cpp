#include <Base/Input.hpp>

#include <doctest/doctest.h>

namespace Base = TellerEngine::Base;

TEST_CASE("押していなければ何も立たない") {
    Base::Input input;
    CHECK_FALSE(input.Held(Base::Button::Confirm));
    CHECK_FALSE(input.Pressed(Base::Button::Confirm));
    CHECK_FALSE(input.Released(Base::Button::Confirm));
}

TEST_CASE("押した最初のフレームだけPressedが立つ") {
    Base::Input input;
    input.BeginFrame();
    input.Set(Base::Button::Confirm, true);
    CHECK(input.Held(Base::Button::Confirm));
    CHECK(input.Pressed(Base::Button::Confirm));

    input.BeginFrame();
    CHECK(input.Held(Base::Button::Confirm));
    CHECK_FALSE(input.Pressed(Base::Button::Confirm));
}

TEST_CASE("離した最初のフレームだけReleasedが立つ") {
    Base::Input input;
    input.BeginFrame();
    input.Set(Base::Button::Cancel, true);
    input.BeginFrame();
    input.Set(Base::Button::Cancel, false);

    CHECK_FALSE(input.Held(Base::Button::Cancel));
    CHECK(input.Released(Base::Button::Cancel));

    input.BeginFrame();
    CHECK_FALSE(input.Released(Base::Button::Cancel));
}

TEST_CASE("ボタンは互いに独立している") {
    Base::Input input;
    input.BeginFrame();
    input.Set(Base::Button::Left, true);
    input.Set(Base::Button::Up, true);

    CHECK(input.Held(Base::Button::Left));
    CHECK(input.Held(Base::Button::Up));
    CHECK_FALSE(input.Held(Base::Button::Right));
    CHECK_FALSE(input.Held(Base::Button::Down));
    CHECK_FALSE(input.Held(Base::Button::Menu));
}

TEST_CASE("まとめて落とせる") {
    Base::Input input;
    input.Set(Base::Button::Left, true);
    input.Set(Base::Button::Menu, true);
    input.Clear();
    CHECK_FALSE(input.Held(Base::Button::Left));
    CHECK_FALSE(input.Held(Base::Button::Menu));
}

TEST_CASE("控えを取って戻すと押し始めの判定も揃う") {
    Base::Input input;
    input.BeginFrame();
    input.Set(Base::Button::Confirm, true);
    const Base::Input::Snapshot saved = input.Save();
    CHECK(input.Pressed(Base::Button::Confirm));

    input.BeginFrame();
    input.Set(Base::Button::Confirm, false);
    CHECK_FALSE(input.Pressed(Base::Button::Confirm));

    input.Restore(saved);
    CHECK(input.Held(Base::Button::Confirm));
    CHECK(input.Pressed(Base::Button::Confirm));
}

TEST_CASE("コンパイル時にも組み立てられる") {
    constexpr Base::Input input = [] {
        Base::Input built;
        built.BeginFrame();
        built.Set(Base::Button::Down, true);
        return built;
    }();
    static_assert(input.Pressed(Base::Button::Down));
    static_assert(!input.Held(Base::Button::Up));
    CHECK(input.Held(Base::Button::Down));
}
