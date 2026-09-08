#pragma once

#include <Base/Compat.hpp>
#include <Base/Error.hpp>
#include <Base/Files.hpp>

#include <toml++/toml.hpp>

#include <filesystem>
#include <string>

namespace TellerEngine::Base {

// 抽出した背景の定義
struct BackgroundInfo {
    bool transparent = false;
    bool smooth = false;
    bool preload = false;
};

inline Expected<BackgroundInfo, Error> ReadBackgroundInfo(const std::filesystem::path &path) {
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
    BackgroundInfo info;
    info.transparent = table["transparent"].value_or(false);
    info.smooth = table["smooth"].value_or(false);
    info.preload = table["preload"].value_or(false);
    return info;
}

} // namespace TellerEngine::Base
