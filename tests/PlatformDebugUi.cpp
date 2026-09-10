#include <Base/Canvas.hpp>
#include <Base/Draw.hpp>
#include <Base/Input.hpp>
#include <Base/Platform/DebugUi.hpp>
#include <Base/Platform/Devices.hpp>
#include <Base/Platform/SdlRenderer.hpp>
#include <Base/Platform/System.hpp>
#include <Base/Platform/Window.hpp>

#include <doctest/doctest.h>

#include <SDL3/SDL.h>

#ifdef TELLER_DEBUG_UI
#include <imgui.h>
#endif

namespace Base = TellerEngine::Base;
namespace Platform = TellerEngine::Base::Platform;

// ブラウザの外ではSDLが画面を作れない
#if !defined(__EMSCRIPTEN__)

namespace {

// 窓と描画先を1組そろえる
struct Screen {
    TellerEngine::Expected<Platform::System, Base::Error> system =
        Platform::System::Create(SDL_INIT_VIDEO);
    TellerEngine::Expected<Platform::Window, Base::Error> window =
        TellerEngine::Unexpected<Base::Error>(Base::Error{});
    TellerEngine::Expected<Platform::SdlRenderer, Base::Error> renderer =
        TellerEngine::Unexpected<Base::Error>(Base::Error{});

    bool Ready() {
        if (!system.has_value()) {
            return false;
        }
        window = Platform::Window::Create("TellerDebugUi", 320, 240);
        if (!window.has_value()) {
            return false;
        }
        renderer = Platform::SdlRenderer::Create(*window);
        return renderer.has_value();
    }
};

} // namespace

TEST_CASE("窓が無ければ作れない") {
    if constexpr (!Platform::DebugUi::Enabled()) {
        MESSAGE("デバッグUIが入っていないので飛ばす");
        return;
    }
    CHECK_FALSE(Platform::DebugUi::Create(nullptr, nullptr).has_value());
}

TEST_CASE("窓と描画先があれば開ける") {
    Screen screen;
    if (!screen.Ready()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    auto ui = Platform::DebugUi::Create(screen.window->Handle(), screen.renderer->Handle());
    REQUIRE(ui.has_value());
    CHECK(ui->Docking() == Platform::DebugUi::Enabled());
    CHECK(ui->DrawCount() == 0);
}

TEST_CASE("枠の入れ子は切り替えられる") {
    Screen screen;
    if (!screen.Ready()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    auto ui = Platform::DebugUi::Create(screen.window->Handle(), screen.renderer->Handle());
    REQUIRE(ui.has_value());

    ui->SetDocking(false);
    CHECK_FALSE(ui->Docking());

    ui->SetDocking(true);
    CHECK(ui->Docking() == Platform::DebugUi::Enabled());
}

TEST_CASE("窓を出すと三角形が積まれる") {
    if constexpr (!Platform::DebugUi::Enabled()) {
        MESSAGE("デバッグUIが入っていないので飛ばす");
        return;
    }

    Screen screen;
    if (!screen.Ready()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    auto ui = Platform::DebugUi::Create(screen.window->Handle(), screen.renderer->Handle());
    REQUIRE(ui.has_value());

#ifdef TELLER_DEBUG_UI
    // 出したばかりの窓は大きさが決まるまで描かれない
    for (int i = 0; i < 3; ++i) {
        ui->NewFrame();
        ImGui::Begin("たしかめ");
        ImGui::Text("ほげ");
        ImGui::End();
        ui->Render(screen.renderer->Handle());
    }
    CHECK(ui->DrawCount() > 0);

    // 何も出さなければ積まれない
    ui->NewFrame();
    ui->Render(screen.renderer->Handle());
    CHECK(ui->DrawCount() == 0);
#endif
}

TEST_CASE("ネイティブでは別ウインドウを出せる") {
    if constexpr (!Platform::DebugUi::Enabled()) {
        MESSAGE("デバッグUIが入っていないので飛ばす");
        return;
    }

    Screen screen;
    if (!screen.Ready()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    auto ui = Platform::DebugUi::Create(screen.window->Handle(), screen.renderer->Handle());
    REQUIRE(ui.has_value());

#if defined(TELLER_DEBUG_UI) && !defined(__EMSCRIPTEN__)
    REQUIRE(ui->Viewports());
    CHECK(ui->WindowCount() == 1);

    ImGui::GetIO().ConfigViewportsNoAutoMerge = true;
    const ImVec2 base = ImGui::GetMainViewport()->Pos;

    for (int i = 0; i < 4; ++i) {
        ui->NewFrame();
        ImGui::SetNextWindowPos(ImVec2(base.x + 400.0f, base.y + 40.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(200.0f, 120.0f), ImGuiCond_Always);
        ImGui::Begin("そとまど");
        ImGui::Text("ほげ");
        ImGui::End();
        ui->Render(screen.renderer->Handle());
    }

    REQUIRE(ui->WindowCount() >= 2);

    // 切り離した窓は自分のレンダラを持つ
    ImGuiViewport *outside = ImGui::GetPlatformIO().Viewports[1];
    CHECK(outside->PlatformHandle != nullptr);
    CHECK(outside->RendererUserData != nullptr);
    CHECK(outside->RendererUserData != screen.renderer->Handle());
    CHECK(outside->DrawData != nullptr);
    CHECK(outside->DrawData->TotalIdxCount > 0);

    // 本体のレンダラで作った絵は副ウインドウでは出せない
    CHECK(SDL_GetRendererFromTexture(reinterpret_cast<SDL_Texture *>(
              ImGui::GetPlatformIO().Textures[0]->GetTexID())) == screen.renderer->Handle());

    // 切ると本体の窓へ畳み戻る
    ui->SetViewports(false);
    CHECK_FALSE(ui->Viewports());
    for (int i = 0; i < 3; ++i) {
        ui->NewFrame();
        ImGui::Begin("そとまど");
        ImGui::Text("ほげ");
        ImGui::End();
        ui->Render(screen.renderer->Handle());
    }
    CHECK(ui->WindowCount() == 1);
    CHECK(ui->DrawCount() > 0);

    ImGui::GetIO().ConfigViewportsNoAutoMerge = false;
#endif
}

TEST_CASE("イベントを渡せる") {
    Screen screen;
    if (!screen.Ready()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    auto ui = Platform::DebugUi::Create(screen.window->Handle(), screen.renderer->Handle());
    REQUIRE(ui.has_value());

    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.windowID = SDL_GetWindowID(screen.window->Handle());
    event.motion.x = 10.0f;
    event.motion.y = 20.0f;
    CHECK(ui->Handle(event) == Platform::DebugUi::Enabled());

    CHECK_FALSE(ui->WantsMouse());
    CHECK_FALSE(ui->WantsKeyboard());
}

TEST_CASE("呼び出し口は足した順に番号が振られる") {
    Screen screen;
    if (!screen.Ready()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    auto ui = Platform::DebugUi::Create(screen.window->Handle(), screen.renderer->Handle());
    REQUIRE(ui.has_value());
    if constexpr (!Platform::DebugUi::Enabled()) {
        MESSAGE("デバッグUIが入っていないので飛ばす");
        return;
    }

    CHECK(ui->ToolCount() == 0);
    CHECK(ui->AddTool("hierarchy") == 0);
    CHECK(ui->AddTool("inspector") == 1);
    CHECK(ui->ToolCount() == 2);

    CHECK_FALSE(ui->ToolOpen(0));
    ui->SetToolOpen(0, true);
    CHECK(ui->ToolOpen(0));
    CHECK_FALSE(ui->ToolOpen(1));

    // 無い番号を触っても崩れない
    ui->SetToolOpen(9, true);
    ui->SetIcon(9, nullptr);
    CHECK_FALSE(ui->ToolOpen(9));
    CHECK(ui->ToolCount() == 2);
}

TEST_CASE("呼び出し口は端に並ぶ") {
    Screen screen;
    if (!screen.Ready()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    auto ui = Platform::DebugUi::Create(screen.window->Handle(), screen.renderer->Handle());
    REQUIRE(ui.has_value());
    if constexpr (!Platform::DebugUi::Enabled()) {
        MESSAGE("デバッグUIが入っていないので飛ばす");
        return;
    }

    ui->AddTool("one");
    ui->AddTool("two");
    ui->AddTool("three");
    ui->SetIconSize(24.0);
    CHECK(ui->IconSize() == doctest::Approx(24.0));
    CHECK(ui->Edge() == Platform::DebugEdge::Left);

#ifdef TELLER_DEBUG_UI
    const auto run = [&]() {
        for (int i = 0; i < 3; ++i) {
            ui->NewFrame();
            ui->DrawTools();
            ui->Render(screen.renderer->Handle());
        }
        return ui->ToolBounds();
    };

    const Platform::DebugBounds left = run();
    CHECK(ui->DrawCount() > 0);

    // 本体の窓の位置は動かしてから確かめる
    const ImVec2 origin = ImGui::GetMainViewport()->WorkPos;
    const ImVec2 area = ImGui::GetMainViewport()->WorkSize;
    CHECK(left.x == doctest::Approx(origin.x));
    CHECK(left.y == doctest::Approx(origin.y));

    // 縦に並ぶので細長い
    CHECK(left.height > left.width);

    ui->SetEdge(Platform::DebugEdge::Right);
    const Platform::DebugBounds right = run();
    CHECK(right.x == doctest::Approx(origin.x + area.x - right.width));
    CHECK(right.width == doctest::Approx(left.width));

    ui->SetEdge(Platform::DebugEdge::Top);
    const Platform::DebugBounds top = run();
    CHECK(top.x == doctest::Approx(origin.x));
    CHECK(top.y == doctest::Approx(origin.y));

    // 横に並ぶので平たい
    CHECK(top.width > top.height);

    ui->SetEdge(Platform::DebugEdge::Bottom);
    const Platform::DebugBounds bottom = run();
    CHECK(bottom.y == doctest::Approx(origin.y + area.y - bottom.height));
    CHECK(bottom.height == doctest::Approx(top.height));

    // 一辺を変えれば太さも変わる
    ui->SetIconSize(48.0);
    ui->SetEdge(Platform::DebugEdge::Left);
    const Platform::DebugBounds bigger = run();
    CHECK(bigger.width > left.width);
#endif
}

TEST_CASE("留め置く場所は呼び出し口を避ける") {
    Screen screen;
    if (!screen.Ready()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    auto ui = Platform::DebugUi::Create(screen.window->Handle(), screen.renderer->Handle());
    REQUIRE(ui.has_value());
    if constexpr (!Platform::DebugUi::Enabled()) {
        MESSAGE("デバッグUIが入っていないので飛ばす");
        return;
    }

    // ブラウザ以外では既定で切れている
    CHECK_FALSE(ui->Dock());
    ui->SetDock(true);
    CHECK(ui->Dock());

#ifdef TELLER_DEBUG_UI
    ui->SetViewports(false);
    ui->SetIconSize(24.0);

    const auto run = [&]() {
        for (int i = 0; i < 3; ++i) {
            ui->NewFrame();
            ui->DrawDock();
            ui->DrawTools();
            ui->Render(screen.renderer->Handle());
        }
        return ui->DockBounds();
    };

    // 呼び出し口が無ければ作業領域いっぱい
    const Platform::DebugBounds full = run();
    const ImVec2 origin = ImGui::GetMainViewport()->WorkPos;
    const ImVec2 area = ImGui::GetMainViewport()->WorkSize;
    CHECK(full.x == doctest::Approx(origin.x));
    CHECK(full.width == doctest::Approx(area.x));
    CHECK(full.height == doctest::Approx(area.y));

    ui->AddTool("one");
    ui->AddTool("two");

    const Platform::DebugBounds left = run();
    const Platform::DebugBounds strip = ui->ToolBounds();
    CHECK(strip.width > 0.0);
    CHECK(left.x == doctest::Approx(origin.x + strip.width));
    CHECK(left.width == doctest::Approx(area.x - strip.width));
    CHECK(left.height == doctest::Approx(area.y));

    ui->SetEdge(Platform::DebugEdge::Top);
    const Platform::DebugBounds top = run();
    CHECK(top.y == doctest::Approx(origin.y + ui->ToolBounds().height));
    CHECK(top.height == doctest::Approx(area.y - ui->ToolBounds().height));
    CHECK(top.width == doctest::Approx(area.x));

    ui->SetEdge(Platform::DebugEdge::Right);
    const Platform::DebugBounds right = run();
    CHECK(right.x == doctest::Approx(origin.x));
    CHECK(right.width == doctest::Approx(area.x - ui->ToolBounds().width));

    ui->SetEdge(Platform::DebugEdge::Bottom);
    const Platform::DebugBounds bottom = run();
    CHECK(bottom.y == doctest::Approx(origin.y));
    CHECK(bottom.height == doctest::Approx(area.y - ui->ToolBounds().height));

    // 切れば何も敷かれない
    ui->SetDock(false);
    ui->NewFrame();
    ui->DrawDock();
    ui->Render(screen.renderer->Handle());
    CHECK(ui->DrawCount() == 0);
#endif
}

TEST_CASE("留め置く場所は下のゲームを隠さない") {
    if constexpr (!Platform::DebugUi::Enabled()) {
        MESSAGE("デバッグUIが入っていないので飛ばす");
        return;
    }

    Screen screen;
    if (!screen.Ready()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    auto ui = Platform::DebugUi::Create(screen.window->Handle(), screen.renderer->Handle());
    REQUIRE(ui.has_value());

#ifdef TELLER_DEBUG_UI
    ui->SetViewports(false);
    ui->SetDock(true);

    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SetColor(Base::Color{255, 0, 0, 255});
    canvas.Rectangle(0.0, 0.0, 320.0, 240.0);

    for (int i = 0; i < 3; ++i) {
        screen.renderer->Render(list);
        ui->NewFrame();
        ui->DrawDock();
        ui->Render(screen.renderer->Handle());
    }

    SDL_Rect rect{160, 120, 1, 1};
    SDL_Surface *surface = SDL_RenderReadPixels(screen.renderer->Handle(), &rect);
    REQUIRE(surface != nullptr);
    Uint8 red = 0;
    Uint8 green = 0;
    Uint8 blue = 0;
    Uint8 alpha = 0;
    SDL_ReadSurfacePixel(surface, 0, 0, &red, &green, &blue, &alpha);
    SDL_DestroySurface(surface);

    // 真ん中は素通しなので、下の赤がそのまま見える
    CHECK(red == 255);
    CHECK(green == 0);
    CHECK(blue == 0);
#endif
}

TEST_CASE("呼び出し口が無ければ何も積まれない") {
    if constexpr (!Platform::DebugUi::Enabled()) {
        MESSAGE("デバッグUIが入っていないので飛ばす");
        return;
    }

    Screen screen;
    if (!screen.Ready()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    auto ui = Platform::DebugUi::Create(screen.window->Handle(), screen.renderer->Handle());
    REQUIRE(ui.has_value());

    ui->NewFrame();
    ui->DrawTools();
    ui->Render(screen.renderer->Handle());
    CHECK(ui->DrawCount() == 0);
    CHECK(ui->ToolBounds().width == doctest::Approx(0.0));
}

TEST_CASE("土台は移せる") {
    Screen screen;
    if (!screen.Ready()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    auto ui = Platform::DebugUi::Create(screen.window->Handle(), screen.renderer->Handle());
    REQUIRE(ui.has_value());

    Platform::DebugUi moved = std::move(*ui);
    CHECK(moved.Docking() == Platform::DebugUi::Enabled());
}

TEST_CASE("入力の口はイベントを覗ける") {
    auto system = Platform::System::Create(SDL_INIT_VIDEO);
    if (!system.has_value()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }

    static int seen = 0;
    static Uint32 kind = 0;
    seen = 0;
    kind = 0;

    Platform::Devices devices;
    devices.Observe(
        [](void *user, const SDL_Event &event) {
            *static_cast<int *>(user) += 1;
            kind = event.type;
        },
        &seen);

    SDL_Event event{};
    event.type = SDL_EVENT_USER;
    REQUIRE(SDL_PushEvent(&event));

    Base::Input input;
    devices.Pump(input);
    CHECK(seen >= 1);
    CHECK(kind == SDL_EVENT_USER);
}

#endif

TEST_CASE("組み込みの有無は積み方で決まる") {
#ifdef TELLER_DEBUG_UI
    CHECK(Platform::DebugUi::Enabled());
#else
    CHECK_FALSE(Platform::DebugUi::Enabled());
#endif
}
