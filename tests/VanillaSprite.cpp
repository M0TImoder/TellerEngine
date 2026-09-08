#include <Base/Assets.hpp>
#include <Base/Canvas.hpp>
#include <Base/Draw.hpp>
#include <Base/BackgroundInfo.hpp>
#include <Base/SpriteInfo.hpp>

#include <doctest/doctest.h>

#include <cstdlib>
#include <filesystem>
#include <string>

#if !defined(__EMSCRIPTEN__)
#include <Base/Platform/AssetTextures.hpp>
#include <Base/Platform/SdlRenderer.hpp>
#include <Base/Platform/System.hpp>
#include <Base/Platform/Textures.hpp>
#include <Base/Platform/Window.hpp>

#include <SDL3/SDL.h>

#include <optional>
#include <vector>
#endif

namespace Base = TellerEngine::Base;

namespace {

// 抽出先は環境変数で渡す
std::filesystem::path AssetRoot() {
    const char *value = std::getenv("TELLER_ASSETS");
    return value == nullptr ? std::filesystem::path{} : std::filesystem::path{value};
}

bool HasAssets() {
    const auto root = AssetRoot();
    return !root.empty() && std::filesystem::is_directory(root / "Sprites");
}

} // namespace

TEST_CASE("本家のスプライトの定義を読める") {
    if (!HasAssets()) {
        MESSAGE("TELLER_ASSETS が無いので飛ばす");
        return;
    }

    const auto info = Base::ReadSpriteInfo(AssetRoot() / "Sprites" / "spr_heart.toml");
    REQUIRE(info.has_value());
    CHECK(info->width == 16);
    CHECK(info->height == 16);
    CHECK(info->frameCount == 2);
    CHECK(info->maskCount == 1);
}

TEST_CASE("原点を持つスプライトも読める") {
    if (!HasAssets()) {
        MESSAGE("TELLER_ASSETS が無いので飛ばす");
        return;
    }

    const auto info = Base::ReadSpriteInfo(AssetRoot() / "Sprites" / "spr_6glove.toml");
    REQUIRE(info.has_value());
    CHECK(info->width == 58);
    CHECK(info->height == 48);
    CHECK(info->originX == doctest::Approx(29.0));
    CHECK(info->originY == doctest::Approx(23.0));
}

TEST_CASE("無い定義を読むと失敗する") {
    const auto info = Base::ReadSpriteInfo("/存在しない/spr.toml");
    CHECK_FALSE(info.has_value());
}

TEST_CASE("コマの並びが定義の数だけ出る") {
    if (!HasAssets()) {
        MESSAGE("TELLER_ASSETS が無いので飛ばす");
        return;
    }

    const auto frames = Base::SpriteFrames(AssetRoot(), "spr_heart", 2);
    REQUIRE(frames.size() == 2);
    CHECK(frames[0].filename() == "0.png");
    CHECK(std::filesystem::is_regular_file(frames[0]));
    CHECK(std::filesystem::is_regular_file(frames[1]));
}

#if !defined(__EMSCRIPTEN__)

TEST_CASE("本家のスプライトが正しい座標に出る") {
    if (!HasAssets()) {
        MESSAGE("TELLER_ASSETS が無いので飛ばす");
        return;
    }

    auto system = TellerEngine::Base::Platform::System::Create(SDL_INIT_VIDEO);
    if (!system.has_value()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }
    auto window = TellerEngine::Base::Platform::Window::Create("TellerHeart", 128, 128);
    REQUIRE(window.has_value());
    auto renderer = TellerEngine::Base::Platform::SdlRenderer::Create(*window);
    REQUIRE(renderer.has_value());

    const auto info = Base::ReadSpriteInfo(AssetRoot() / "Sprites" / "spr_heart.toml");
    REQUIRE(info.has_value());

    TellerEngine::Base::Platform::Textures textures{renderer->Handle()};
    const auto frames = Base::SpriteFrames(AssetRoot(), "spr_heart", info->frameCount);
    const auto image = textures.Add(
        TellerEngine::Span<const std::filesystem::path>{frames}, info->originX, info->originY);
    REQUIRE(image.has_value());
    renderer->SetTextures(&textures);

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.Sprite(*image, 0, 40.0, 40.0);
    renderer->Render(list);

    const auto readPixel = [&](int x, int y) {
        SDL_Rect rect{x, y, 1, 1};
        SDL_Surface *surface = SDL_RenderReadPixels(renderer->Handle(), &rect);
        REQUIRE(surface != nullptr);
        Uint8 red = 0;
        Uint8 green = 0;
        Uint8 blue = 0;
        Uint8 alpha = 0;
        SDL_ReadSurfacePixel(surface, 0, 0, &red, &green, &blue, &alpha);
        SDL_DestroySurface(surface);
        return Base::Color{red, green, blue, alpha};
    };

    // ハートの中心は赤い
    const Base::Color center = readPixel(40 + 8, 40 + 8);
    CHECK(center.red > 200);
    CHECK(center.green < 60);
    CHECK(center.blue < 60);

    // 置いた場所の外は背景のまま
    const Base::Color outside = readPixel(40 + 24, 40 + 24);
    CHECK(outside.red == 0);

    // 左上の角は透明なので背景が出る
    const Base::Color corner = readPixel(40, 40);
    CHECK(corner.red == 0);
}

#endif

TEST_CASE("背景の定義を読める") {
    if (!HasAssets()) {
        MESSAGE("TELLER_ASSETS が無いので飛ばす");
        return;
    }

    const auto info =
        Base::ReadBackgroundInfo(AssetRoot() / "Backgrounds" / "background0.toml");
    REQUIRE(info.has_value());
    CHECK_FALSE(info->smooth);
}

TEST_CASE("無い背景の定義は失敗する") {
    const auto info = Base::ReadBackgroundInfo("/存在しない/bg.toml");
    CHECK_FALSE(info.has_value());
}

#if !defined(__EMSCRIPTEN__)

TEST_CASE("名前でスプライトと背景を読み込める") {
    if (!HasAssets()) {
        MESSAGE("TELLER_ASSETS が無いので飛ばす");
        return;
    }

    auto system = TellerEngine::Base::Platform::System::Create(SDL_INIT_VIDEO);
    if (!system.has_value()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }
    auto window = TellerEngine::Base::Platform::Window::Create("TellerAssets", 64, 64);
    REQUIRE(window.has_value());
    auto renderer = TellerEngine::Base::Platform::SdlRenderer::Create(*window);
    REQUIRE(renderer.has_value());

    TellerEngine::Base::Platform::AssetTextures assets{renderer->Handle(), AssetRoot()};

    const auto heart = assets.Sprite("spr_heart");
    REQUIRE(heart.has_value());
    const auto background = assets.Background("background0");
    REQUIRE(background.has_value());
    CHECK(*heart != *background);
    CHECK(assets.Count() == 2);

    // 二度目は読み直さない
    const auto again = assets.Sprite("spr_heart");
    REQUIRE(again.has_value());
    CHECK(*again == *heart);
    CHECK(assets.Count() == 2);

    // 定義も取れる
    const Base::SpriteInfo *info = assets.InfoOf("spr_heart");
    REQUIRE(info != nullptr);
    CHECK(info->width == 16);
    CHECK(info->frameCount == 2);

    // 背景はコマが1枚で原点を持たない
    const auto *entry = assets.Store().Find(*background);
    REQUIRE(entry != nullptr);
    CHECK(entry->frames.size() == 1);
    CHECK(entry->originX == doctest::Approx(0.0));
}

TEST_CASE("無い名前は失敗する") {
    if (!HasAssets()) {
        MESSAGE("TELLER_ASSETS が無いので飛ばす");
        return;
    }

    auto system = TellerEngine::Base::Platform::System::Create(SDL_INIT_VIDEO);
    if (!system.has_value()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }
    auto window = TellerEngine::Base::Platform::Window::Create("TellerMissing", 64, 64);
    REQUIRE(window.has_value());
    auto renderer = TellerEngine::Base::Platform::SdlRenderer::Create(*window);
    REQUIRE(renderer.has_value());

    TellerEngine::Base::Platform::AssetTextures assets{renderer->Handle(), AssetRoot()};
    CHECK_FALSE(assets.Sprite("spr_存在しない").has_value());
    CHECK_FALSE(assets.Background("bg_存在しない").has_value());
    CHECK(assets.Count() == 0);
}

TEST_CASE("本家の背景が出る") {
    if (!HasAssets()) {
        MESSAGE("TELLER_ASSETS が無いので飛ばす");
        return;
    }

    auto system = TellerEngine::Base::Platform::System::Create(SDL_INIT_VIDEO);
    if (!system.has_value()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }
    auto window = TellerEngine::Base::Platform::Window::Create("TellerBackground", 64, 64);
    REQUIRE(window.has_value());
    auto renderer = TellerEngine::Base::Platform::SdlRenderer::Create(*window);
    REQUIRE(renderer.has_value());

    TellerEngine::Base::Platform::AssetTextures assets{renderer->Handle(), AssetRoot()};
    const auto background = assets.Background("background0");
    REQUIRE(background.has_value());
    renderer->SetTextures(&assets.Store());

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.Sprite(*background, 0, 0.0, 0.0);
    renderer->Render(list);

    SDL_Rect rect{2, 2, 1, 1};
    SDL_Surface *surface = SDL_RenderReadPixels(renderer->Handle(), &rect);
    REQUIRE(surface != nullptr);
    Uint8 red = 0;
    Uint8 green = 0;
    Uint8 blue = 0;
    Uint8 alpha = 0;
    SDL_ReadSurfacePixel(surface, 0, 0, &red, &green, &blue, &alpha);
    SDL_DestroySurface(surface);

    // 何かが描かれている
    CHECK((red != 0 || green != 0 || blue != 0));
}

#endif
