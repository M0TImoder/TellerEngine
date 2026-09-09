#include <Base/Text.hpp>
#include <Base/TextParser.hpp>
#include <Teller/Markup.hpp>

#include <doctest/doctest.h>

#include <string>

namespace Base = TellerEngine::Base;
namespace Teller = TellerEngine::Teller;

namespace {

const Teller::Markup kParser;

Base::Text Parse(std::string_view source) { return kParser.Parse(source); }

} // namespace

TEST_CASE("普通の文はそのまま通る") {
    const Base::Text text = Parse("* こんにちは。");
    CHECK(text.Characters() == "* こんにちは。");
    CHECK(text.Events().empty());
}

TEST_CASE("色を名前で指定できる") {
    const Base::Text text = Parse("しろ{color:red|あか}しろ");

    CHECK(text.Characters() == "しろあかしろ");
    REQUIRE(text.Spans().size() == 3);
    CHECK(text.Spans()[1].style.color == Base::Color{255, 0, 0, 255});
    CHECK(text.Spans()[2].style.color == Base::Color{255, 255, 255, 255});
}

TEST_CASE("名前に無い色はカラーコードで指定する") {
    CHECK(Teller::Markup::ParseColor("#ff8040") == Base::Color{255, 128, 64, 255});
    CHECK(Teller::Markup::ParseColor("#000000") == Base::Color{0, 0, 0, 255});
    CHECK_FALSE(Teller::Markup::ParseColor("#zzzzzz").has_value());
    CHECK_FALSE(Teller::Markup::ParseColor("#fff").has_value());
    CHECK_FALSE(Teller::Markup::ParseColor("crimson").has_value());

    const Base::Text text = Parse("{color:#40a8ff|そら}");
    CHECK(text.Spans()[0].style.color == Base::Color{64, 168, 255, 255});
}

TEST_CASE("名前で引ける色がそろっている") {
    for (const char *name : {"white", "black", "red", "green", "blue", "yellow", "orange",
                             "lightblue", "magenta", "pink"}) {
        INFO(name);
        CHECK(Teller::Markup::ColorOf(name).has_value());
    }
    CHECK_FALSE(Teller::Markup::ColorOf("teal").has_value());
}

TEST_CASE("入れ子にできる") {
    const Base::Text text = Parse("{speed:2.0|{color:red|Howdy!}}");

    CHECK(text.Characters() == "Howdy!");
    REQUIRE(text.Spans().size() == 1);
    CHECK(text.Spans()[0].style.speed == doctest::Approx(2.0));
    CHECK(text.Spans()[0].style.color == Base::Color{255, 0, 0, 255});
}

TEST_CASE("入れ子から出ると外側の見た目に戻る") {
    const Base::Text text = Parse("{color:red|あか{color:blue|あお}あか}");

    REQUIRE(text.Spans().size() == 3);
    CHECK(text.Spans()[0].style.color == Base::Color{255, 0, 0, 255});
    CHECK(text.Spans()[1].style.color == Base::Color{0, 0, 255, 255});
    CHECK(text.Spans()[2].style.color == Base::Color{255, 0, 0, 255});
}

TEST_CASE("開き波括弧そのものを書ける") {
    const Base::Text text = Parse("{{color:red|これは書かれない}");
    CHECK(text.Characters() == "{color:red|これは書かれない}");
    CHECK(text.Spans().size() == 1);
}

TEST_CASE("閉じ波括弧は構文の外ならそのまま通る") {
    CHECK(Parse("これは } そのまま").Characters() == "これは } そのまま");
    CHECK(Parse("{color:red|あか}のあと}").Characters() == "あかのあと}");
}

TEST_CASE("中身に開き波括弧が混じっても壊れない") {
    const Base::Text text = Parse("{color:red|{{中}");
    CHECK(text.Characters() == "{中");
    CHECK(text.Spans()[0].style.color == Base::Color{255, 0, 0, 255});
}

TEST_CASE("揺れと波と速さが見た目に入る") {
    CHECK(Parse("{shake|ゆれ}").Spans()[0].style.shake == doctest::Approx(1.0));
    CHECK(Parse("{shake:3|ゆれ}").Spans()[0].style.shake == doctest::Approx(3.0));
    CHECK(Parse("{wave:0.5|なみ}").Spans()[0].style.wave == doctest::Approx(0.5));
    CHECK(Parse("{speed:2|はやい}").Spans()[0].style.speed == doctest::Approx(2.0));
}

TEST_CASE("ルビは範囲に振られる") {
    const Base::Text text = Parse("この{ruby:かんじ|漢字}を よむ");

    CHECK(text.Characters() == "この漢字を よむ");
    REQUIRE(text.Rubies().size() == 1);
    CHECK(text.Rubies()[0].reading == "かんじ");
    CHECK(text.Rubies()[0].begin == std::string("この").size());
    CHECK(text.Rubies()[0].end == std::string("この漢字").size());
}

TEST_CASE("ルビの中も記法が効く") {
    const Base::Text text = Parse("{ruby:よみ|{color:red|漢字}}");
    CHECK(text.Characters() == "漢字");
    CHECK(text.Spans()[0].style.color == Base::Color{255, 0, 0, 255});
    REQUIRE(text.Rubies().size() == 1);
    CHECK(text.Rubies()[0].reading == "よみ");
}

TEST_CASE("読みが空のルビは付かない") {
    const Base::Text text = Parse("{ruby:|漢字}");
    CHECK(text.Characters() == "漢字");
    CHECK(text.Rubies().empty());
}

TEST_CASE("区切りは移植元と同じ意味になる") {
    const Base::Text text = Parse("いち{br}に{end}さん{close}");

    CHECK(text.Characters() == "いちにさん");
    REQUIRE(text.Events().size() == 3);
    CHECK(text.Events()[0].kind == Base::TextEventKind::Break);
    CHECK(text.Events()[1].kind == Base::TextEventKind::Wait);
    CHECK(text.Events()[2].kind == Base::TextEventKind::Close);
    CHECK(text.Events()[2].value == 0);

    CHECK(Parse("{close:all}").Events()[0].value == 1);
}

TEST_CASE("間は秒で書いてフレームになる") {
    CHECK(Parse("{wait:0.5}").Events()[0].value == 15);
    CHECK(Parse("{wait:1}").Events()[0].value == 30);
    CHECK(Parse("{wait:0}").Events()[0].value == 0);
}

TEST_CASE("顔と表情と書き手を番号で指定できる") {
    CHECK(Parse("{face:3|かお}").Spans()[0].style.face == 3);
    CHECK(Parse("{emotion:2|かお}").Spans()[0].style.emotion == 2);
    CHECK(Parse("{typer:10|もじ}").Spans()[0].style.typer == 10);
}

TEST_CASE("範囲を取らずに見た目を変えられる") {
    const Base::Text text = Parse("しろ{color:red}あか");
    REQUIRE(text.Spans().size() == 1);
    CHECK(text.Characters() == "しろあか");
    // 範囲がなければその場限り
    CHECK(text.Spans()[0].style.color == Base::Color{255, 255, 255, 255});
}

TEST_CASE("小さくしたり静かにしたりできる") {
    CHECK(Parse("{small|ちいさい}").Spans()[0].style.halfSize);
    CHECK_FALSE(Parse("{quiet|しずか}").Spans()[0].style.sound);
}

TEST_CASE("閉じていない波括弧は文字として残る") {
    const Base::Text text = Parse("あ{color:red|とじてない");
    CHECK(text.Characters() == "あ{color:red|とじてない");
}

TEST_CASE("知らない名前は落ちずに範囲だけ通す") {
    const Base::Text text = Parse("{unknown:1|なかみ}");
    CHECK(text.Characters() == "なかみ");
    CHECK(text.Events().empty());
}

TEST_CASE("差し替えできる形になっている") {
    const Base::TextParser &parser = kParser;
    CHECK(parser.Parse("{color:red|あ}").Spans()[0].style.color == Base::Color{255, 0, 0, 255});
}

TEST_CASE("移植元の一節と同じ結果になる") {
    const Base::Text text =
        Parse("{color:black|La, la.{wait:0.1}{br}Time to wake{br}up and{color:red| smell}"
              "{color:black|{br}the{wait:0.13} pain.}}{end}");

    CHECK(text.Characters() == "La, la.Time to wakeup and smellthe pain.");

    int breaks = 0;
    for (const Base::TextEvent &event : text.Events()) {
        breaks += event.kind == Base::TextEventKind::Break ? 1 : 0;
    }
    CHECK(breaks == 3);
}
