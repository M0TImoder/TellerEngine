#include <Literals.hpp>

#include <doctest/doctest.h>

namespace Base = TellerEngine::Base;
namespace Teller = TellerEngine::Teller;
namespace Vanilla = TellerEngine::Vanilla;

using namespace TellerEngine::Literals;

TEST_CASE("サフィックスは実行時の解釈と同じものを作る") {
    const Base::Text literal = "{color:red|Howdy!}"_t;
    const Base::Text runtime = Teller::Parse("{color:red|Howdy!}");

    CHECK(literal.Characters() == runtime.Characters());
    REQUIRE(literal.Spans().size() == runtime.Spans().size());
    CHECK(literal.Spans()[0].style.color.red == 255);
}

TEST_CASE("長い形も同じ") {
    CHECK("abc"_teller.Characters() == "abc"_t.Characters());
    CHECK("abc"_legacy.Characters() == "abc"_l.Characters());
}

TEST_CASE("移植元の記法のサフィックス") {
    const Base::Text text = "\\Rあか"_l;
    REQUIRE(text.Spans().size() == 1);
    CHECK(text.Spans()[0].style.color.red == 255);
    CHECK(text.Spans()[0].style.color.green == 0);
}

TEST_CASE("入れ子も通る") {
    const Base::Text text = "{speed:2.0|{color:blue|Howdy!}}"_t;
    CHECK(text.Characters() == "Howdy!");
    REQUIRE(text.Spans().size() == 1);
    CHECK(text.Spans()[0].style.speed == doctest::Approx(2.0));
    CHECK(text.Spans()[0].style.color.blue == 255);
}

TEST_CASE("独自記法の誤りを検出する") {
    using Teller::Markup;
    CHECK(Markup::Diagnose("{color:red|あ}"));
    CHECK(Markup::Diagnose("}だけなら文字").problem == Base::MarkupProblem::None);
    CHECK(Markup::Diagnose("{{ は文字").problem == Base::MarkupProblem::None);

    CHECK(Markup::Diagnose("{color:red|閉じてない").problem ==
          Base::MarkupProblem::Unclosed);
    CHECK(Markup::Diagnose("{colour:red|あ}").problem ==
          Base::MarkupProblem::UnknownName);
    CHECK(Markup::Diagnose("{color|あ}").problem == Base::MarkupProblem::MissingArgument);
    CHECK(Markup::Diagnose("{color:red}").problem == Base::MarkupProblem::MissingRange);
    CHECK(Markup::Diagnose("{color:むらさき|あ}").problem ==
          Base::MarkupProblem::BadArgument);
    CHECK(Markup::Diagnose("{color:#12345|あ}").problem == Base::MarkupProblem::BadArgument);
    CHECK(Markup::Diagnose("{color:#1234gg|あ}").problem == Base::MarkupProblem::BadArgument);
    CHECK(Markup::Diagnose("{shake:つよく|あ}").problem == Base::MarkupProblem::BadArgument);
    CHECK(Markup::Diagnose("{flag:1.5}").problem == Base::MarkupProblem::BadArgument);
    CHECK(Markup::Diagnose("{close:half}").problem == Base::MarkupProblem::BadArgument);
    CHECK(Markup::Diagnose("{ruby|あ}").problem == Base::MarkupProblem::MissingArgument);
}

TEST_CASE("入れ子の中の誤りも検出する") {
    CHECK(Teller::Markup::Diagnose("{speed:2.0|{colour:red|あ}}").problem ==
          Base::MarkupProblem::UnknownName);
}

TEST_CASE("移植元の記法の誤りを検出する") {
    using Vanilla::LegacyMarkup;
    CHECK(LegacyMarkup::Diagnose("\\Rあか\\E1"));
    CHECK(LegacyMarkup::Diagnose("ふつうの文 & / %%"));

    CHECK(LegacyMarkup::Diagnose("末尾の\\").problem ==
          Base::MarkupProblem::MissingArgument);
    CHECK(LegacyMarkup::Diagnose("\\Q").problem == Base::MarkupProblem::UnknownName);
    CHECK(LegacyMarkup::Diagnose("\\E").problem == Base::MarkupProblem::MissingArgument);
    CHECK(LegacyMarkup::Diagnose("^").problem == Base::MarkupProblem::MissingArgument);
    CHECK(LegacyMarkup::Diagnose("^あ").problem == Base::MarkupProblem::BadArgument);
}

TEST_CASE("誤りの位置を指す") {
    const Base::MarkupDiagnostic found = Teller::Markup::Diagnose("あ{colour|x}");
    CHECK(found.at == 4);
}

TEST_CASE("コンパイル時に解ける") {
    static_assert(Teller::Markup::Diagnose("{color:red|Howdy!}"));
    static_assert(!Teller::Markup::Diagnose("{color:red|閉じてない"));
    static_assert(Vanilla::LegacyMarkup::Diagnose("\\Rあか"));
    static_assert(!Vanilla::LegacyMarkup::Diagnose("\\Q"));
}
