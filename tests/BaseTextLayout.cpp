#include <Base/TextLayout.hpp>
#include <Teller/Markup.hpp>

#include <doctest/doctest.h>

namespace Base = TellerEngine::Base;
namespace Teller = TellerEngine::Teller;

namespace {

// どの字も送り10、高さ20
Base::FontInfo Even() {
    Base::FontInfo font;
    for (char32_t letter = U'a'; letter <= U'z'; ++letter) {
        font.glyphs.push_back(Base::Glyph{letter, 0, 0, 10, 20, 10, 0});
    }
    for (char32_t letter = U'ぁ'; letter <= U'ん'; ++letter) {
        font.glyphs.push_back(Base::Glyph{letter, 0, 0, 10, 20, 10, 0});
    }
    return font;
}

Base::TextLayoutSettings SettingsFor(const Base::FontInfo &font) {
    Base::TextLayoutSettings settings;
    settings.font = &font;
    return settings;
}

} // namespace

TEST_CASE("字は送りのぶんだけ横に並ぶ") {
    const Base::FontInfo font = Even();
    const Base::Text text = Teller::Parse("abc");
    const Base::TextLayout layout = Base::LayOutText(text, SettingsFor(font));

    REQUIRE(layout.glyphs.size() == 3);
    CHECK(layout.glyphs[0].x == doctest::Approx(0.0));
    CHECK(layout.glyphs[1].x == doctest::Approx(10.0));
    CHECK(layout.glyphs[2].x == doctest::Approx(20.0));
    CHECK(layout.lines.size() == 1);
    CHECK(layout.width == doctest::Approx(30.0));
    CHECK(layout.height == doctest::Approx(20.0));
}

TEST_CASE("送りを決めればフォントより優先される") {
    const Base::FontInfo font = Even();
    Base::TextLayoutSettings settings = SettingsFor(font);
    settings.spacing = 16.0;

    const Base::Text text = Teller::Parse("ab");
    const Base::TextLayout layout = Base::LayOutText(text, settings);
    CHECK(layout.glyphs[1].x == doctest::Approx(16.0));
}

TEST_CASE("改行で次の行へ移る") {
    const Base::FontInfo font = Even();
    const Base::Text text = Teller::Parse("ab{br}cd");
    const Base::TextLayout layout = Base::LayOutText(text, SettingsFor(font));

    REQUIRE(layout.glyphs.size() == 4);
    CHECK(layout.glyphs[1].line == 0);
    CHECK(layout.glyphs[2].line == 1);
    CHECK(layout.glyphs[2].x == doctest::Approx(0.0));
    CHECK(layout.glyphs[2].y == doctest::Approx(20.0));
    CHECK(layout.lines.size() == 2);
}

TEST_CASE("続けた改行は行を空ける") {
    const Base::FontInfo font = Even();
    const Base::Text text = Teller::Parse("a{br}{br}b");
    const Base::TextLayout layout = Base::LayOutText(text, SettingsFor(font));

    CHECK(layout.lines.size() == 3);
    CHECK(layout.glyphs[1].line == 2);
    CHECK(layout.glyphs[1].y == doctest::Approx(40.0));
}

TEST_CASE("終わりを越えた位置から書く字は次の行へ送られる") {
    const Base::FontInfo font = Even();
    Base::TextLayoutSettings settings = SettingsFor(font);
    settings.lineEnd = 25.0;

    const Base::Text text = Teller::Parse("abcde");
    const Base::TextLayout layout = Base::LayOutText(text, settings);

    CHECK(layout.glyphs[0].line == 0);
    CHECK(layout.glyphs[1].line == 0);
    CHECK(layout.glyphs[2].line == 0);
    CHECK(layout.glyphs[3].line == 1);
    CHECK(layout.glyphs[3].x == doctest::Approx(0.0));
}

TEST_CASE("半分の大きさの字は送りも半分で少し下がる") {
    const Base::FontInfo font = Even();
    const Base::Text text = Teller::Parse("{small|ab}c");
    const Base::TextLayout layout = Base::LayOutText(text, SettingsFor(font));

    REQUIRE(layout.glyphs.size() == 3);
    CHECK(layout.glyphs[0].scaleX == doctest::Approx(0.5));
    CHECK(layout.glyphs[1].x == doctest::Approx(5.0));
    CHECK(layout.glyphs[2].x == doctest::Approx(10.0));
    CHECK(layout.glyphs[0].y == doctest::Approx(20.0 * Base::kHalfSizeDrop));
    CHECK(layout.glyphs[2].y == doctest::Approx(0.0));
}

TEST_CASE("ルビの分だけ行が高くなる") {
    const Base::FontInfo font = Even();
    Base::TextLayoutSettings settings = SettingsFor(font);
    settings.rubyGap = 0.0;

    const Base::Text plain = Teller::Parse("ab");
    const Base::Text ruby = Teller::Parse("{ruby:かな|ab}");
    const Base::TextLayout without = Base::LayOutText(plain, settings);
    const Base::TextLayout with = Base::LayOutText(ruby, settings);

    CHECK(without.height == doctest::Approx(20.0));
    CHECK(with.height == doctest::Approx(30.0));
    CHECK(with.lines[0].rubyHeight == doctest::Approx(10.0));
}

TEST_CASE("ルビは親の字の中央に載る") {
    const Base::FontInfo font = Even();
    Base::TextLayoutSettings settings = SettingsFor(font);
    settings.rubyGap = 0.0;

    const Base::Text text = Teller::Parse("{ruby:あい|abcd}");
    const Base::TextLayout layout = Base::LayOutText(text, settings);

    REQUIRE(layout.glyphs.size() == 6);
    CHECK(layout.glyphs[4].ruby);
    CHECK(layout.glyphs[4].x == doctest::Approx(15.0));
    CHECK(layout.glyphs[5].x == doctest::Approx(20.0));
    CHECK(layout.glyphs[4].y == doctest::Approx(0.0));
    CHECK(layout.glyphs[0].y == doctest::Approx(10.0));
}

TEST_CASE("ルビは親の終わりで出る") {
    const Base::FontInfo font = Even();
    const Base::Text text = Teller::Parse("{ruby:あ|ab}c");
    const Base::TextLayout layout = Base::LayOutText(text, SettingsFor(font));

    REQUIRE(layout.glyphs.size() == 4);
    CHECK(layout.glyphs[3].ruby);
    CHECK(layout.glyphs[3].at == layout.glyphs[1].at);
}

TEST_CASE("ルビの無い行は高くならない") {
    const Base::FontInfo font = Even();
    Base::TextLayoutSettings settings = SettingsFor(font);
    settings.rubyGap = 0.0;

    const Base::Text text = Teller::Parse("{ruby:あ|ab}{br}cd");
    const Base::TextLayout layout = Base::LayOutText(text, settings);

    REQUIRE(layout.lines.size() == 2);
    CHECK(layout.lines[0].rubyHeight == doctest::Approx(10.0));
    CHECK(layout.lines[1].rubyHeight == doctest::Approx(0.0));
    CHECK(layout.lines[1].top == doctest::Approx(30.0));
}

TEST_CASE("行をまたいだルビは行ごとに置かれる") {
    const Base::FontInfo font = Even();
    Base::TextLayoutSettings settings = SettingsFor(font);
    settings.lineEnd = 25.0;
    settings.rubyGap = 0.0;

    const Base::Text text = Teller::Parse("{ruby:あい|abcde}");
    const Base::TextLayout layout = Base::LayOutText(text, settings);

    std::size_t first = 0;
    std::size_t second = 0;
    for (const Base::PlacedGlyph &glyph : layout.glyphs) {
        if (!glyph.ruby) {
            continue;
        }
        (glyph.line == 0 ? first : second) += 1;
    }
    CHECK(first == 2);
    CHECK(second == 2);
    CHECK(layout.lines[0].rubyHeight == doctest::Approx(10.0));
    CHECK(layout.lines[1].rubyHeight == doctest::Approx(10.0));
}

TEST_CASE("フォントが無ければ何も並ばない") {
    const Base::Text text = Teller::Parse("abc");
    const Base::TextLayout layout = Base::LayOutText(text, Base::TextLayoutSettings{});
    CHECK(layout.glyphs.empty());
    CHECK(layout.lines.empty());
}

TEST_CASE("拡大すると位置も高さも伸びる") {
    const Base::FontInfo font = Even();
    Base::TextLayoutSettings settings = SettingsFor(font);
    settings.scaleX = 2.0;
    settings.scaleY = 2.0;

    const Base::Text text = Teller::Parse("ab");
    const Base::TextLayout layout = Base::LayOutText(text, settings);
    CHECK(layout.glyphs[1].x == doctest::Approx(20.0));
    CHECK(layout.height == doctest::Approx(40.0));
}
