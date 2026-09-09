#include <Base/TextTyper.hpp>
#include <Teller/Markup.hpp>
#include <Vanilla/LegacyMarkup.hpp>

#include <doctest/doctest.h>

namespace Base = TellerEngine::Base;
namespace Teller = TellerEngine::Teller;
namespace Vanilla = TellerEngine::Vanilla;

TEST_CASE("1ステップに1文字ずつ出る") {
    const Base::Text text = Teller::Parse("abc");
    Base::TextTyper typer;
    typer.Start(text);

    CHECK(typer.Revealed() == 0);
    typer.Step();
    CHECK(typer.Revealed() == 1);
    typer.Step();
    CHECK(typer.Revealed() == 2);
    typer.Step();
    CHECK(typer.Revealed() == 3);
    typer.Step();
    CHECK(typer.Done());
}

TEST_CASE("間隔を空けると遅くなる") {
    const Base::Text text = Teller::Parse("ab");
    Base::TextTyper typer;
    typer.Start(text, Base::TextTyperSettings{3.0, 1});

    typer.Step();
    typer.Step();
    CHECK(typer.Revealed() == 0);
    typer.Step();
    CHECK(typer.Revealed() == 1);
}

TEST_CASE("まとめて出せる") {
    const Base::Text text = Teller::Parse("abcd");
    Base::TextTyper typer;
    typer.Start(text, Base::TextTyperSettings{1.0, 2});

    typer.Step();
    CHECK(typer.Revealed() == 2);
}

TEST_CASE("速さの指定で進み方が変わる") {
    const Base::Text text = Teller::Parse("{speed:2.0|abcd}");
    Base::TextTyper typer;
    typer.Start(text);

    typer.Step();
    CHECK(typer.Revealed() == 2);
}

TEST_CASE("日本語は1文字ずつ出る") {
    const Base::Text text = Teller::Parse("あい");
    Base::TextTyper typer;
    typer.Start(text);

    typer.Step();
    CHECK(typer.Revealed() == 3);
    typer.Step();
    CHECK(typer.Revealed() == 6);
}

TEST_CASE("間は指定したステップだけ止まる") {
    const Base::Text text = Teller::Parse("a{wait:0.1}b");
    Base::TextTyper typer;
    typer.Start(text);

    typer.Step();
    CHECK(typer.Revealed() == 1);
    for (int i = 0; i < 3; ++i) {
        typer.Step();
        CHECK(typer.Revealed() == 1);
    }
    typer.Step();
    CHECK(typer.Revealed() == 2);
    CHECK(typer.Done() == false);
}

TEST_CASE("入力待ちで止まる") {
    const Base::Text text = Teller::Parse("a{end}b");
    Base::TextTyper typer;
    typer.Start(text);

    typer.Step();
    typer.Step();
    CHECK(typer.Waiting());
    CHECK(typer.Revealed() == 1);

    typer.Step();
    CHECK(typer.Revealed() == 1);

    typer.Continue();
    typer.Step();
    CHECK(typer.Revealed() == 2);
}

TEST_CASE("閉じる指示で終わる") {
    const Base::Text text = Teller::Parse("a{close:all}");
    Base::TextTyper typer;
    typer.Start(text);

    typer.Step();
    typer.Step();
    CHECK(typer.Closed());
    CHECK(typer.CloseKind() == 1);
}

TEST_CASE("改行は1文字ぶんの時間を使わない") {
    const Base::Text text = Teller::Parse("a{br}b");
    Base::TextTyper typer;
    typer.Start(text);

    typer.Step();
    typer.Step();
    CHECK(typer.Revealed() == 2);
}

TEST_CASE("その場の指示は起きたステップだけ拾える") {
    const Base::Text text = Teller::Parse("a{flag:3}b");
    Base::TextTyper typer;
    typer.Start(text);

    typer.Step();
    CHECK(typer.Fired().empty());

    typer.Step();
    REQUIRE(typer.Fired().size() == 1);
    CHECK(typer.Fired()[0].kind == Base::TextEventKind::Flag);
    CHECK(typer.Fired()[0].value == 3);

    typer.Step();
    CHECK(typer.Fired().empty());
}

TEST_CASE("飛ばすと待ちまで一気に出る") {
    const Base::Text text = Teller::Parse("abcd{end}ef");
    Base::TextTyper typer;
    typer.Start(text);

    typer.Skip();
    CHECK(typer.Revealed() == 4);
    CHECK(typer.Waiting());

    typer.Continue();
    typer.Skip();
    CHECK(typer.Revealed() == 6);
    CHECK(typer.Done());
}

TEST_CASE("飛ばしても間では止まらない") {
    const Base::Text text = Teller::Parse("a{wait:1.0}b");
    Base::TextTyper typer;
    typer.Start(text);

    typer.Skip();
    CHECK(typer.Revealed() == 2);
}

TEST_CASE("打鍵音は文字を出したステップに鳴る") {
    const Base::Text text = Teller::Parse("a b");
    Base::TextTyper typer;
    typer.Start(text);

    typer.Step();
    CHECK(typer.Spoke());
    typer.Step();
    CHECK_FALSE(typer.Spoke());
    typer.Step();
    CHECK(typer.Spoke());
}

TEST_CASE("静かな範囲では鳴らない") {
    const Base::Text text = Teller::Parse("{quiet|ab}");
    Base::TextTyper typer;
    typer.Start(text);

    typer.Step();
    CHECK_FALSE(typer.Spoke());
}

TEST_CASE("移植元の記法でも同じように打てる") {
    const Base::Text text = Vanilla::Parse("ab^3c/");
    Base::TextTyper typer;
    typer.Start(text);

    typer.Step();
    typer.Step();
    CHECK(typer.Revealed() == 2);

    for (int i = 0; i < 30; ++i) {
        typer.Step();
        CHECK(typer.Revealed() == 2);
    }
    typer.Step();
    CHECK(typer.Revealed() == 3);

    typer.Step();
    CHECK(typer.Waiting());
    CHECK(typer.WaitKind() == 1);
}

TEST_CASE("始める前は何も起きない") {
    Base::TextTyper typer;
    typer.Step();
    typer.Skip();
    typer.Continue();
    CHECK(typer.Revealed() == 0);
    CHECK(typer.Current() == Base::TextTyper::State::Idle);
}
