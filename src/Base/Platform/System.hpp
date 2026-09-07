#pragma once

#include <Base/Compat.hpp>
#include <Base/Error.hpp>

#include <SDL3/SDL.h>

#include <utility>

#if defined(__EMSCRIPTEN_PTHREADS__)
#error "TellerEngineは単一スレッドで組む"
#endif

namespace TellerEngine::Base::Platform {

// SDLの初期化と後始末を持つ
class System {
public:
    static Expected<System, Error> Create(SDL_InitFlags flags) {
        if (!SDL_Init(flags)) {
            return Unexpected<Error>(Error{ErrorCode::Unavailable, SDL_GetError()});
        }
        return System{true};
    }

    System(const System &) = delete;
    System &operator=(const System &) = delete;

    System(System &&other) noexcept : owned_(std::exchange(other.owned_, false)) {}

    System &operator=(System &&other) noexcept {
        if (this != &other) {
            Release();
            owned_ = std::exchange(other.owned_, false);
        }
        return *this;
    }

    ~System() { Release(); }

private:
    explicit System(bool owned) : owned_(owned) {}

    void Release() {
        if (owned_) {
            SDL_Quit();
            owned_ = false;
        }
    }

    bool owned_ = false;
};

} // namespace TellerEngine::Base::Platform
