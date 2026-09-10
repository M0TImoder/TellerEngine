#pragma once

#include <Base/Compat.hpp>
#include <Base/Error.hpp>

#include <SDL3/SDL.h>

#include <cstddef>
#include <memory>
#include <string>

namespace TellerEngine::Base::Platform {

// 呼び出し口を並べる辺
enum class DebugEdge {
    Left,
    Right,
    Top,
    Bottom,
};

// 呼び出し口の並びが占める場所
struct DebugBounds {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

// デバッグUIの土台
class DebugUi {
public:
    static Expected<DebugUi, Error> Create(SDL_Window *window, SDL_Renderer *renderer);

    DebugUi(const DebugUi &) = delete;
    DebugUi &operator=(const DebugUi &) = delete;
    DebugUi(DebugUi &&other) noexcept;
    DebugUi &operator=(DebugUi &&other) noexcept;
    ~DebugUi();

    // 組み込まれているか
    static constexpr bool Enabled() {
#ifdef TELLER_DEBUG_UI
        return true;
#else
        return false;
#endif
    }

    // UIが受け取ったならtrue
    bool Handle(const SDL_Event &event);

    void NewFrame();

    // 積み荷を出した後、表に出す前に呼ぶ
    void Render(SDL_Renderer *renderer);

    // UIが入力を握っているか
    bool WantsMouse() const;
    bool WantsKeyboard() const;

    void SetDocking(bool enabled);
    bool Docking() const;

    // 別ウインドウとして切り離せるようにする
    // ブラウザでは開けない
    void SetViewports(bool enabled);
    bool Viewports() const;

    // 本体を含めた今のウインドウの数
    std::size_t WindowCount() const;

    // 呼び出し口を1つ足し、番号を返す
    std::size_t AddTool(std::string name);

    // アイコンは縦横が同じ絵を渡す
    void SetIcon(std::size_t tool, SDL_Texture *icon);

    bool ToolOpen(std::size_t tool) const;
    void SetToolOpen(std::size_t tool, bool open);
    std::size_t ToolCount() const;

    void SetEdge(DebugEdge edge);
    DebugEdge Edge() const;

    // アイコンの一辺
    void SetIconSize(double size);
    double IconSize() const;

    // 呼び出し口を並べる
    // NewFrameの後に呼ぶ
    void DrawTools();

    DebugBounds ToolBounds() const;

    // 窓を本体の中に留め置けるようにする
    // 別ウインドウを出せないブラウザでは最初から入
    void SetDock(bool enabled);
    bool Dock() const;

    // 留め置く場所を敷く
    // NewFrameの後、窓を出す前に呼ぶ
    void DrawDock();

    DebugBounds DockBounds() const;

    // 直前のフレームで出た三角形の数
    std::size_t DrawCount() const;

private:
    struct State;

    DebugUi() = default;

    std::unique_ptr<State> state_;
};

} // namespace TellerEngine::Base::Platform
