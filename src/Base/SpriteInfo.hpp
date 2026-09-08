#pragma once

#include <Base/Compat.hpp>
#include <Base/Error.hpp>
#include <Base/Files.hpp>

#include <toml++/toml.hpp>

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace TellerEngine::Base {

// 抽出したスプライトの定義
struct SpriteInfo {
    int width = 0;
    int height = 0;
    double originX = 0.0;
    double originY = 0.0;
    std::size_t frameCount = 0;
    std::size_t maskCount = 0;
};

inline Expected<SpriteInfo, Error> ReadSpriteInfo(const std::filesystem::path &path) {
    const auto text = Files::ReadText(path);
    if (!text) {
        return Unexpected<Error>(text.error());
    }

    toml::parse_result parsed = toml::parse(*text);
    if (!parsed) {
        return Unexpected<Error>(
            Error{ErrorCode::Malformed,
                  path.string() + ": " + std::string(parsed.error().description())});
    }

    const toml::table &table = parsed.table();
    SpriteInfo info;
    info.width = static_cast<int>(table["width"].value_or(0));
    info.height = static_cast<int>(table["height"].value_or(0));
    info.originX = table["origin_x"].value_or(0.0);
    info.originY = table["origin_y"].value_or(0.0);
    info.frameCount = static_cast<std::size_t>(table["frame_count"].value_or(0));
    info.maskCount = static_cast<std::size_t>(table["mask_count"].value_or(0));
    return info;
}

// コマのファイルを順に並べる
inline std::vector<std::filesystem::path> SpriteFrames(const std::filesystem::path &root,
                                                       const std::string &name,
                                                       std::size_t frameCount) {
    std::vector<std::filesystem::path> frames;
    frames.reserve(frameCount);
    for (std::size_t i = 0; i < frameCount; ++i) {
        frames.push_back(root / "Sprites" / name / (std::to_string(i) + ".png"));
    }
    return frames;
}

} // namespace TellerEngine::Base
