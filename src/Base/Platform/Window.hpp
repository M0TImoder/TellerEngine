#pragma once

#include <Base/Compat.hpp>
#include <Base/Error.hpp>

#include <SDL3/SDL.h>

#include <string>
#include <utility>

namespace TellerEngine::Base::Platform {

class Window {
public:
    static Expected<Window, Error> Create(const std::string &title, int width, int height) {
        SDL_Window *handle = SDL_CreateWindow(title.c_str(), width, height, 0);
        if (handle == nullptr) {
            return Unexpected<Error>(Error{ErrorCode::Unavailable, SDL_GetError()});
        }
        return Window{handle};
    }

    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;

    Window(Window &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

    Window &operator=(Window &&other) noexcept {
        if (this != &other) {
            Release();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    ~Window() { Release(); }

    SDL_Window *Handle() const { return handle_; }

    int Width() const {
        int width = 0;
        SDL_GetWindowSize(handle_, &width, nullptr);
        return width;
    }

    int Height() const {
        int height = 0;
        SDL_GetWindowSize(handle_, nullptr, &height);
        return height;
    }

    void SetTitle(const std::string &title) { SDL_SetWindowTitle(handle_, title.c_str()); }

private:
    explicit Window(SDL_Window *handle) : handle_(handle) {}

    void Release() {
        if (handle_ != nullptr) {
            SDL_DestroyWindow(handle_);
            handle_ = nullptr;
        }
    }

    SDL_Window *handle_ = nullptr;
};

} // namespace TellerEngine::Base::Platform
