#pragma once

#include <Base/BackgroundInfo.hpp>
#include <Base/Compat.hpp>
#include <Base/Error.hpp>
#include <Base/Platform/Textures.hpp>
#include <Base/SpriteInfo.hpp>

#include <SDL3/SDL.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace TellerEngine::Base::Platform {

// 抽出先の名前から描画に使う番号を得る
// 同じ名前は二度読まない
class AssetTextures {
public:
    AssetTextures(SDL_Renderer *renderer, std::filesystem::path root)
        : textures_(renderer), root_(std::move(root)) {}

    const Textures &Store() const { return textures_; }
    const std::filesystem::path &Root() const { return root_; }

    Expected<ImageId, Error> Sprite(const std::string &name) {
        if (const auto found = sprites_.find(name); found != sprites_.end()) {
            return found->second;
        }

        const auto info = ReadSpriteInfo(root_ / "Sprites" / (name + ".toml"));
        if (!info) {
            return Unexpected<Error>(info.error());
        }

        const auto frames = SpriteFrames(root_, name, info->frameCount);
        const auto image = textures_.Add(Span<const std::filesystem::path>{frames},
                                         info->originX, info->originY);
        if (!image) {
            return Unexpected<Error>(image.error());
        }

        sprites_.emplace(name, *image);
        infos_.emplace(name, *info);
        return *image;
    }

    Expected<ImageId, Error> Background(const std::string &name) {
        if (const auto found = backgrounds_.find(name); found != backgrounds_.end()) {
            return found->second;
        }

        const std::vector<std::filesystem::path> frames{root_ / "Backgrounds" /
                                                        (name + ".png")};
        const auto image =
            textures_.Add(Span<const std::filesystem::path>{frames}, 0.0, 0.0);
        if (!image) {
            return Unexpected<Error>(image.error());
        }

        backgrounds_.emplace(name, *image);
        return *image;
    }

    // 読み込み済みのスプライトの定義
    const SpriteInfo *InfoOf(const std::string &name) const {
        const auto found = infos_.find(name);
        return found == infos_.end() ? nullptr : &found->second;
    }

    std::size_t Count() const { return textures_.Count(); }

private:
    Textures textures_;
    std::filesystem::path root_;
    std::unordered_map<std::string, ImageId> sprites_;
    std::unordered_map<std::string, ImageId> backgrounds_;
    std::unordered_map<std::string, SpriteInfo> infos_;
};

} // namespace TellerEngine::Base::Platform
