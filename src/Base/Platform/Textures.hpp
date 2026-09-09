#pragma once

#include <Base/Compat.hpp>
#include <Base/Error.hpp>
#include <Base/Draw.hpp>
#include <Base/Platform/Image.hpp>

#include <SDL3/SDL.h>

#include <cstdint>
#include <filesystem>
#include <utility>
#include <vector>

namespace TellerEngine::Base::Platform {

// 1つのスプライトのコマと原点
struct SpriteEntry {
    std::vector<SDL_Texture *> frames;
    double originX = 0.0;
    double originY = 0.0;
    int width = 0;
    int height = 0;
};

// 描画コマンドが指す番号と実物の対応を持つ
class Textures {
public:
    explicit Textures(SDL_Renderer *renderer) : renderer_(renderer) {}

    Textures(const Textures &) = delete;
    Textures &operator=(const Textures &) = delete;

    Textures(Textures &&other) noexcept
        : renderer_(std::exchange(other.renderer_, nullptr)),
          sprites_(std::move(other.sprites_)) {
        other.sprites_.clear();
    }

    ~Textures() { Clear(); }

    void Clear() {
        for (SpriteEntry &sprite : sprites_) {
            for (SDL_Texture *frame : sprite.frames) {
                SDL_DestroyTexture(frame);
            }
        }
        sprites_.clear();
    }

    // 描き込める絵を1枚作る
    Expected<ImageId, Error> AddTarget(int width, int height) {
        SDL_Texture *texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
                                                 SDL_TEXTUREACCESS_TARGET, width, height);
        if (texture == nullptr) {
            return Unexpected<Error>(Error{ErrorCode::Unavailable, SDL_GetError()});
        }
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

        SpriteEntry sprite;
        sprite.width = width;
        sprite.height = height;
        sprite.frames.push_back(texture);
        sprites_.push_back(std::move(sprite));
        return static_cast<ImageId>(sprites_.size());
    }

    // 絵を捨てる
    void Free(ImageId image) {
        const auto index = static_cast<std::uint32_t>(image);
        if (index < 1 || index > sprites_.size()) {
            return;
        }
        SpriteEntry &sprite = sprites_[index - 1];
        for (SDL_Texture *frame : sprite.frames) {
            SDL_DestroyTexture(frame);
        }
        sprite.frames.clear();
    }

    // コマを並べて登録し、番号を返す
    Expected<ImageId, Error> Add(Span<const std::filesystem::path> framePaths,
                                       double originX, double originY) {
        SpriteEntry sprite;
        sprite.originX = originX;
        sprite.originY = originY;

        for (const std::filesystem::path &path : framePaths) {
            auto pixels = LoadPng(path);
            if (!pixels) {
                for (SDL_Texture *made : sprite.frames) {
                    SDL_DestroyTexture(made);
                }
                return Unexpected<Error>(pixels.error());
            }

            SDL_Texture *texture = Upload(*pixels);
            if (texture == nullptr) {
                for (SDL_Texture *made : sprite.frames) {
                    SDL_DestroyTexture(made);
                }
                return Unexpected<Error>(Error{ErrorCode::Unavailable, SDL_GetError()});
            }

            sprite.width = pixels->width;
            sprite.height = pixels->height;
            sprite.frames.push_back(texture);
        }

        sprites_.push_back(std::move(sprite));
        return static_cast<ImageId>(sprites_.size());
    }

    // 焼いた絵をそのまま登録する
    Expected<ImageId, Error> Add(const Pixels &pixels, double originX, double originY) {
        SDL_Texture *texture = Upload(pixels);
        if (texture == nullptr) {
            return Unexpected<Error>(Error{ErrorCode::Unavailable, SDL_GetError()});
        }

        SpriteEntry sprite;
        sprite.originX = originX;
        sprite.originY = originY;
        sprite.width = pixels.width;
        sprite.height = pixels.height;
        sprite.frames.push_back(texture);
        sprites_.push_back(std::move(sprite));
        return static_cast<ImageId>(sprites_.size());
    }

    const SpriteEntry *Find(ImageId image) const {
        const auto index = static_cast<std::uint32_t>(image);
        if (index < 1 || index > sprites_.size() || sprites_[index - 1].frames.empty()) {
            return nullptr;
        }
        return &sprites_[index - 1];
    }

    // 並びの外を指したら最初のコマへ戻る
    SDL_Texture *FrameOf(ImageId image, std::uint32_t frame) const {
        const SpriteEntry *sprite = Find(image);
        if (sprite == nullptr || sprite->frames.empty()) {
            return nullptr;
        }
        return sprite->frames[frame % sprite->frames.size()];
    }

    std::size_t Count() const { return sprites_.size(); }

private:
    SDL_Texture *Upload(const Pixels &pixels) {
        SDL_Texture *texture =
            SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                              pixels.width, pixels.height);
        if (texture == nullptr) {
            return nullptr;
        }
        SDL_UpdateTexture(texture, nullptr, pixels.data.data(), pixels.width * 4);
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        return texture;
    }

    SDL_Renderer *renderer_ = nullptr;
    std::vector<SpriteEntry> sprites_;
};

} // namespace TellerEngine::Base::Platform
