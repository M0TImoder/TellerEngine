#pragma once

#include <Base/Draw.hpp>

namespace TellerEngine::Base {

// 積み荷を画面に出す
class Renderer {
public:
    virtual ~Renderer() = default;

    // 積み荷を裏面へ描く
    virtual void Render(const DrawList &list) = 0;

    // 裏面を表に出す
    virtual void Flip() = 0;

    void Present(const DrawList &list) {
        Render(list);
        Flip();
    }

    virtual void SetClearColor(Color color) = 0;
    virtual Color ClearColor() const = 0;
};

} // namespace TellerEngine::Base
