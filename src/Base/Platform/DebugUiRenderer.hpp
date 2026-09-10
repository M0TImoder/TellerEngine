#pragma once

#ifdef TELLER_DEBUG_UI

#include <SDL3/SDL.h>

struct ImDrawData;

// SDL_Rendererで描くImGuiの口
// 副ウインドウごとにレンダラを作り、絵を複製して張り替える
namespace TellerEngine::Base::Platform::DebugUiRender {

bool Init(SDL_Renderer *primary);

void Shutdown();

// 作り直しが要る絵を整える
void NewFrame();

void Draw(ImDrawData *data, SDL_Renderer *renderer);

} // namespace TellerEngine::Base::Platform::DebugUiRender

#endif
