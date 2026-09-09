#include <Base/FontInfo.hpp>

#include <doctest/doctest.h>

#include <cstdlib>
#include <filesystem>

namespace Base = TellerEngine::Base;

namespace {

std::filesystem::path AssetRoot() {
    const char *value = std::getenv("TELLER_ASSETS");
    return value == nullptr ? std::filesystem::path{} : std::filesystem::path{value};
}

bool HasFonts() {
    const auto root = AssetRoot();
    return !root.empty() && std::filesystem::is_directory(root / "Fonts");
}

Base::FontInfo Made() {
    Base::FontInfo font;
    font.emSize = 24;
    font.glyphs = {
        Base::Glyph{U'A', 0, 0, 12, 26, 14, 0},
        Base::Glyph{U'B', 16, 0, 12, 26, 14, 1},
        Base::Glyph{U'g', 40, 0, 12, 32, 14, 0},
        Base::Glyph{U'i', 32, 0, 4, 26, 6, 0},
    };
    return font;
}

} // namespace

TEST_CASE("字は二分探索で引ける") {
    const Base::FontInfo font = Made();
    REQUIRE(font.Find(U'B') != nullptr);
    CHECK(font.Find(U'B')->offset == 1);
    CHECK(font.Find(U'z') == nullptr);
}

TEST_CASE("幅は送りの合計") {
    const Base::FontInfo font = Made();
    CHECK(font.Measure("AB") == doctest::Approx(28.0));
    CHECK(font.Measure("Aig") == doctest::Approx(34.0));
    CHECK(font.Measure("") == doctest::Approx(0.0));
}

TEST_CASE("知らない字は幅を持たない") {
    const Base::FontInfo font = Made();
    CHECK(font.Measure("Az") == doctest::Approx(14.0));
}

TEST_CASE("高さは一番高い字") {
    CHECK(Made().Height() == 32);
}

TEST_CASE("本家のフォントを読める") {
    if (!HasFonts()) {
        MESSAGE("TELLER_ASSETS が無いので飛ばす");
        return;
    }

    const auto font = Base::ReadFontInfo(AssetRoot() / "Fonts" / "fnt_main.toml");
    REQUIRE(font.has_value());
    CHECK(font->emSize == 24);
    CHECK(font->glyphs.size() == 269);

    REQUIRE(font->Find(U' ') != nullptr);
    CHECK(font->Find(U' ')->shift == 6);
    REQUIRE(font->Find(U'!') != nullptr);
    CHECK(font->Find(U'!')->shift == 12);
    CHECK(font->Find(U'!')->offset == 2);
    CHECK(font->Height() == 32);
}

TEST_CASE("読めないフォントは失敗を返す") {
    const auto font = Base::ReadFontInfo("この名前のフォントは無い.toml");
    CHECK_FALSE(font.has_value());
}
