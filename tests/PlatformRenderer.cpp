#include <Base/Canvas.hpp>
#include <Base/Draw.hpp>
#include <Base/Platform/Image.hpp>
#include <Base/Platform/SdlRenderer.hpp>
#include <Base/Platform/Textures.hpp>
#include <Extract/Image.hpp>
#include <Base/Platform/System.hpp>
#include <Base/Platform/Window.hpp>

#include <doctest/doctest.h>

#include <SDL3/SDL.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace Base = TellerEngine::Base;
namespace Platform = TellerEngine::Base::Platform;

#if !defined(__EMSCRIPTEN__)

namespace {

// 画面が無ければ何も返さない
struct Screen {
    std::optional<Platform::System> system;
    std::optional<Platform::Window> window;
    std::optional<Platform::SdlRenderer> renderer;

    bool Open() {
        auto created = Platform::System::Create(SDL_INIT_VIDEO);
        if (!created.has_value()) {
            return false;
        }
        system.emplace(std::move(*created));

        auto opened = Platform::Window::Create("TellerRenderer", 64, 64);
        if (!opened.has_value()) {
            return false;
        }
        window.emplace(std::move(*opened));

        auto made = Platform::SdlRenderer::Create(*window);
        if (!made.has_value()) {
            return false;
        }
        renderer.emplace(std::move(*made));
        return true;
    }
};

// 単色で塗ったPNGを1枚こしらえる
std::filesystem::path MakePng(const std::string &name, int width, int height,
                              Base::Color color) {
    TellerEngine::Extract::Image image;
    image.width = static_cast<std::uint32_t>(width);
    image.height = static_cast<std::uint32_t>(height);
    image.pixels.resize(static_cast<std::size_t>(width) * height * 4);
    for (std::size_t i = 0; i < image.pixels.size(); i += 4) {
        image.pixels[i + 0] = color.red;
        image.pixels[i + 1] = color.green;
        image.pixels[i + 2] = color.blue;
        image.pixels[i + 3] = color.alpha;
    }
    const auto path = std::filesystem::temp_directory_path() / name;
    const auto written = TellerEngine::Extract::WritePng(image, path);
    REQUIRE(written.has_value());
    return path;
}

// 左半分と右半分で色が違うPNGをこしらえる
std::filesystem::path MakeSplitPng(const std::string &name, int width, int height,
                                   Base::Color left, Base::Color right) {
    TellerEngine::Extract::Image image;
    image.width = static_cast<std::uint32_t>(width);
    image.height = static_cast<std::uint32_t>(height);
    image.pixels.resize(static_cast<std::size_t>(width) * height * 4);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const Base::Color &color = x < width / 2 ? left : right;
            const std::size_t i = (static_cast<std::size_t>(y) * width + x) * 4;
            image.pixels[i + 0] = color.red;
            image.pixels[i + 1] = color.green;
            image.pixels[i + 2] = color.blue;
            image.pixels[i + 3] = color.alpha;
        }
    }
    const auto path = std::filesystem::temp_directory_path() / name;
    const auto written = TellerEngine::Extract::WritePng(image, path);
    REQUIRE(written.has_value());
    return path;
}

Base::Color PixelAt(Platform::SdlRenderer &renderer, int x, int y) {
    SDL_Rect rect{x, y, 1, 1};
    SDL_Surface *surface = SDL_RenderReadPixels(renderer.Handle(), &rect);
    if (surface == nullptr) {
        return Base::Color{0, 0, 0, 0};
    }
    Uint8 red = 0;
    Uint8 green = 0;
    Uint8 blue = 0;
    Uint8 alpha = 0;
    SDL_ReadSurfacePixel(surface, 0, 0, &red, &green, &blue, &alpha);
    SDL_DestroySurface(surface);
    return Base::Color{red, green, blue, alpha};
}

} // namespace

TEST_CASE("塗った矩形が画面に出る") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SetColor(Base::Color{255, 0, 0, 255});
    canvas.Rectangle(10.0, 10.0, 20.0, 20.0);

    screen.renderer->Render(list);

    const Base::Color inside = PixelAt(*screen.renderer, 15, 15);
    CHECK(inside.red == 255);
    CHECK(inside.green == 0);
    CHECK(inside.blue == 0);

    const Base::Color outside = PixelAt(*screen.renderer, 40, 40);
    CHECK(outside.red == 0);
    CHECK(outside.green == 0);
}

TEST_CASE("背景の色を変えられる") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    CHECK(screen.renderer->ClearColor() == Base::Color{0, 0, 0, 255});
    screen.renderer->SetClearColor(Base::Color{0, 0, 255, 255});

    Base::DrawList list;
    screen.renderer->Render(list);

    const Base::Color pixel = PixelAt(*screen.renderer, 32, 32);
    CHECK(pixel.blue == 255);
    CHECK(pixel.red == 0);
}

TEST_CASE("塗らない矩形は縁だけが出る") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SetColor(Base::Color{0, 255, 0, 255});
    canvas.Rectangle(10.0, 10.0, 20.0, 20.0, false);

    screen.renderer->Render(list);

    CHECK(PixelAt(*screen.renderer, 10, 10).green == 255);
    CHECK(PixelAt(*screen.renderer, 20, 20).green == 0);
}

TEST_CASE("線が出る") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SetColor(Base::Color{255, 255, 255, 255});
    canvas.Line(0.0, 32.0, 63.0, 32.0);

    screen.renderer->Render(list);

    CHECK(PixelAt(*screen.renderer, 32, 32).red == 255);
    CHECK(PixelAt(*screen.renderer, 32, 40).red == 0);
}

TEST_CASE("三角形と円が塗られる") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SetColor(Base::Color{255, 255, 0, 255});
    canvas.Triangle(0.0, 0.0, 30.0, 0.0, 0.0, 30.0);
    canvas.SetColor(Base::Color{0, 255, 255, 255});
    canvas.Circle(48.0, 48.0, 10.0);

    screen.renderer->Render(list);

    const Base::Color triangle = PixelAt(*screen.renderer, 5, 5);
    CHECK(triangle.red == 255);
    CHECK(triangle.green == 255);
    CHECK(triangle.blue == 0);

    const Base::Color circle = PixelAt(*screen.renderer, 48, 48);
    CHECK(circle.red == 0);
    CHECK(circle.green == 255);
    CHECK(circle.blue == 255);
}

TEST_CASE("後から積んだものが上に出る") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SetColor(Base::Color{255, 0, 0, 255});
    canvas.Rectangle(0.0, 0.0, 40.0, 40.0);
    canvas.SetColor(Base::Color{0, 0, 255, 255});
    canvas.Rectangle(0.0, 0.0, 40.0, 40.0);

    screen.renderer->Render(list);

    const Base::Color pixel = PixelAt(*screen.renderer, 20, 20);
    CHECK(pixel.blue == 255);
    CHECK(pixel.red == 0);
}

TEST_CASE("透明度を切ると下が透けない") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SetColor(Base::Color{255, 255, 255, 255});
    canvas.Rectangle(0.0, 0.0, 40.0, 40.0);
    canvas.SetBlend(Base::BlendMode::None);
    canvas.SetColor(Base::Color{255, 0, 0, 255});
    canvas.SetAlpha(0.5);
    canvas.Rectangle(0.0, 0.0, 40.0, 40.0);

    screen.renderer->Render(list);

    // 混ぜないので出した色がそのまま残る
    const Base::Color pixel = PixelAt(*screen.renderer, 20, 20);
    CHECK(pixel.red == 255);
    CHECK(pixel.green == 0);
    CHECK(pixel.blue == 0);
}

TEST_CASE("透明度が生きていれば下と混ざる") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SetColor(Base::Color{255, 255, 255, 255});
    canvas.Rectangle(0.0, 0.0, 40.0, 40.0);
    canvas.SetColor(Base::Color{255, 0, 0, 255});
    canvas.SetAlpha(0.5);
    canvas.Rectangle(0.0, 0.0, 40.0, 40.0);

    screen.renderer->Render(list);

    const Base::Color pixel = PixelAt(*screen.renderer, 20, 20);
    CHECK(pixel.red == 255);
    CHECK(pixel.green > 100);
    CHECK(pixel.green < 160);
}

TEST_CASE("加算は明るくなる") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SetColor(Base::Color{0, 0, 128, 255});
    canvas.Rectangle(0.0, 0.0, 40.0, 40.0);
    canvas.SetBlend(Base::BlendMode::Add);
    canvas.SetColor(Base::Color{0, 0, 100, 255});
    canvas.Rectangle(0.0, 0.0, 40.0, 40.0);

    screen.renderer->Render(list);

    const Base::Color pixel = PixelAt(*screen.renderer, 20, 20);
    CHECK(pixel.blue > 200);
}

TEST_CASE("減算は暗くなる") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SetColor(Base::Color{200, 200, 200, 255});
    canvas.Rectangle(0.0, 0.0, 40.0, 40.0);
    canvas.SetBlend(Base::BlendMode::Subtract);
    canvas.SetColor(Base::Color{100, 100, 100, 255});
    canvas.Rectangle(0.0, 0.0, 40.0, 40.0);

    screen.renderer->Render(list);

    const Base::Color pixel = PixelAt(*screen.renderer, 20, 20);
    CHECK(pixel.red < 150);
    CHECK(pixel.red > 50);
}

TEST_CASE("乗算は掛け合わさる") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SetColor(Base::Color{255, 255, 255, 255});
    canvas.Rectangle(0.0, 0.0, 40.0, 40.0);
    canvas.SetBlend(Base::BlendMode::Multiply);
    canvas.SetColor(Base::Color{255, 0, 0, 255});
    canvas.Rectangle(0.0, 0.0, 40.0, 40.0);

    screen.renderer->Render(list);

    const Base::Color pixel = PixelAt(*screen.renderer, 20, 20);
    CHECK(pixel.red == 255);
    CHECK(pixel.green == 0);
}

TEST_CASE("混ぜ方は積み荷の値にも表れる") {
    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SetBlend(Base::BlendMode::None);
    canvas.Rectangle(0.0, 0.0, 1.0, 1.0);
    CHECK(list.Commands()[0].blend == Base::BlendMode::None);

    canvas.SetBlend(Base::BlendMode::Normal);
    canvas.Rectangle(0.0, 0.0, 1.0, 1.0);
    CHECK(list.Commands()[1].blend == Base::BlendMode::Normal);
}

TEST_CASE("スプライトが出る") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    const auto path = MakePng("teller-sprite.png", 8, 8, Base::Color{0, 255, 0, 255});
    Platform::Textures textures{screen.renderer->Handle()};
    const std::vector<std::filesystem::path> frames{path};
    const auto image = textures.Add(TellerEngine::Span<const std::filesystem::path>{frames},
                                    0.0, 0.0);
    REQUIRE(image.has_value());
    screen.renderer->SetTextures(&textures);

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.Sprite(*image, 0, 10.0, 10.0);

    screen.renderer->Render(list);

    CHECK(PixelAt(*screen.renderer, 12, 12).green == 255);
    CHECK(PixelAt(*screen.renderer, 30, 30).green == 0);
}

TEST_CASE("原点の分だけずれて出る") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    const auto path = MakePng("teller-origin.png", 8, 8, Base::Color{255, 0, 255, 255});
    Platform::Textures textures{screen.renderer->Handle()};
    const std::vector<std::filesystem::path> frames{path};
    const auto image = textures.Add(TellerEngine::Span<const std::filesystem::path>{frames},
                                    4.0, 4.0);
    REQUIRE(image.has_value());
    screen.renderer->SetTextures(&textures);

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.Sprite(*image, 0, 20.0, 20.0);

    screen.renderer->Render(list);

    // 原点が中心なので20,20の周りに出る
    CHECK(PixelAt(*screen.renderer, 20, 20).red == 255);
    CHECK(PixelAt(*screen.renderer, 17, 17).red == 255);
    CHECK(PixelAt(*screen.renderer, 25, 25).red == 0);
}

TEST_CASE("拡大すると大きく出る") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    const auto path = MakePng("teller-scale.png", 4, 4, Base::Color{255, 255, 0, 255});
    Platform::Textures textures{screen.renderer->Handle()};
    const std::vector<std::filesystem::path> frames{path};
    const auto image = textures.Add(TellerEngine::Span<const std::filesystem::path>{frames},
                                    0.0, 0.0);
    REQUIRE(image.has_value());
    screen.renderer->SetTextures(&textures);

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SpriteExt(*image, 0, 0.0, 0.0, 4.0, 4.0, 0.0);

    screen.renderer->Render(list);

    CHECK(PixelAt(*screen.renderer, 14, 14).red == 255);
    CHECK(PixelAt(*screen.renderer, 20, 20).red == 0);
}

TEST_CASE("色と透明度がスプライトにも効く") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    const auto path = MakePng("teller-tint.png", 8, 8, Base::Color{255, 255, 255, 255});
    Platform::Textures textures{screen.renderer->Handle()};
    const std::vector<std::filesystem::path> frames{path};
    const auto image = textures.Add(TellerEngine::Span<const std::filesystem::path>{frames},
                                    0.0, 0.0);
    REQUIRE(image.has_value());
    screen.renderer->SetTextures(&textures);

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SetColor(Base::Color{255, 0, 0, 255});
    canvas.Sprite(*image, 0, 10.0, 10.0);

    screen.renderer->Render(list);

    const Base::Color pixel = PixelAt(*screen.renderer, 12, 12);
    CHECK(pixel.red == 255);
    CHECK(pixel.green == 0);
}

TEST_CASE("コマを並べて切り替えられる") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    const std::vector<std::filesystem::path> frames{
        MakePng("teller-frame0.png", 8, 8, Base::Color{255, 0, 0, 255}),
        MakePng("teller-frame1.png", 8, 8, Base::Color{0, 0, 255, 255}),
    };
    Platform::Textures textures{screen.renderer->Handle()};
    const auto image = textures.Add(TellerEngine::Span<const std::filesystem::path>{frames},
                                    0.0, 0.0);
    REQUIRE(image.has_value());
    screen.renderer->SetTextures(&textures);

    {
        Base::DrawList list;
        Base::Canvas canvas{list};
        canvas.Sprite(*image, 0, 0.0, 0.0);
        screen.renderer->Render(list);
        CHECK(PixelAt(*screen.renderer, 4, 4).red == 255);
    }
    {
        Base::DrawList list;
        Base::Canvas canvas{list};
        canvas.Sprite(*image, 1, 0.0, 0.0);
        screen.renderer->Render(list);
        CHECK(PixelAt(*screen.renderer, 4, 4).blue == 255);
    }
    {
        // 並びの外は最初へ戻る
        Base::DrawList list;
        Base::Canvas canvas{list};
        canvas.Sprite(*image, 2, 0.0, 0.0);
        screen.renderer->Render(list);
        CHECK(PixelAt(*screen.renderer, 4, 4).red == 255);
    }
}

TEST_CASE("無い画像を指しても落ちない") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    Platform::Textures textures{screen.renderer->Handle()};
    screen.renderer->SetTextures(&textures);

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.Sprite(Base::ImageId{99}, 0, 10.0, 10.0);
    screen.renderer->Render(list);

    CHECK(textures.Count() == 0);
    CHECK(textures.Find(Base::ImageId{99}) == nullptr);
}

TEST_CASE("読めない画像は登録に失敗する") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    Platform::Textures textures{screen.renderer->Handle()};
    const std::vector<std::filesystem::path> frames{"/存在しない/画像.png"};
    const auto image = textures.Add(TellerEngine::Span<const std::filesystem::path>{frames},
                                    0.0, 0.0);
    CHECK_FALSE(image.has_value());
    CHECK(textures.Count() == 0);
}

TEST_CASE("太い線は幅を持って出る") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SetColor(Base::Color{255, 255, 255, 255});
    canvas.Line(0.0, 32.0, 63.0, 32.0, 9.0);

    screen.renderer->Render(list);

    CHECK(PixelAt(*screen.renderer, 32, 32).red == 255);
    CHECK(PixelAt(*screen.renderer, 32, 35).red == 255);
    CHECK(PixelAt(*screen.renderer, 32, 45).red == 0);
}

TEST_CASE("楕円は囲む矩形で決まる") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SetColor(Base::Color{0, 255, 0, 255});
    canvas.Ellipse(10.0, 28.0, 54.0, 36.0);

    screen.renderer->Render(list);

    CHECK(PixelAt(*screen.renderer, 32, 32).green == 255);
    CHECK(PixelAt(*screen.renderer, 14, 32).green == 255);
    // 縦は薄いので外れる
    CHECK(PixelAt(*screen.renderer, 32, 20).green == 0);
}

TEST_CASE("角の丸い矩形が出る") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SetColor(Base::Color{0, 0, 255, 255});
    canvas.RoundRectangle(10.0, 10.0, 50.0, 50.0);

    screen.renderer->Render(list);

    CHECK(PixelAt(*screen.renderer, 30, 30).blue == 255);
    // 角は落ちている
    CHECK(PixelAt(*screen.renderer, 10, 10).blue == 0);
}

TEST_CASE("矩形の角ごとに色を変えられる") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.RectangleColor(0.0, 0.0, 64.0, 64.0, Base::Color{255, 0, 0, 255},
                          Base::Color{255, 0, 0, 255}, Base::Color{0, 0, 255, 255},
                          Base::Color{0, 0, 255, 255});

    screen.renderer->Render(list);

    const Base::Color upper = PixelAt(*screen.renderer, 32, 4);
    const Base::Color lower = PixelAt(*screen.renderer, 32, 60);
    CHECK(upper.red > 200);
    CHECK(lower.blue > 200);
}

TEST_CASE("円は中心から縁へ色が変わる") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.CircleColor(32.0, 32.0, 25.0, Base::Color{255, 255, 255, 255},
                       Base::Color{255, 0, 0, 255});

    screen.renderer->Render(list);

    CHECK(PixelAt(*screen.renderer, 32, 32).green > 200);
    CHECK(PixelAt(*screen.renderer, 32, 12).green < 100);
}

TEST_CASE("分割数を変えると形が変わる") {
    Base::DrawList coarse;
    Base::DrawList fine;
    {
        Base::Canvas canvas{coarse};
        canvas.SetCirclePrecision(4);
        canvas.Circle(0.0, 0.0, 10.0);
    }
    {
        Base::Canvas canvas{fine};
        canvas.Circle(0.0, 0.0, 10.0);
    }

    CHECK(coarse.Commands()[0].segments == 4);
    CHECK(fine.Commands()[0].segments == Base::kDefaultCirclePrecision);
    CHECK(coarse.Hash() != fine.Hash());
}

TEST_CASE("一部だけを切り出して出せる") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    const auto path = MakeSplitPng("teller-part.png", 16, 16, Base::Color{255, 0, 0, 255},
                                   Base::Color{0, 0, 255, 255});
    Platform::Textures textures{screen.renderer->Handle()};
    const std::vector<std::filesystem::path> frames{path};
    const auto image = textures.Add(TellerEngine::Span<const std::filesystem::path>{frames},
                                    0.0, 0.0);
    REQUIRE(image.has_value());
    screen.renderer->SetTextures(&textures);

    Base::DrawList list;
    Base::Canvas canvas{list};
    // 右半分だけを取り出す
    canvas.SpritePart(*image, 0, 8.0, 0.0, 8.0, 16.0, 10.0, 10.0);

    screen.renderer->Render(list);

    CHECK(PixelAt(*screen.renderer, 12, 12).blue == 255);
    CHECK(PixelAt(*screen.renderer, 12, 12).red == 0);
    // 8画素分しか出ない
    CHECK(PixelAt(*screen.renderer, 20, 12).blue == 0);
}

TEST_CASE("部分描画は原点を見ない") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    const auto path = MakePng("teller-partorigin.png", 16, 16, Base::Color{0, 255, 0, 255});
    Platform::Textures textures{screen.renderer->Handle()};
    const std::vector<std::filesystem::path> frames{path};
    const auto image = textures.Add(TellerEngine::Span<const std::filesystem::path>{frames},
                                    8.0, 8.0);
    REQUIRE(image.has_value());
    screen.renderer->SetTextures(&textures);

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SpritePart(*image, 0, 0.0, 0.0, 16.0, 16.0, 20.0, 20.0);

    screen.renderer->Render(list);

    // 原点を引いていれば12,12に出るが、見ないので20,20から出る
    CHECK(PixelAt(*screen.renderer, 22, 22).green == 255);
    CHECK(PixelAt(*screen.renderer, 14, 14).green == 0);
}

TEST_CASE("切り出しながら拡大できる") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    const auto path = MakeSplitPng("teller-partext.png", 16, 16, Base::Color{255, 0, 0, 255},
                                   Base::Color{0, 0, 255, 255});
    Platform::Textures textures{screen.renderer->Handle()};
    const std::vector<std::filesystem::path> frames{path};
    const auto image = textures.Add(TellerEngine::Span<const std::filesystem::path>{frames},
                                    0.0, 0.0);
    REQUIRE(image.has_value());
    screen.renderer->SetTextures(&textures);

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SpritePartExt(*image, 0, 0.0, 0.0, 8.0, 8.0, 0.0, 0.0, 4.0, 4.0);

    screen.renderer->Render(list);

    CHECK(PixelAt(*screen.renderer, 30, 30).red == 255);
    CHECK(PixelAt(*screen.renderer, 40, 30).red == 0);
}

TEST_CASE("指した大きさに伸ばせる") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    const auto path = MakePng("teller-stretch.png", 4, 4, Base::Color{255, 255, 0, 255});
    Platform::Textures textures{screen.renderer->Handle()};
    const std::vector<std::filesystem::path> frames{path};
    const auto image = textures.Add(TellerEngine::Span<const std::filesystem::path>{frames},
                                    2.0, 2.0);
    REQUIRE(image.has_value());
    screen.renderer->SetTextures(&textures);

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SpriteStretched(*image, 0, 0.0, 0.0, 50.0, 20.0);

    screen.renderer->Render(list);

    CHECK(PixelAt(*screen.renderer, 45, 15).red == 255);
    CHECK(PixelAt(*screen.renderer, 45, 25).red == 0);
    CHECK(PixelAt(*screen.renderer, 55, 15).red == 0);
}

TEST_CASE("切り出す範囲が絵からはみ出しても落ちない") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    const auto path = MakePng("teller-clamp.png", 8, 8, Base::Color{255, 255, 255, 255});
    Platform::Textures textures{screen.renderer->Handle()};
    const std::vector<std::filesystem::path> frames{path};
    const auto image = textures.Add(TellerEngine::Span<const std::filesystem::path>{frames},
                                    0.0, 0.0);
    REQUIRE(image.has_value());
    screen.renderer->SetTextures(&textures);

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SpritePart(*image, 0, 4.0, 4.0, 100.0, 100.0, 0.0, 0.0);
    canvas.SpritePart(*image, 0, 50.0, 50.0, 10.0, 10.0, 20.0, 20.0);

    screen.renderer->Render(list);

    // はみ出た分は切り詰められる
    CHECK(PixelAt(*screen.renderer, 2, 2).red == 255);
    CHECK(PixelAt(*screen.renderer, 6, 6).red == 0);
    // 範囲の外を指した方は何も出ない
    CHECK(PixelAt(*screen.renderer, 22, 22).red == 0);
}

TEST_CASE("描き込める絵を作って使える") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    Platform::Textures textures{screen.renderer->Handle()};
    const auto surface = textures.AddTarget(16, 16);
    REQUIRE(surface.has_value());
    screen.renderer->SetTextures(&textures);

    const auto *entry = textures.Find(*surface);
    REQUIRE(entry != nullptr);
    CHECK(entry->width == 16);
    CHECK(entry->height == 16);

    Base::DrawList list;
    Base::Canvas canvas{list};
    // 絵の中を緑で埋める
    canvas.SetTarget(*surface);
    canvas.SetColor(Base::Color{0, 255, 0, 255});
    canvas.Rectangle(0.0, 0.0, 16.0, 16.0);
    // 画面へ戻して、その絵を貼る
    canvas.ResetTarget();
    canvas.SetColor(Base::Color{255, 255, 255, 255});
    canvas.Sprite(*surface, 0, 20.0, 20.0);

    screen.renderer->Render(list);

    CHECK(PixelAt(*screen.renderer, 25, 25).green == 255);
    CHECK(PixelAt(*screen.renderer, 50, 50).green == 0);
}

TEST_CASE("描画先を戻さなくても最後には画面に戻る") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    Platform::Textures textures{screen.renderer->Handle()};
    const auto surface = textures.AddTarget(16, 16);
    REQUIRE(surface.has_value());
    screen.renderer->SetTextures(&textures);

    {
        Base::DrawList list;
        Base::Canvas canvas{list};
        canvas.SetTarget(*surface);
        canvas.SetColor(Base::Color{0, 0, 255, 255});
        canvas.Rectangle(0.0, 0.0, 16.0, 16.0);
        screen.renderer->Render(list);
    }
    {
        Base::DrawList list;
        Base::Canvas canvas{list};
        canvas.SetColor(Base::Color{255, 0, 0, 255});
        canvas.Rectangle(0.0, 0.0, 40.0, 40.0);
        screen.renderer->Render(list);
    }

    CHECK(PixelAt(*screen.renderer, 20, 20).red == 255);
}

TEST_CASE("捨てた絵は存在しなくなる") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    Platform::Textures textures{screen.renderer->Handle()};
    const auto surface = textures.AddTarget(8, 8);
    REQUIRE(surface.has_value());
    CHECK(textures.Find(*surface) != nullptr);

    textures.Free(*surface);
    CHECK(textures.Find(*surface) == nullptr);
    CHECK(textures.FrameOf(*surface, 0) == nullptr);

    // 捨てたあとに指しても落ちない
    screen.renderer->SetTextures(&textures);
    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SetTarget(*surface);
    canvas.Rectangle(0.0, 0.0, 8.0, 8.0);
    canvas.ResetTarget();
    screen.renderer->Render(list);
}

TEST_CASE("描き込んだ絵の一部だけを貼れる") {
    Screen screen;
    if (!screen.Open()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    Platform::Textures textures{screen.renderer->Handle()};
    const auto surface = textures.AddTarget(16, 16);
    REQUIRE(surface.has_value());
    screen.renderer->SetTextures(&textures);

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SetTarget(*surface);
    canvas.SetColor(Base::Color{255, 0, 0, 255});
    canvas.Rectangle(0.0, 0.0, 8.0, 16.0);
    canvas.SetColor(Base::Color{0, 0, 255, 255});
    canvas.Rectangle(8.0, 0.0, 8.0, 16.0);
    canvas.ResetTarget();
    canvas.SetColor(Base::Color{255, 255, 255, 255});
    canvas.SpritePart(*surface, 0, 8.0, 0.0, 8.0, 16.0, 20.0, 20.0);

    screen.renderer->Render(list);

    CHECK(PixelAt(*screen.renderer, 22, 25).blue == 255);
    CHECK(PixelAt(*screen.renderer, 22, 25).red == 0);
}

#endif
