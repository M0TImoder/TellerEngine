#pragma once

#include <Base/BackgroundInfo.hpp>
#include <Base/FontInfo.hpp>
#include <Base/Compat.hpp>
#include <Base/Error.hpp>
#include <Base/Platform/Textures.hpp>
#include <Base/Platform/TrueType.hpp>
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

    // 定義があれば書き出した絵を、無ければフォントファイルを焼く
    Expected<FontFace, Error> Font(const std::string &name,
                                   const TrueTypeSettings &settings = {}) {
        if (const auto found = fonts_.find(name); found != fonts_.end()) {
            return FontFace{&found->second.info, found->second.image};
        }

        const std::filesystem::path fonts = root_ / "Fonts";
        const std::filesystem::path definition = fonts / (name + ".toml");

        FontInfo info;
        ImageId image = ImageId::None;

        if (Files::Exists(definition)) {
            auto read = ReadFontInfo(definition);
            if (!read) {
                return Unexpected<Error>(read.error());
            }
            const std::vector<std::filesystem::path> frames{fonts / (name + ".png")};
            const auto placed =
                textures_.Add(Span<const std::filesystem::path>{frames}, 0.0, 0.0);
            if (!placed) {
                return Unexpected<Error>(placed.error());
            }
            info = std::move(*read);
            image = *placed;
        } else {
            const std::filesystem::path outline = OutlinePath(fonts, name);
            if (outline.empty()) {
                return Unexpected<Error>(
                    Error{ErrorCode::NotFound, definition.string()});
            }
            auto baked = LoadTrueType(outline, settings);
            if (!baked) {
                return Unexpected<Error>(baked.error());
            }
            const auto placed = textures_.Add(baked->pixels, 0.0, 0.0);
            if (!placed) {
                return Unexpected<Error>(placed.error());
            }
            info = std::move(baked->info);
            image = *placed;
        }

        const auto placed = fonts_.emplace(name, LoadedFont{std::move(info), image});
        return FontFace{&placed.first->second.info, placed.first->second.image};
    }

    // 読み込み済みのスプライトの定義
    const SpriteInfo *InfoOf(const std::string &name) const {
        const auto found = infos_.find(name);
        return found == infos_.end() ? nullptr : &found->second;
    }

    std::size_t Count() const { return textures_.Count(); }

private:
    static std::filesystem::path OutlinePath(const std::filesystem::path &fonts,
                                             const std::string &name) {
        for (const char *extension : {".ttf", ".otf", ".ttc"}) {
            const std::filesystem::path candidate = fonts / (name + extension);
            if (Files::Exists(candidate)) {
                return candidate;
            }
        }
        return {};
    }

    struct LoadedFont {
        FontInfo info;
        ImageId image = ImageId::None;
    };

    Textures textures_;
    std::filesystem::path root_;
    std::unordered_map<std::string, ImageId> sprites_;
    std::unordered_map<std::string, ImageId> backgrounds_;
    std::unordered_map<std::string, SpriteInfo> infos_;
    std::unordered_map<std::string, LoadedFont> fonts_;
};

} // namespace TellerEngine::Base::Platform
