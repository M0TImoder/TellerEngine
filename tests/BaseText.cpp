#include <Base/Text.hpp>

#include <doctest/doctest.h>

#include <string>

namespace Base = TellerEngine::Base;

TEST_CASE("何も入れなければ空") {
    const Base::Text text;
    CHECK(text.Empty());
    CHECK(text.Characters().empty());
    CHECK(text.Spans().empty());
    CHECK(text.Events().empty());
}

TEST_CASE("文字を足すと範囲が1つできる") {
    Base::Text text;
    text.Append("こんにちは", Base::TextStyle{});

    REQUIRE(text.Spans().size() == 1);
    CHECK(text.Characters() == "こんにちは");
    CHECK(text.Spans()[0].begin == 0);
    CHECK(text.Spans()[0].end == text.Length());
}

TEST_CASE("同じ見た目が続けば範囲はまとまる") {
    Base::Text text;
    text.Append("あ", Base::TextStyle{});
    text.Append("い", Base::TextStyle{});
    text.Append("う", Base::TextStyle{});

    CHECK(text.Spans().size() == 1);
    CHECK(text.Characters() == "あいう");
}

TEST_CASE("見た目が変われば範囲が分かれる") {
    Base::Text text;
    Base::TextStyle white;
    Base::TextStyle red;
    red.color = Base::Color{255, 0, 0, 255};

    text.Append("しろ", white);
    text.Append("あか", red);
    text.Append("しろ", white);

    REQUIRE(text.Spans().size() == 3);
    CHECK(text.Spans()[1].style.color == Base::Color{255, 0, 0, 255});
    CHECK(text.Spans()[0].end == text.Spans()[1].begin);
    CHECK(text.Spans()[1].end == text.Spans()[2].begin);
}

TEST_CASE("空の追加は範囲を作らない") {
    Base::Text text;
    text.Append("", Base::TextStyle{});
    CHECK(text.Spans().empty());
    CHECK(text.Empty());
}

TEST_CASE("指示は今の位置に挟まる") {
    Base::Text text;
    text.Append("まえ", Base::TextStyle{});
    text.Add(Base::TextEventKind::Break);
    text.Append("あと", Base::TextStyle{});
    text.Add(Base::TextEventKind::Wait);

    REQUIRE(text.Events().size() == 2);
    CHECK(text.Events()[0].kind == Base::TextEventKind::Break);
    CHECK(text.Events()[0].at == std::string("まえ").size());
    CHECK(text.Events()[1].kind == Base::TextEventKind::Wait);
    CHECK(text.Events()[1].at == text.Length());
}

TEST_CASE("指示は値を持てる") {
    Base::Text text;
    text.Add(Base::TextEventKind::Pause, 3);
    text.Add(Base::TextEventKind::Flag, 20);

    CHECK(text.Events()[0].value == 3);
    CHECK(text.Events()[1].value == 20);
}

TEST_CASE("指示だけでも空ではない") {
    Base::Text text;
    text.Add(Base::TextEventKind::Close);
    CHECK_FALSE(text.Empty());
    CHECK(text.Characters().empty());
}

TEST_CASE("位置から見た目を引ける") {
    Base::Text text;
    Base::TextStyle white;
    Base::TextStyle yellow;
    yellow.color = Base::Color{255, 255, 0, 255};

    text.Append("ab", white);
    text.Append("cd", yellow);

    CHECK(text.StyleAt(0).color == Base::Color{255, 255, 255, 255});
    CHECK(text.StyleAt(1).color == Base::Color{255, 255, 255, 255});
    CHECK(text.StyleAt(2).color == Base::Color{255, 255, 0, 255});
    CHECK(text.StyleAt(3).color == Base::Color{255, 255, 0, 255});

    // 範囲の外は最後の見た目
    CHECK(text.StyleAt(99).color == Base::Color{255, 255, 0, 255});
}

TEST_CASE("顔と表情と書き手も見た目に入る") {
    Base::Text text;
    Base::TextStyle style;
    style.face = 3;
    style.emotion = 2;
    style.typer = 4;
    style.halfSize = true;
    style.sound = false;
    text.Append("x", style);

    const Base::TextStyle back = text.StyleAt(0);
    CHECK(back.face == 3);
    CHECK(back.emotion == 2);
    CHECK(back.typer == 4);
    CHECK(back.halfSize);
    CHECK_FALSE(back.sound);
}

TEST_CASE("空にすれば全部消える") {
    Base::Text text;
    text.Append("あ", Base::TextStyle{});
    text.Add(Base::TextEventKind::Break);
    text.Clear();

    CHECK(text.Empty());
    CHECK(text.Spans().empty());
    CHECK(text.Events().empty());
}
