#include <Base/Files.hpp>
#include <Base/Platform/TrueType.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <string>
#include <vector>

#if !defined(__EMSCRIPTEN__)
#include <Base/Canvas.hpp>
#include <Base/TextDraw.hpp>
#include <Base/TextLayout.hpp>
#include <Base/Platform/AssetTextures.hpp>
#include <Base/Platform/SdlRenderer.hpp>
#include <Base/Platform/System.hpp>
#include <Base/Platform/Window.hpp>

#include <SDL3/SDL.h>
#endif

namespace Base = TellerEngine::Base;
namespace Platform = TellerEngine::Base::Platform;

namespace {

// 手元にあるフォントを1つ探す
std::filesystem::path AnyOutline() {
    const std::filesystem::path places[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/Library/Fonts/Arial.ttf",
        "C:/Windows/Fonts/arial.ttf",
    };
    for (const std::filesystem::path &path : places) {
        if (Base::Files::Exists(path)) {
            return path;
        }
    }
    return {};
}

Platform::TrueTypeSettings Ascii() {
    Platform::TrueTypeSettings settings;
    settings.ranges = {{0x20, 0x7E}};
    return settings;
}

} // namespace

TEST_CASE("フォントファイルから字を焼ける") {
    const std::filesystem::path outline = AnyOutline();
    if (outline.empty()) {
        MESSAGE("フォントファイルが見つからないので飛ばす");
        return;
    }

    const auto baked = Platform::LoadTrueType(outline, Ascii());
    REQUIRE(baked.has_value());
    CHECK(baked->info.displayName == outline.stem().string());
    CHECK(baked->info.emSize == 24);
    CHECK(baked->info.glyphs.size() == 95);
    CHECK(baked->pixels.width >= 512);
    CHECK(baked->pixels.height > 0);
    CHECK(baked->pixels.data.size() ==
          static_cast<std::size_t>(baked->pixels.width * baked->pixels.height * 4));
}

TEST_CASE("焼いた字は文字コードで引ける") {
    const std::filesystem::path outline = AnyOutline();
    if (outline.empty()) {
        MESSAGE("フォントファイルが見つからないので飛ばす");
        return;
    }

    const auto baked = Platform::LoadTrueType(outline, Ascii());
    REQUIRE(baked.has_value());

    REQUIRE(baked->info.Find(U'A') != nullptr);
    CHECK(baked->info.Find(U'A')->shift > 0);
    CHECK(baked->info.Find(U'A')->width > 0);
    CHECK(baked->info.Find(U'あ') == nullptr);
    CHECK(baked->info.Measure("AB") > baked->info.Measure("A"));
}

TEST_CASE("字の矩形は上端が揃う") {
    const std::filesystem::path outline = AnyOutline();
    if (outline.empty()) {
        MESSAGE("フォントファイルが見つからないので飛ばす");
        return;
    }

    const auto baked = Platform::LoadTrueType(outline, Ascii());
    REQUIRE(baked.has_value());

    const Base::Glyph *large = baked->info.Find(U'A');
    const Base::Glyph *small = baked->info.Find(U'x');
    const Base::Glyph *below = baked->info.Find(U'g');
    const Base::Glyph *above = baked->info.Find(U'\'');
    REQUIRE(large != nullptr);
    REQUIRE(small != nullptr);
    REQUIRE(below != nullptr);
    REQUIRE(above != nullptr);

    // 高さは行の上端から字の下端まで
    CHECK(large->height == small->height);
    CHECK(below->height > large->height);
    CHECK(above->height < large->height);
}

TEST_CASE("高さは焼いた時点で決まる") {
    const std::filesystem::path outline = AnyOutline();
    if (outline.empty()) {
        MESSAGE("フォントファイルが見つからないので飛ばす");
        return;
    }

    const auto baked = Platform::LoadTrueType(outline, Ascii());
    REQUIRE(baked.has_value());
    CHECK(baked->info.height > 0);
    CHECK(baked->info.height == baked->info.Height());
}

TEST_CASE("大きさを変えると送りも変わる") {
    const std::filesystem::path outline = AnyOutline();
    if (outline.empty()) {
        MESSAGE("フォントファイルが見つからないので飛ばす");
        return;
    }

    Platform::TrueTypeSettings small = Ascii();
    small.size = 12.0;
    Platform::TrueTypeSettings large = Ascii();
    large.size = 48.0;

    const auto thin = Platform::LoadTrueType(outline, small);
    const auto thick = Platform::LoadTrueType(outline, large);
    REQUIRE(thin.has_value());
    REQUIRE(thick.has_value());
    CHECK(thick->info.Find(U'A')->shift > thin->info.Find(U'A')->shift);
    CHECK(thick->info.Height() > thin->info.Height());
}

TEST_CASE("縁を2値にできる") {
    const std::filesystem::path outline = AnyOutline();
    if (outline.empty()) {
        MESSAGE("フォントファイルが見つからないので飛ばす");
        return;
    }

    Platform::TrueTypeSettings settings = Ascii();
    settings.antiAliasing = false;
    const auto baked = Platform::LoadTrueType(outline, settings);
    REQUIRE(baked.has_value());

    bool onlyEnds = true;
    bool white = true;
    for (std::size_t i = 0; i < baked->pixels.data.size(); i += 4) {
        const auto alpha = baked->pixels.data[i + 3];
        if (alpha != 0 && alpha != 255) {
            onlyEnds = false;
        }
        if (alpha != 0 && (baked->pixels.data[i] != 255 || baked->pixels.data[i + 1] != 255 ||
                           baked->pixels.data[i + 2] != 255)) {
            white = false;
        }
    }
    CHECK(onlyEnds);
    CHECK(white);
}

TEST_CASE("範囲を複数まとめて焼ける") {
    const std::filesystem::path outline = AnyOutline();
    if (outline.empty()) {
        MESSAGE("フォントファイルが見つからないので飛ばす");
        return;
    }

    Platform::TrueTypeSettings settings;
    settings.ranges = {{U'A', U'C'}, {U'a', U'b'}};
    const auto baked = Platform::LoadTrueType(outline, settings);
    REQUIRE(baked.has_value());
    CHECK(baked->info.glyphs.size() == 5);
    CHECK(baked->info.glyphs.front().character == U'A');
    CHECK(baked->info.glyphs.back().character == U'b');
}

TEST_CASE("フォントでないものは失敗する") {
    const std::vector<std::byte> junk(64, std::byte{0x7F});
    CHECK_FALSE(
        Platform::BakeTrueType(TellerEngine::Span<const std::byte>{junk}, {}).has_value());
    CHECK_FALSE(
        Platform::BakeTrueType(TellerEngine::Span<const std::byte>{}, {}).has_value());
    CHECK_FALSE(Platform::LoadTrueType("この名前のフォントは無い.ttf", {}).has_value());
}

TEST_CASE("範囲に字が1つも無ければ失敗する") {
    const std::filesystem::path outline = AnyOutline();
    if (outline.empty()) {
        MESSAGE("フォントファイルが見つからないので飛ばす");
        return;
    }

    Platform::TrueTypeSettings settings;
    settings.ranges = {{0xE000, 0xE010}};
    CHECK_FALSE(Platform::LoadTrueType(outline, settings).has_value());
}

#if !defined(__EMSCRIPTEN__)

TEST_CASE("定義が無ければフォントファイルを直に読む") {
    const std::filesystem::path outline = AnyOutline();
    if (outline.empty()) {
        MESSAGE("フォントファイルが見つからないので飛ばす");
        return;
    }

    auto system = Platform::System::Create(SDL_INIT_VIDEO);
    if (!system.has_value()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }
    auto window = Platform::Window::Create("TellerOutline", 64, 64);
    REQUIRE(window.has_value());
    auto renderer = Platform::SdlRenderer::Create(*window);
    REQUIRE(renderer.has_value());

    const auto root = std::filesystem::temp_directory_path() / "teller-truetype";
    std::filesystem::remove_all(root);
    REQUIRE(Base::Files::EnsureDirectory(root / "Fonts").has_value());
    std::filesystem::copy_file(outline, root / "Fonts" / "fnt_outline.ttf");

    Platform::AssetTextures assets{renderer->Handle(), root};
    const auto face = assets.Font("fnt_outline", Ascii());
    REQUIRE(face.has_value());
    REQUIRE(face->info != nullptr);
    CHECK(face->info->Find(U'A') != nullptr);
    CHECK(assets.Count() == 1);

    // 二度目は焼き直さない
    const auto again = assets.Font("fnt_outline");
    REQUIRE(again.has_value());
    CHECK(again->image == face->image);
    CHECK(assets.Count() == 1);

    CHECK_FALSE(assets.Font("fnt_無い").has_value());

    // 焼いたフォントでもそのまま並べて出せる
    renderer->SetTextures(&assets.Store());
    Base::Text text;
    text.Append("AB", Base::TextStyle{});
    Base::TextLayoutSettings layout;
    layout.font = face->info;
    Base::DrawList list;
    Base::Canvas canvas{list};
    Base::TextDrawSettings draw;
    draw.face = *face;
    Base::DrawTextLayout(canvas, Base::LayOutText(text, layout), draw);
    CHECK(list.Size() == 2);
    renderer->Render(list);

    SDL_Rect rect{0, 0, 64, 64};
    SDL_Surface *surface = SDL_RenderReadPixels(renderer->Handle(), &rect);
    REQUIRE(surface != nullptr);
    std::size_t lit = 0;
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            Uint8 red = 0;
            Uint8 green = 0;
            Uint8 blue = 0;
            Uint8 alpha = 0;
            SDL_ReadSurfacePixel(surface, x, y, &red, &green, &blue, &alpha);
            if (red != 0 || green != 0 || blue != 0) {
                lit += 1;
            }
        }
    }
    SDL_DestroySurface(surface);
    CHECK(lit > 0);

    std::filesystem::remove_all(root);
}

#endif
