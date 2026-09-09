#include <Base/Files.hpp>
#include <Base/Platform/TrueType.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include <stb_truetype.h>
#pragma GCC diagnostic pop

namespace TellerEngine::Base::Platform {

namespace {

// 1文字ぶんの濃さと、行の上端からの位置
struct Drawn {
    Glyph glyph;
    std::vector<unsigned char> ink;
    int inkWidth = 0;
    int inkHeight = 0;
    int inkTop = 0;
};

int RoundUpPowerOfTwo(int value) {
    int result = 1;
    while (result < value) {
        result *= 2;
    }
    return result;
}

} // namespace

Expected<BakedFont, Error> BakeTrueType(Span<const std::byte> contents,
                                        const TrueTypeSettings &settings) {
    if (contents.empty()) {
        return Unexpected<Error>(Error{ErrorCode::Malformed, "フォントが空"});
    }

    const auto *bytes = reinterpret_cast<const unsigned char *>(contents.data());
    const int start = stbtt_GetFontOffsetForIndex(bytes, 0);
    stbtt_fontinfo font;
    if (start < 0 || stbtt_InitFont(&font, bytes, start) == 0) {
        return Unexpected<Error>(Error{ErrorCode::Malformed, "フォントを読めない"});
    }

    const double size = settings.size > 0.0 ? settings.size : 24.0;
    const float scale = stbtt_ScaleForPixelHeight(&font, static_cast<float>(size));
    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
    const int ascentTop = static_cast<int>(std::ceil(ascent * scale));

    std::vector<Drawn> drawn;
    for (const auto &range : settings.ranges) {
        for (char32_t code = range.first; code <= range.second; ++code) {
            const int index = stbtt_FindGlyphIndex(&font, static_cast<int>(code));
            if (index == 0) {
                continue;
            }

            int advance = 0;
            int bearing = 0;
            stbtt_GetCodepointHMetrics(&font, static_cast<int>(code), &advance, &bearing);

            Drawn entry;
            entry.glyph.character = code;
            entry.glyph.shift = static_cast<int>(std::lround(advance * scale));

            int left = 0;
            int top = 0;
            int right = 0;
            int bottom = 0;
            stbtt_GetCodepointBitmapBox(&font, static_cast<int>(code), scale, scale, &left,
                                        &top, &right, &bottom);
            const int width = right - left;
            const int height = bottom - top;
            if (width <= 0 || height <= 0) {
                drawn.push_back(std::move(entry));
                continue;
            }

            int skip = 0;
            entry.inkTop = ascentTop + top;
            if (entry.inkTop < 0) {
                skip = -entry.inkTop;
                entry.inkTop = 0;
            }
            const int rows = height - skip;
            if (rows <= 0) {
                drawn.push_back(std::move(entry));
                continue;
            }

            entry.ink.assign(static_cast<std::size_t>(width * height), 0);
            stbtt_MakeCodepointBitmap(&font, entry.ink.data(), width, height, width, scale,
                                      scale, static_cast<int>(code));
            if (!settings.antiAliasing) {
                for (unsigned char &value : entry.ink) {
                    value = value >= 128 ? 255 : 0;
                }
            }

            entry.inkWidth = width;
            entry.inkHeight = height;
            entry.ink.erase(entry.ink.begin(),
                            entry.ink.begin() + static_cast<std::ptrdiff_t>(skip * width));
            entry.inkHeight = rows;
            entry.glyph.width = width;
            entry.glyph.height = entry.inkTop + rows;
            entry.glyph.offset = left;
            drawn.push_back(std::move(entry));
        }
    }

    if (drawn.empty()) {
        return Unexpected<Error>(Error{ErrorCode::Malformed, "焼ける字が無い"});
    }

    int widest = 0;
    for (const Drawn &entry : drawn) {
        widest = std::max(widest, entry.glyph.width);
    }
    const int atlasWidth = std::max(512, RoundUpPowerOfTwo(widest + 1));

    int penX = 0;
    int penY = 0;
    int shelf = 0;
    for (Drawn &entry : drawn) {
        if (entry.glyph.width <= 0 || entry.glyph.height <= 0) {
            continue;
        }
        if (penX + entry.glyph.width + 1 > atlasWidth) {
            penX = 0;
            penY += shelf + 1;
            shelf = 0;
        }
        entry.glyph.x = penX;
        entry.glyph.y = penY;
        penX += entry.glyph.width + 1;
        shelf = std::max(shelf, entry.glyph.height);
    }
    const int atlasHeight = RoundUpPowerOfTwo(penY + shelf + 1);

    BakedFont baked;
    baked.pixels.width = atlasWidth;
    baked.pixels.height = atlasHeight;
    baked.pixels.data.assign(static_cast<std::size_t>(atlasWidth * atlasHeight * 4), 0);

    for (const Drawn &entry : drawn) {
        for (int row = 0; row < entry.inkHeight; ++row) {
            for (int column = 0; column < entry.inkWidth; ++column) {
                const auto coverage =
                    entry.ink[static_cast<std::size_t>(row * entry.inkWidth + column)];
                const int x = entry.glyph.x + column;
                const int y = entry.glyph.y + entry.inkTop + row;
                const auto offset =
                    static_cast<std::size_t>((y * atlasWidth + x) * 4);
                baked.pixels.data[offset + 0] = 255;
                baked.pixels.data[offset + 1] = 255;
                baked.pixels.data[offset + 2] = 255;
                baked.pixels.data[offset + 3] = coverage;
            }
        }
        baked.info.glyphs.push_back(entry.glyph);
    }

    std::sort(baked.info.glyphs.begin(), baked.info.glyphs.end(),
              [](const Glyph &left, const Glyph &right) {
                  return left.character < right.character;
              });
    baked.info.emSize = static_cast<int>(std::lround(size));
    baked.info.height = baked.info.Height();
    return baked;
}

Expected<BakedFont, Error> LoadTrueType(const std::filesystem::path &path,
                                        const TrueTypeSettings &settings) {
    auto contents = Files::ReadBytes(path);
    if (!contents) {
        return Unexpected<Error>(contents.error());
    }

    auto baked = BakeTrueType(Span<const std::byte>{*contents}, settings);
    if (!baked) {
        return Unexpected<Error>(Error{baked.error().code, path.string() + ": " +
                                                               baked.error().context});
    }
    baked->info.displayName = path.stem().string();
    return baked;
}

} // namespace TellerEngine::Base::Platform
