#pragma once

#include <Base/Compat.hpp>
#include <Base/Error.hpp>
#include <Base/FontInfo.hpp>
#include <Base/Platform/Image.hpp>

#include <cstddef>
#include <filesystem>
#include <utility>
#include <vector>

namespace TellerEngine::Base::Platform {

// 焼き付ける大きさと範囲
struct TrueTypeSettings {
    // 上端から下端までの画素数
    double size = 24.0;

    // 切ると縁が2値になる
    bool antiAliasing = true;

    // 焼き付ける文字コードの範囲
    // 端も含む
    std::vector<std::pair<char32_t, char32_t>> ranges{{0x20, 0x24F}};
};

// 焼き付けた結果
struct BakedFont {
    FontInfo info;
    Pixels pixels;
};

// 字の矩形は上端を揃えて並べる
Expected<BakedFont, Error> BakeTrueType(Span<const std::byte> contents,
                                        const TrueTypeSettings &settings);

Expected<BakedFont, Error> LoadTrueType(const std::filesystem::path &path,
                                        const TrueTypeSettings &settings);

} // namespace TellerEngine::Base::Platform
