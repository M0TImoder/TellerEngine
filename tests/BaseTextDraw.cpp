#include <Base/Canvas.hpp>
#include <Base/TextDraw.hpp>
#include <Base/TextLayout.hpp>
#include <Teller/Markup.hpp>

#include <doctest/doctest.h>

#include <cstdlib>
#include <filesystem>

#if !defined(__EMSCRIPTEN__)
#include <Base/Platform/AssetTextures.hpp>
#include <Base/Platform/SdlRenderer.hpp>
#include <Base/Platform/System.hpp>
#include <Base/Platform/Window.hpp>

#include <SDL3/SDL.h>

#include <vector>
#endif

namespace Base = TellerEngine::Base;
namespace Teller = TellerEngine::Teller;

#if !defined(__EMSCRIPTEN__)

namespace {

std::filesystem::path AssetRoot() {
    const char *value = std::getenv("TELLER_ASSETS");
    return value == nullptr ? std::filesystem::path{} : std::filesystem::path{value};
}

bool HasFonts() {
    const auto root = AssetRoot();
    return !root.empty() && std::filesystem::is_directory(root / "Fonts");
}

namespace Platform = TellerEngine::Base::Platform;

// 画面に出ている点のうち、色の付いたものを数える
struct Screen {
    std::vector<Base::Color> pixels;
    int width = 0;
    int height = 0;

    Base::Color At(int x, int y) const { return pixels[static_cast<std::size_t>(y * width + x)]; }

    std::size_t Lit() const {
        std::size_t count = 0;
        for (const Base::Color &pixel : pixels) {
            if (pixel.red != 0 || pixel.green != 0 || pixel.blue != 0) {
                count += 1;
            }
        }
        return count;
    }

    // 色の付いた点のうち一番右
    int Rightmost() const {
        int found = -1;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const Base::Color pixel = At(x, y);
                if (pixel.red != 0 || pixel.green != 0 || pixel.blue != 0) {
                    found = found > x ? found : x;
                }
            }
        }
        return found;
    }
};

Screen ReadScreen(Platform::SdlRenderer &renderer, int width, int height) {
    Screen screen;
    screen.width = width;
    screen.height = height;
    SDL_Rect rect{0, 0, width, height};
    SDL_Surface *surface = SDL_RenderReadPixels(renderer.Handle(), &rect);
    if (surface == nullptr) {
        return screen;
    }
    screen.pixels.resize(static_cast<std::size_t>(width * height));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Uint8 red = 0;
            Uint8 green = 0;
            Uint8 blue = 0;
            Uint8 alpha = 0;
            SDL_ReadSurfacePixel(surface, x, y, &red, &green, &blue, &alpha);
            screen.pixels[static_cast<std::size_t>(y * width + x)] =
                Base::Color{red, green, blue, alpha};
        }
    }
    SDL_DestroySurface(surface);
    return screen;
}

} // namespace

TEST_CASE("本家のフォントで文字が出る") {
    if (!HasFonts()) {
        MESSAGE("TELLER_ASSETS が無いので飛ばす");
        return;
    }

    auto system = Platform::System::Create(SDL_INIT_VIDEO);
    if (!system.has_value()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    constexpr int kWidth = 128;
    constexpr int kHeight = 48;
    auto window = Platform::Window::Create("TellerText", kWidth, kHeight);
    REQUIRE(window.has_value());
    auto renderer = Platform::SdlRenderer::Create(*window);
    REQUIRE(renderer.has_value());

    Platform::AssetTextures assets{renderer->Handle(), AssetRoot()};
    const auto face = assets.Font("fnt_main");
    REQUIRE(face.has_value());
    REQUIRE(face->info != nullptr);
    renderer->SetTextures(&assets.Store());

    // 二度目は読み直さない
    const auto again = assets.Font("fnt_main");
    REQUIRE(again.has_value());
    CHECK(again->image == face->image);
    CHECK(assets.Count() == 1);

    const Base::Text text = Teller::Parse("{color:red|AAAA}");
    Base::TextLayoutSettings settings;
    settings.font = face->info;
    const Base::TextLayout layout = Base::LayOutText(text, settings);
    REQUIRE(layout.glyphs.size() == 4);
    CHECK(layout.glyphs[1].x == doctest::Approx(14.0));

    Base::DrawList list;
    Base::Canvas canvas{list};
    Base::TextDrawSettings draw;
    draw.face = *face;
    Base::DrawTextLayout(canvas, layout, draw);
    renderer->Render(list);

    const Screen all = ReadScreen(*renderer, kWidth, kHeight);
    REQUIRE_FALSE(all.pixels.empty());
    CHECK(all.Lit() > 0);

    // 赤で出した字は他の成分を持たない
    CHECK(all.At(4, 8).green == 0);
    CHECK(all.Rightmost() < 56);

    // 途中まで出せば右側は空く
    list.Clear();
    draw.revealed = 2;
    Base::DrawTextLayout(canvas, layout, draw);
    renderer->Render(list);

    const Screen half = ReadScreen(*renderer, kWidth, kHeight);
    CHECK(half.Lit() > 0);
    CHECK(half.Lit() < all.Lit());
    CHECK(half.Rightmost() < 28);
}

TEST_CASE("無いフォントは失敗する") {
    if (!HasFonts()) {
        MESSAGE("TELLER_ASSETS が無いので飛ばす");
        return;
    }

    auto system = Platform::System::Create(SDL_INIT_VIDEO);
    if (!system.has_value()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }
    auto window = Platform::Window::Create("TellerMissingFont", 64, 64);
    REQUIRE(window.has_value());
    auto renderer = Platform::SdlRenderer::Create(*window);
    REQUIRE(renderer.has_value());

    Platform::AssetTextures assets{renderer->Handle(), AssetRoot()};
    CHECK_FALSE(assets.Font("fnt_存在しない").has_value());
    CHECK(assets.Count() == 0);
}

#endif

TEST_CASE("フォントが無ければ何も積まれない") {
    Base::DrawList list;
    Base::Canvas canvas{list};
    Base::TextLayout layout;
    Base::DrawTextLayout(canvas, layout, Base::TextDrawSettings{});
    CHECK(list.Size() == 0);
}
