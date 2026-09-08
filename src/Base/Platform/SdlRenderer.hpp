#pragma once

#include <Base/Compat.hpp>
#include <Base/Draw.hpp>
#include <Base/Error.hpp>
#include <Base/Platform/Textures.hpp>
#include <Base/Platform/Window.hpp>
#include <Base/Renderer.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace TellerEngine::Base::Platform {

inline constexpr double kPi = 3.14159265358979323846;

class SdlRenderer : public Renderer {
public:
    static Expected<SdlRenderer, Error> Create(Window &window) {
        SDL_Renderer *handle = SDL_CreateRenderer(window.Handle(), nullptr);
        if (handle == nullptr) {
            return Unexpected<Error>(Error{ErrorCode::Unavailable, SDL_GetError()});
        }
        return SdlRenderer{handle};
    }

    SdlRenderer(const SdlRenderer &) = delete;
    SdlRenderer &operator=(const SdlRenderer &) = delete;

    SdlRenderer(SdlRenderer &&other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)),
          textures_(std::exchange(other.textures_, nullptr)), clear_(other.clear_) {}

    SdlRenderer &operator=(SdlRenderer &&other) noexcept {
        if (this != &other) {
            Release();
            handle_ = std::exchange(other.handle_, nullptr);
            textures_ = std::exchange(other.textures_, nullptr);
            clear_ = other.clear_;
        }
        return *this;
    }

    ~SdlRenderer() override { Release(); }

    SDL_Renderer *Handle() const { return handle_; }

    void SetClearColor(Color color) override { clear_ = color; }
    Color ClearColor() const override { return clear_; }

    void SetTextures(const Textures *textures) { textures_ = textures; }

    void Render(const DrawList &list) override {
        SDL_SetRenderDrawColor(handle_, clear_.red, clear_.green, clear_.blue, clear_.alpha);
        SDL_RenderClear(handle_);

        for (const DrawCommand &command : list.Commands()) {
            Apply(command);
            switch (command.kind) {
            case DrawKind::Rectangle:
                DrawRectangle(command);
                break;
            case DrawKind::RoundRectangle:
                DrawRoundRectangle(command);
                break;
            case DrawKind::Line:
                DrawLine(command);
                break;
            case DrawKind::Circle:
                DrawEllipse(command, command.x, command.y, command.radius, command.radius);
                break;
            case DrawKind::Ellipse:
                DrawEllipse(command, (command.x + command.secondX) * 0.5,
                            (command.y + command.secondY) * 0.5,
                            std::abs(command.secondX - command.x) * 0.5,
                            std::abs(command.secondY - command.y) * 0.5);
                break;
            case DrawKind::Triangle:
                DrawTriangle(command);
                break;
            case DrawKind::Sprite:
                DrawSprite(command);
                break;
            case DrawKind::Target:
                SetTarget(command.image);
                break;
            case DrawKind::Text:
                break;
            }
        }

        SDL_SetRenderTarget(handle_, nullptr);
    }

    void Flip() override { SDL_RenderPresent(handle_); }

private:
    explicit SdlRenderer(SDL_Renderer *handle) : handle_(handle) {}

    void Release() {
        if (handle_ != nullptr) {
            SDL_DestroyRenderer(handle_);
            handle_ = nullptr;
        }
    }

    void Apply(const DrawCommand &command) {
        const double alpha = command.alpha * (command.color.alpha / 255.0);
        SDL_SetRenderDrawColor(handle_, command.color.red, command.color.green,
                               command.color.blue,
                               static_cast<Uint8>(std::lround(alpha * 255.0)));
        SDL_SetRenderDrawBlendMode(handle_, BlendOf(command.blend));
    }

    static SDL_BlendMode BlendOf(BlendMode blend) {
        switch (blend) {
        case BlendMode::None:
            return SDL_BLENDMODE_NONE;
        case BlendMode::Add:
            return SDL_BLENDMODE_ADD;
        case BlendMode::Multiply:
            return SDL_BLENDMODE_MUL;
        case BlendMode::Subtract:
            return Subtract();
        case BlendMode::Normal:
            break;
        }
        return SDL_BLENDMODE_BLEND;
    }

    static SDL_BlendMode Subtract() {
        static const SDL_BlendMode mode = SDL_ComposeCustomBlendMode(
            SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDFACTOR_ONE,
            SDL_BLENDOPERATION_REV_SUBTRACT, SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE,
            SDL_BLENDOPERATION_ADD);
        return mode;
    }

    void DrawRectangle(const DrawCommand &command) {
        SDL_FRect rect{static_cast<float>(command.x), static_cast<float>(command.y),
                       static_cast<float>(command.width), static_cast<float>(command.height)};
        if (!command.filled) {
            SDL_RenderRect(handle_, &rect);
            return;
        }
        if (Uniform(command)) {
            SDL_RenderFillRect(handle_, &rect);
            return;
        }

        const float left = rect.x;
        const float top = rect.y;
        const float right = rect.x + rect.w;
        const float bottom = rect.y + rect.h;
        const SDL_Vertex corners[4] = {
            {{left, top}, ColorOf(command.color, command.alpha), {0.0f, 0.0f}},
            {{right, top}, ColorOf(command.extra[0], command.alpha), {0.0f, 0.0f}},
            {{right, bottom}, ColorOf(command.extra[1], command.alpha), {0.0f, 0.0f}},
            {{left, bottom}, ColorOf(command.extra[2], command.alpha), {0.0f, 0.0f}},
        };
        const int order[6] = {0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(handle_, nullptr, corners, 4, order, 6);
    }

    static bool Uniform(const DrawCommand &command) {
        return command.extra[0] == command.color && command.extra[1] == command.color &&
               command.extra[2] == command.color;
    }

    void DrawLine(const DrawCommand &command) {
        if (command.lineWidth <= 1.0) {
            SDL_RenderLine(handle_, static_cast<float>(command.x),
                           static_cast<float>(command.y),
                           static_cast<float>(command.secondX),
                           static_cast<float>(command.secondY));
            return;
        }

        const double deltaX = command.secondX - command.x;
        const double deltaY = command.secondY - command.y;
        const double length = std::hypot(deltaX, deltaY);
        if (length <= 0.0) {
            return;
        }
        const double half = command.lineWidth * 0.5;
        const double offsetX = -deltaY / length * half;
        const double offsetY = deltaX / length * half;

        const SDL_FColor color = ColorOf(command.color, command.alpha);
        const SDL_Vertex corners[4] = {
            {{static_cast<float>(command.x + offsetX), static_cast<float>(command.y + offsetY)},
             color,
             {0.0f, 0.0f}},
            {{static_cast<float>(command.x - offsetX), static_cast<float>(command.y - offsetY)},
             color,
             {0.0f, 0.0f}},
            {{static_cast<float>(command.secondX - offsetX),
              static_cast<float>(command.secondY - offsetY)},
             color,
             {0.0f, 0.0f}},
            {{static_cast<float>(command.secondX + offsetX),
              static_cast<float>(command.secondY + offsetY)},
             color,
             {0.0f, 0.0f}},
        };
        const int order[6] = {0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(handle_, nullptr, corners, 4, order, 6);
    }

    void DrawEllipse(const DrawCommand &command, double centerX, double centerY,
                     double radiusX, double radiusY) {
        const auto segments = static_cast<int>(command.segments < 4 ? 4 : command.segments);

        const auto pointAt = [&](int index) {
            const double angle = 2.0 * kPi * index / segments;
            return SDL_FPoint{static_cast<float>(centerX + radiusX * std::cos(angle)),
                              static_cast<float>(centerY + radiusY * std::sin(angle))};
        };

        if (!command.filled) {
            std::vector<SDL_FPoint> points;
            points.reserve(static_cast<std::size_t>(segments) + 1);
            for (int i = 0; i <= segments; ++i) {
                points.push_back(pointAt(i));
            }
            SDL_RenderLines(handle_, points.data(), static_cast<int>(points.size()));
            return;
        }

        const SDL_FColor inner = ColorOf(command.color, command.alpha);
        const SDL_FColor outer = ColorOf(command.extra[0], command.alpha);
        std::vector<SDL_Vertex> vertices;
        vertices.reserve(static_cast<std::size_t>(segments) * 3);
        for (int i = 0; i < segments; ++i) {
            vertices.push_back(SDL_Vertex{
                {static_cast<float>(centerX), static_cast<float>(centerY)}, inner, {0.0f, 0.0f}});
            vertices.push_back(SDL_Vertex{pointAt(i), outer, {0.0f, 0.0f}});
            vertices.push_back(SDL_Vertex{pointAt(i + 1), outer, {0.0f, 0.0f}});
        }
        SDL_RenderGeometry(handle_, nullptr, vertices.data(),
                           static_cast<int>(vertices.size()), nullptr, 0);
    }

    // 角を丸めた矩形
    void DrawRoundRectangle(const DrawCommand &command) {
        const double radius =
            std::min({std::abs(command.width), std::abs(command.height), 10.0}) * 0.5;
        const double left = command.x;
        const double top = command.y;
        const double right = command.x + command.width;
        const double bottom = command.y + command.height;

        if (!command.filled) {
            SDL_RenderLine(handle_, static_cast<float>(left + radius), static_cast<float>(top),
                           static_cast<float>(right - radius), static_cast<float>(top));
            SDL_RenderLine(handle_, static_cast<float>(left + radius),
                           static_cast<float>(bottom), static_cast<float>(right - radius),
                           static_cast<float>(bottom));
            SDL_RenderLine(handle_, static_cast<float>(left), static_cast<float>(top + radius),
                           static_cast<float>(left), static_cast<float>(bottom - radius));
            SDL_RenderLine(handle_, static_cast<float>(right), static_cast<float>(top + radius),
                           static_cast<float>(right), static_cast<float>(bottom - radius));
            return;
        }

        SDL_FRect middle{static_cast<float>(left), static_cast<float>(top + radius),
                         static_cast<float>(command.width),
                         static_cast<float>(command.height - radius * 2.0)};
        SDL_FRect upper{static_cast<float>(left + radius), static_cast<float>(top),
                        static_cast<float>(command.width - radius * 2.0),
                        static_cast<float>(radius)};
        SDL_FRect lower{static_cast<float>(left + radius),
                        static_cast<float>(bottom - radius),
                        static_cast<float>(command.width - radius * 2.0),
                        static_cast<float>(radius)};
        SDL_RenderFillRect(handle_, &middle);
        SDL_RenderFillRect(handle_, &upper);
        SDL_RenderFillRect(handle_, &lower);
    }

    void SetTarget(ImageId surface) {
        if (surface == ImageId::None || textures_ == nullptr) {
            SDL_SetRenderTarget(handle_, nullptr);
            return;
        }
        SDL_SetRenderTarget(handle_, textures_->FrameOf(surface, 0));
    }

    void DrawSprite(const DrawCommand &command) {
        if (textures_ == nullptr) {
            return;
        }
        const SpriteEntry *sprite = textures_->Find(command.image);
        SDL_Texture *frame = textures_->FrameOf(command.image, command.frame);
        if (sprite == nullptr || frame == nullptr) {
            return;
        }

        SDL_SetTextureColorMod(frame, command.color.red, command.color.green,
                               command.color.blue);
        SDL_SetTextureAlphaMod(
            frame, static_cast<Uint8>(std::lround(
                       command.alpha * (command.color.alpha / 255.0) * 255.0)));
        SDL_SetTextureBlendMode(frame, BlendOf(command.blend));

        SDL_FRect source{0.0f, 0.0f, static_cast<float>(sprite->width),
                         static_cast<float>(sprite->height)};
        double sourceWidth = sprite->width;
        double sourceHeight = sprite->height;

        if (command.usePart) {
            const double left = std::clamp(command.partX, 0.0, static_cast<double>(sprite->width));
            const double top =
                std::clamp(command.partY, 0.0, static_cast<double>(sprite->height));
            sourceWidth = std::clamp(command.partWidth, 0.0, sprite->width - left);
            sourceHeight = std::clamp(command.partHeight, 0.0, sprite->height - top);
            if (sourceWidth <= 0.0 || sourceHeight <= 0.0) {
                return;
            }
            source = SDL_FRect{static_cast<float>(left), static_cast<float>(top),
                               static_cast<float>(sourceWidth),
                               static_cast<float>(sourceHeight)};
        }

        double width = sourceWidth * command.scaleX;
        double height = sourceHeight * command.scaleY;
        if (!command.usePart && command.ignoreOrigin && command.width > 0.0 &&
            command.height > 0.0) {
            width = command.width;
            height = command.height;
        }

        const double offsetX = command.ignoreOrigin ? 0.0 : sprite->originX * command.scaleX;
        const double offsetY = command.ignoreOrigin ? 0.0 : sprite->originY * command.scaleY;

        SDL_FRect destination{static_cast<float>(command.x - offsetX),
                              static_cast<float>(command.y - offsetY),
                              static_cast<float>(width), static_cast<float>(height)};

        if (command.rotation == 0.0) {
            SDL_RenderTexture(handle_, frame, &source, &destination);
            return;
        }

        const SDL_FPoint center{static_cast<float>(offsetX), static_cast<float>(offsetY)};
        SDL_RenderTextureRotated(handle_, frame, &source, &destination, -command.rotation,
                                 &center, SDL_FLIP_NONE);
    }

    void DrawTriangle(const DrawCommand &command) {
        if (!command.filled) {
            const SDL_FPoint points[4] = {
                {static_cast<float>(command.x), static_cast<float>(command.y)},
                {static_cast<float>(command.secondX), static_cast<float>(command.secondY)},
                {static_cast<float>(command.thirdX), static_cast<float>(command.thirdY)},
                {static_cast<float>(command.x), static_cast<float>(command.y)},
            };
            SDL_RenderLines(handle_, points, 4);
            return;
        }

        const SDL_Vertex vertices[3] = {
            {{static_cast<float>(command.x), static_cast<float>(command.y)},
             ColorOf(command.color, command.alpha),
             {0.0f, 0.0f}},
            {{static_cast<float>(command.secondX), static_cast<float>(command.secondY)},
             ColorOf(command.extra[0], command.alpha),
             {0.0f, 0.0f}},
            {{static_cast<float>(command.thirdX), static_cast<float>(command.thirdY)},
             ColorOf(command.extra[1], command.alpha),
             {0.0f, 0.0f}},
        };
        SDL_RenderGeometry(handle_, nullptr, vertices, 3, nullptr, 0);
    }

    static SDL_FColor ColorOf(Color color, double alpha) {
        return SDL_FColor{static_cast<float>(color.red / 255.0),
                          static_cast<float>(color.green / 255.0),
                          static_cast<float>(color.blue / 255.0),
                          static_cast<float>(alpha * (color.alpha / 255.0))};
    }

    SDL_Renderer *handle_ = nullptr;
    const Textures *textures_ = nullptr;
    Color clear_{0, 0, 0, 255};
};

} // namespace TellerEngine::Base::Platform
