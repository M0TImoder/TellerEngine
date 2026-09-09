#include <Base/Text.hpp>
#include <Base/TextParser.hpp>
#include <Vanilla/LegacyMarkup.hpp>

#include <doctest/doctest.h>

#include <string>

namespace Base = TellerEngine::Base;
namespace Vanilla = TellerEngine::Vanilla;

namespace {

const Vanilla::LegacyMarkup kParser;

Base::Text Parse(std::string_view source) { return kParser.Parse(source); }

} // namespace

TEST_CASE("普通の文はそのまま通る") {
    const Base::Text text = Parse("* こんにちは");
    CHECK(text.Characters() == "* こんにちは");
    CHECK(text.Events().empty());
    REQUIRE(text.Spans().size() == 1);
}

TEST_CASE("色が変わると範囲が分かれる") {
    const Base::Text text = Parse("しろ\\Rあか\\Wしろ");

    CHECK(text.Characters() == "しろあかしろ");
    REQUIRE(text.Spans().size() == 3);
    CHECK(text.Spans()[0].style.color == Base::Color{255, 255, 255, 255});
    CHECK(text.Spans()[1].style.color == Base::Color{255, 0, 0, 255});
    CHECK(text.Spans()[2].style.color == Base::Color{255, 255, 255, 255});
}

TEST_CASE("移植元の色が全部そろっている") {
    CHECK(Vanilla::LegacyMarkup::ColorOf('R') == Base::Color{255, 0, 0, 255});
    CHECK(Vanilla::LegacyMarkup::ColorOf('G') == Base::Color{0, 255, 0, 255});
    CHECK(Vanilla::LegacyMarkup::ColorOf('W') == Base::Color{255, 255, 255, 255});
    CHECK(Vanilla::LegacyMarkup::ColorOf('Y') == Base::Color{255, 255, 0, 255});
    CHECK(Vanilla::LegacyMarkup::ColorOf('X') == Base::Color{0, 0, 0, 255});
    CHECK(Vanilla::LegacyMarkup::ColorOf('B') == Base::Color{0, 0, 255, 255});
    CHECK(Vanilla::LegacyMarkup::ColorOf('O') == Base::Color{255, 168, 64, 255});
    CHECK(Vanilla::LegacyMarkup::ColorOf('L') == Base::Color{142, 194, 253, 255});
    CHECK(Vanilla::LegacyMarkup::ColorOf('P') == Base::Color{255, 0, 255, 255});
    CHECK(Vanilla::LegacyMarkup::ColorOf('p') == Base::Color{255, 192, 212, 255});
    CHECK_FALSE(Vanilla::LegacyMarkup::ColorOf('Q').has_value());
}

TEST_CASE("表情と顔は見た目に入る") {
    const Base::Text text = Parse("\\E1\\F3かお");

    CHECK(text.Characters() == "かお");
    REQUIRE_FALSE(text.Spans().empty());
    CHECK(text.Spans()[0].style.emotion == 1);
    CHECK(text.Spans()[0].style.face == 3);
}

TEST_CASE("間は指示になる") {
    const Base::Text text = Parse("まえ^3あと");

    CHECK(text.Characters() == "まえあと");
    REQUIRE(text.Events().size() == 1);
    CHECK(text.Events()[0].kind == Base::TextEventKind::Pause);
    CHECK(text.Events()[0].value == 30);
    CHECK(text.Events()[0].at == std::string("まえ").size());
}

TEST_CASE("^0 は間にならず文字として残る") {
    const Base::Text text = Parse("あ^0い");
    CHECK(text.Characters() == "あ^0い");
    CHECK(text.Events().empty());
}

TEST_CASE("改行と終端と閉じるが指示になる") {
    const Base::Text text = Parse("いち&に/さん%");

    CHECK(text.Characters() == "いちにさん");
    REQUIRE(text.Events().size() == 3);
    CHECK(text.Events()[0].kind == Base::TextEventKind::Break);
    CHECK(text.Events()[1].kind == Base::TextEventKind::Wait);
    CHECK(text.Events()[2].kind == Base::TextEventKind::Close);
}

TEST_CASE("終端は直後の文字で待ち方が変わる") {
    CHECK(Parse("あ/").Events()[0].value == 1);
    CHECK(Parse("あ/%").Events()[0].value == 2);
    CHECK(Parse("あ/^3").Events()[0].value == 4);
    CHECK(Parse("あ/*").Events()[0].value == 6);
    CHECK(Parse("あ/^0").Events()[0].value == 1);
}

TEST_CASE("閉じるは2つ続くと最後を意味する") {
    const Base::Text single = Parse("あ%");
    CHECK(single.Events()[0].value == 0);

    const Base::Text last = Parse("あ%%");
    REQUIRE(last.Events().size() == 1);
    CHECK(last.Events()[0].value == 1);
    CHECK(last.Characters() == "あ");
}

TEST_CASE("書き手を切り替えられる") {
    CHECK(Parse("\\TS x").Spans()[0].style.typer == 10);
    CHECK(Parse("\\TR x").Spans()[0].style.typer == 76);
    CHECK(Parse("\\T0 x").Spans()[0].style.typer == 5);
    CHECK(Vanilla::LegacyMarkup::TyperOf('Q').has_value() == false);
}

TEST_CASE("大きさの切り替えが見た目に入る") {
    CHECK(Parse("\\T-ちいさい").Spans()[0].style.halfSize);
    CHECK_FALSE(Parse("\\T-あ\\T+い").Spans()[1].style.halfSize);
}

TEST_CASE("打鍵音の有無が見た目に入る") {
    CHECK_FALSE(Parse("\\S-しずか").Spans()[0].style.sound);
    CHECK(Parse("\\S-あ\\S+い").Spans()[1].style.sound);
}

TEST_CASE("効果音は指示になる") {
    const Base::Text text = Parse("\\Spおと");
    REQUIRE(text.Events().size() == 1);
    CHECK(text.Events()[0].kind == Base::TextEventKind::Sound);
    CHECK(text.Events()[0].value == 105);
}

TEST_CASE("選択肢と演出のフラグが指示になる") {
    const Base::Text text = Parse("\\C\\M4");
    REQUIRE(text.Events().size() == 2);
    CHECK(text.Events()[0].kind == Base::TextEventKind::Choice);
    CHECK(text.Events()[1].kind == Base::TextEventKind::Flag);
    CHECK(text.Events()[1].value == 4);
}

TEST_CASE("記号とアイコンが指示になる") {
    const Base::Text symbol = Parse("\\z4");
    CHECK(symbol.Events()[0].kind == Base::TextEventKind::Symbol);
    CHECK(symbol.Events()[0].value == 4);

    const Base::Text icon = Parse("\\*Z");
    CHECK(icon.Events()[0].kind == Base::TextEventKind::Icon);
    CHECK(icon.Events()[0].value == 'Z');
}

TEST_CASE("知らないコードは落として文字も残さない") {
    const Base::Text text = Parse("あ\\Qい");
    CHECK(text.Characters() == "あい");
    CHECK(text.Events().empty());
}

TEST_CASE("末尾の中途半端なコードでも落ちない") {
    CHECK(Parse("あ\\").Characters() == "あ\\");
    CHECK(Parse("あ^").Characters() == "あ^");
    CHECK(Parse("あ\\E").Characters() == "あ");
}

TEST_CASE("本家の一節をそのまま通せる") {
    const Base::Text text =
        Parse("\\XLa, la.^3 &Time to wake&up and\\R smell\\X &the^4 pain./");

    CHECK(text.Characters() == "La, la. Time to wakeup and smell the pain.");

    int breaks = 0;
    int pauses = 0;
    int waits = 0;
    for (const Base::TextEvent &event : text.Events()) {
        breaks += event.kind == Base::TextEventKind::Break ? 1 : 0;
        pauses += event.kind == Base::TextEventKind::Pause ? 1 : 0;
        waits += event.kind == Base::TextEventKind::Wait ? 1 : 0;
    }
    CHECK(breaks == 3);
    CHECK(pauses == 2);
    CHECK(waits == 1);

    // 色は 黒 → 赤 → 黒 と変わる
    REQUIRE(text.Spans().size() == 3);
    CHECK(text.Spans()[0].style.color == Base::Color{0, 0, 0, 255});
    CHECK(text.Spans()[1].style.color == Base::Color{255, 0, 0, 255});
    CHECK(text.Spans()[2].style.color == Base::Color{0, 0, 0, 255});
}

TEST_CASE("差し替えできる形になっている") {
    const Base::TextParser &parser = kParser;
    const Base::Text text = parser.Parse("\\Rあか");
    CHECK(text.Spans()[0].style.color == Base::Color{255, 0, 0, 255});
}
