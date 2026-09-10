#include <Base/Platform/DebugUiRenderer.hpp>

#ifdef TELLER_DEBUG_UI

#include <imgui.h>

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace TellerEngine::Base::Platform::DebugUiRender {

namespace {

// レンダラ1つ分の絵の控え
struct Copies {
    std::unordered_map<ImTextureData *, SDL_Texture *> textures;
    std::unordered_map<ImTextureData *, std::uint64_t> revisions;
};

struct Backend {
    SDL_Renderer *primary = nullptr;
    std::vector<SDL_FColor> colors;

    // 中身が変わるたびに増える
    std::unordered_map<ImTextureData *, std::uint64_t> revisions;

    std::unordered_map<SDL_Renderer *, Copies> copies;
};

Backend *backend = nullptr;

SDL_Texture *MakeTexture(SDL_Renderer *renderer, const ImTextureData &source) {
    SDL_Texture *texture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                          source.Width, source.Height);
    if (texture == nullptr) {
        return nullptr;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    return texture;
}

void UploadAll(SDL_Texture *texture, const ImTextureData &source) {
    SDL_UpdateTexture(texture, nullptr, source.Pixels, source.GetPitch());
}

// 本体のレンダラの絵を要求どおりに作るor直すor捨てる
void ApplyRequest(ImTextureData *texture) {
    {
        if (texture->Status == ImTextureStatus_WantCreate) {
            SDL_Texture *made = MakeTexture(backend->primary, *texture);
            if (made == nullptr) {
                return;
            }
            UploadAll(made, *texture);
            texture->SetTexID(reinterpret_cast<ImTextureID>(made));
            texture->SetStatus(ImTextureStatus_OK);
            backend->revisions[texture] += 1;
            return;
        }

        if (texture->Status == ImTextureStatus_WantUpdates) {
            auto *made = reinterpret_cast<SDL_Texture *>(texture->GetTexID());
            for (const ImTextureRect &area : texture->Updates) {
                const SDL_Rect rect{area.x, area.y, area.w, area.h};
                SDL_UpdateTexture(made, &rect, texture->GetPixelsAt(area.x, area.y),
                                  texture->GetPitch());
            }
            texture->SetStatus(ImTextureStatus_OK);
            backend->revisions[texture] += 1;
            return;
        }

        if (texture->Status == ImTextureStatus_WantDestroy && texture->UnusedFrames > 0) {
            for (auto &entry : backend->copies) {
                const auto found = entry.second.textures.find(texture);
                if (found != entry.second.textures.end()) {
                    SDL_DestroyTexture(found->second);
                    entry.second.textures.erase(found);
                    entry.second.revisions.erase(texture);
                }
            }
            SDL_DestroyTexture(reinterpret_cast<SDL_Texture *>(texture->GetTexID()));
            backend->revisions.erase(texture);
            texture->SetTexID(ImTextureID_Invalid);
            texture->SetStatus(ImTextureStatus_Destroyed);
        }
    }
}

// 途中で増えた絵も取りこぼさない
void ApplyRequests(const ImVector<ImTextureData *> *list) {
    if (list == nullptr) {
        return;
    }
    for (ImTextureData *texture : *list) {
        if (texture->Status != ImTextureStatus_OK) {
            ApplyRequest(texture);
        }
    }
}

// 副ウインドウのレンダラに、同じ中身の絵を用意する
SDL_Texture *CopyFor(SDL_Renderer *renderer, ImTextureData *source) {
    Copies &copies = backend->copies[renderer];
    SDL_Texture *&texture = copies.textures[source];
    if (texture == nullptr) {
        texture = MakeTexture(renderer, *source);
        if (texture == nullptr) {
            return nullptr;
        }
        copies.revisions[source] = 0;
    }

    const std::uint64_t latest = backend->revisions[source];
    if (copies.revisions[source] != latest) {
        if (source->Pixels == nullptr) {
            return texture;
        }
        UploadAll(texture, *source);
        copies.revisions[source] = latest;
    }
    return texture;
}

SDL_Texture *TextureFor(const ImDrawCmd &command, SDL_Renderer *renderer) {
    if (renderer == backend->primary) {
        return reinterpret_cast<SDL_Texture *>(command.GetTexID());
    }
    if (command.TexRef._TexData != nullptr) {
        return CopyFor(renderer, command.TexRef._TexData);
    }

    // 使用者が渡した絵は、作ったレンダラでしか出せない
    auto *given = reinterpret_cast<SDL_Texture *>(command.GetTexID());
    if (given == nullptr || SDL_GetRendererFromTexture(given) != renderer) {
        return nullptr;
    }
    return given;
}

void Renderer_CreateWindow(ImGuiViewport *viewport) {
    // SDL3の口はPlatformHandleに窓の番号を入れる
    const auto id = static_cast<SDL_WindowID>(reinterpret_cast<std::intptr_t>(
        viewport->PlatformHandle));
    SDL_Window *window = SDL_GetWindowFromID(id);
    if (window == nullptr) {
        return;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
    viewport->RendererUserData = renderer;
}

void Renderer_DestroyWindow(ImGuiViewport *viewport) {
    auto *renderer = static_cast<SDL_Renderer *>(viewport->RendererUserData);
    if (renderer == nullptr) {
        return;
    }
    const auto found = backend->copies.find(renderer);
    if (found != backend->copies.end()) {
        for (auto &entry : found->second.textures) {
            SDL_DestroyTexture(entry.second);
        }
        backend->copies.erase(found);
    }
    SDL_DestroyRenderer(renderer);
    viewport->RendererUserData = nullptr;
}

void Renderer_RenderWindow(ImGuiViewport *viewport, void *) {
    auto *renderer = static_cast<SDL_Renderer *>(viewport->RendererUserData);
    if (renderer == nullptr) {
        return;
    }
    if ((viewport->Flags & ImGuiViewportFlags_NoRendererClear) == 0) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
    }
    Draw(viewport->DrawData, renderer);
}

void Renderer_SwapBuffers(ImGuiViewport *viewport, void *) {
    if (auto *renderer = static_cast<SDL_Renderer *>(viewport->RendererUserData)) {
        SDL_RenderPresent(renderer);
    }
}

} // namespace

bool Init(SDL_Renderer *primary) {
    if (primary == nullptr || backend != nullptr) {
        return false;
    }
    backend = new Backend();
    backend->primary = primary;

    ImGuiIO &io = ImGui::GetIO();
    io.BackendRendererName = "TellerSdlRenderer";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;

    ImGuiPlatformIO &platform = ImGui::GetPlatformIO();
    platform.Renderer_CreateWindow = Renderer_CreateWindow;
    platform.Renderer_DestroyWindow = Renderer_DestroyWindow;
    platform.Renderer_RenderWindow = Renderer_RenderWindow;
    platform.Renderer_SwapBuffers = Renderer_SwapBuffers;
    return true;
}

void Shutdown() {
    if (backend == nullptr) {
        return;
    }
    ImGui::DestroyPlatformWindows();

    ImGuiPlatformIO &platform = ImGui::GetPlatformIO();
    platform.Renderer_CreateWindow = nullptr;
    platform.Renderer_DestroyWindow = nullptr;
    platform.Renderer_RenderWindow = nullptr;
    platform.Renderer_SwapBuffers = nullptr;

    for (ImTextureData *texture : ImGui::GetPlatformIO().Textures) {
        if (texture->RefCount == 1) {
            SDL_DestroyTexture(reinterpret_cast<SDL_Texture *>(texture->GetTexID()));
            texture->SetTexID(ImTextureID_Invalid);
            texture->SetStatus(ImTextureStatus_Destroyed);
        }
    }

    ImGuiIO &io = ImGui::GetIO();
    io.BackendRendererName = nullptr;
    io.BackendFlags &=
        ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures |
          ImGuiBackendFlags_RendererHasViewports);

    delete backend;
    backend = nullptr;
}

void NewFrame() {
    if (backend != nullptr) {
        ApplyRequests(&ImGui::GetPlatformIO().Textures);
    }
}

void Draw(ImDrawData *data, SDL_Renderer *renderer) {
    if (backend == nullptr || data == nullptr || renderer == nullptr) {
        return;
    }

    if (renderer == backend->primary) {
        ApplyRequests(data->Textures);
    }

    const float scaleX = data->FramebufferScale.x;
    const float scaleY = data->FramebufferScale.y;
    if (data->DisplaySize.x * scaleX <= 0.0f || data->DisplaySize.y * scaleY <= 0.0f) {
        return;
    }

    float oldScaleX = 1.0f;
    float oldScaleY = 1.0f;
    SDL_GetRenderScale(renderer, &oldScaleX, &oldScaleY);
    const bool oldViewportSet = SDL_RenderViewportSet(renderer);
    const bool oldClipSet = SDL_RenderClipEnabled(renderer);
    SDL_Rect oldViewport{};
    SDL_Rect oldClip{};
    SDL_GetRenderViewport(renderer, &oldViewport);
    SDL_GetRenderClipRect(renderer, &oldClip);

    SDL_SetRenderViewport(renderer, nullptr);
    SDL_SetRenderClipRect(renderer, nullptr);
    SDL_SetRenderScale(renderer, 1.0f, 1.0f);

    const ImVec2 offset = data->DisplayPos;
    for (const ImDrawList *list : data->CmdLists) {
        const ImDrawVert *vertices = list->VtxBuffer.Data;
        const ImDrawIdx *indices = list->IdxBuffer.Data;

        backend->colors.resize(static_cast<std::size_t>(list->VtxBuffer.Size));
        for (int i = 0; i < list->VtxBuffer.Size; ++i) {
            const ImU32 packed = vertices[i].col;
            backend->colors[static_cast<std::size_t>(i)] =
                SDL_FColor{static_cast<float>((packed >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f,
                           static_cast<float>((packed >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f,
                           static_cast<float>((packed >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f,
                           static_cast<float>((packed >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f};
        }

        for (const ImDrawCmd &command : list->CmdBuffer) {
            if (command.UserCallback != nullptr) {
                if (command.UserCallback != ImDrawCallback_ResetRenderState) {
                    command.UserCallback(list, &command);
                }
                continue;
            }

            const float left = (command.ClipRect.x - offset.x) * scaleX;
            const float top = (command.ClipRect.y - offset.y) * scaleY;
            const float right = (command.ClipRect.z - offset.x) * scaleX;
            const float bottom = (command.ClipRect.w - offset.y) * scaleY;
            if (right <= left || bottom <= top) {
                continue;
            }
            const SDL_Rect clip{static_cast<int>(left), static_cast<int>(top),
                                static_cast<int>(right - left), static_cast<int>(bottom - top)};
            SDL_SetRenderClipRect(renderer, &clip);

            SDL_Texture *texture = TextureFor(command, renderer);
            if (texture == nullptr) {
                continue;
            }

            const auto *position = reinterpret_cast<const float *>(
                reinterpret_cast<const char *>(vertices + command.VtxOffset) +
                offsetof(ImDrawVert, pos));
            const auto *uv = reinterpret_cast<const float *>(
                reinterpret_cast<const char *>(vertices + command.VtxOffset) +
                offsetof(ImDrawVert, uv));
            const SDL_FColor *color = backend->colors.data() + command.VtxOffset;

            SDL_RenderGeometryRaw(
                renderer, texture, position, static_cast<int>(sizeof(ImDrawVert)), color,
                static_cast<int>(sizeof(SDL_FColor)), uv,
                static_cast<int>(sizeof(ImDrawVert)),
                static_cast<int>(list->VtxBuffer.Size) - static_cast<int>(command.VtxOffset),
                indices + command.IdxOffset, static_cast<int>(command.ElemCount),
                static_cast<int>(sizeof(ImDrawIdx)));
        }
    }

    SDL_SetRenderViewport(renderer, oldViewportSet ? &oldViewport : nullptr);
    SDL_SetRenderClipRect(renderer, oldClipSet ? &oldClip : nullptr);
    SDL_SetRenderScale(renderer, oldScaleX, oldScaleY);
}

} // namespace TellerEngine::Base::Platform::DebugUiRender

#endif
