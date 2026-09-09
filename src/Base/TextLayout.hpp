#pragma once

#include <Base/FontInfo.hpp>
#include <Base/Text.hpp>
#include <Base/Utf8.hpp>

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

namespace TellerEngine::Base {

// 文字を並べる決まり
struct TextLayoutSettings {
    const FontInfo *font = nullptr;

    // 省くとfontをrubyScale倍にして使う
    const FontInfo *rubyFont = nullptr;

    double x = 0.0;
    double y = 0.0;

    // これを越えた位置から書き始める文字は次の行へ送る
    // 0以下なら折り返さない
    double lineEnd = 0.0;

    // 0より大きければどの文字もこの幅で送る
    double spacing = 0.0;

    // 0以下ならフォントの高さ
    double lineHeight = 0.0;

    double rubyScale = 0.5;

    // ルビと本文の間
    double rubyGap = 1.0;

    double scaleX = 1.0;
    double scaleY = 1.0;
};

struct PlacedGlyph {
    char32_t character = 0;

    // 元の並びでのバイト位置
    std::uint32_t at = 0;

    std::uint32_t line = 0;
    double x = 0.0;
    double y = 0.0;
    double scaleX = 1.0;
    double scaleY = 1.0;
    TextStyle style;
    bool ruby = false;
};

struct LaidOutLine {
    double top = 0.0;
    double height = 0.0;

    // ルビのぶんだけ本文が下がる
    double rubyHeight = 0.0;

    double width = 0.0;
};

struct TextLayout {
    std::vector<PlacedGlyph> glyphs;
    std::vector<LaidOutLine> lines;
    double width = 0.0;
    double height = 0.0;
};

namespace Detail {

inline double GlyphAdvance(const FontInfo &font, const TextLayoutSettings &settings,
                           char32_t character, double scaleX) {
    const double base = settings.spacing > 0.0 ? settings.spacing : font.Advance(character);
    return base * scaleX;
}

} // namespace Detail

// 半分の大きさの文字が下がる割合
inline constexpr double kHalfSizeDrop = 0.33;

inline TextLayout LayOutText(const Text &text, const TextLayoutSettings &settings) {
    TextLayout layout;
    if (settings.font == nullptr) {
        return layout;
    }

    const FontInfo &font = *settings.font;
    const double lineHeight =
        settings.lineHeight > 0.0 ? settings.lineHeight : font.Height() * settings.scaleY;

    std::vector<std::uint32_t> breaks;
    for (const TextEvent &event : text.Events()) {
        if (event.kind == TextEventKind::Break) {
            breaks.push_back(event.at);
        }
    }

    const std::string_view characters = text.Characters();
    layout.lines.push_back(LaidOutLine{});

    std::uint32_t line = 0;
    double pen = settings.x;
    std::size_t nextBreak = 0;

    const auto newLine = [&]() {
        line += 1;
        pen = settings.x;
        layout.lines.push_back(LaidOutLine{});
    };

    std::size_t position = 0;
    while (true) {
        while (nextBreak < breaks.size() && breaks[nextBreak] <= position) {
            if (breaks[nextBreak] == position) {
                newLine();
            }
            nextBreak += 1;
        }
        if (position >= characters.size()) {
            break;
        }

        const Utf8Char letter = DecodeUtf8(characters, position);
        const TextStyle style = text.StyleAt(static_cast<std::uint32_t>(position));
        const double shrink = style.halfSize ? 0.5 : 1.0;
        const double scaleX = settings.scaleX * shrink;
        const double scaleY = settings.scaleY * shrink;

        if (settings.lineEnd > 0.0 && pen > settings.lineEnd) {
            newLine();
        }

        PlacedGlyph placed;
        placed.character = letter.code;
        placed.at = static_cast<std::uint32_t>(position);
        placed.line = line;
        placed.x = pen;
        placed.scaleX = scaleX;
        placed.scaleY = scaleY;
        placed.style = style;
        layout.glyphs.push_back(placed);

        pen += Detail::GlyphAdvance(font, settings, letter.code, scaleX);
        layout.lines[line].width = pen - settings.x;
        position += letter.size;
    }

    const std::size_t baseCount = layout.glyphs.size();
    const FontInfo &rubyFont = settings.rubyFont != nullptr ? *settings.rubyFont : font;
    const double rubyShrink = settings.rubyFont != nullptr ? 1.0 : settings.rubyScale;
    const double rubyHeight =
        rubyFont.Height() * rubyShrink * settings.scaleY + settings.rubyGap;

    for (const TextRuby &ruby : text.Rubies()) {
        std::size_t groupBegin = 0;
        while (groupBegin < baseCount) {
            if (layout.glyphs[groupBegin].at < ruby.begin ||
                layout.glyphs[groupBegin].at >= ruby.end) {
                groupBegin += 1;
                continue;
            }

            std::size_t groupEnd = groupBegin;
            while (groupEnd + 1 < baseCount && layout.glyphs[groupEnd + 1].at < ruby.end &&
                   layout.glyphs[groupEnd + 1].line == layout.glyphs[groupBegin].line) {
                groupEnd += 1;
            }

            const PlacedGlyph first = layout.glyphs[groupBegin];
            const PlacedGlyph last = layout.glyphs[groupEnd];
            const double left = first.x;
            const double right =
                last.x + Detail::GlyphAdvance(font, settings, last.character, last.scaleX);

            double readingWidth = 0.0;
            std::size_t cursor = 0;
            while (cursor < ruby.reading.size()) {
                const Utf8Char letter = DecodeUtf8(ruby.reading, cursor);
                readingWidth +=
                    rubyFont.Advance(letter.code) * rubyShrink * settings.scaleX;
                cursor += letter.size;
            }

            double pen = left + ((right - left) - readingWidth) / 2.0;
            cursor = 0;
            while (cursor < ruby.reading.size()) {
                const Utf8Char letter = DecodeUtf8(ruby.reading, cursor);
                PlacedGlyph placed;
                placed.character = letter.code;
                placed.at = last.at;
                placed.line = first.line;
                placed.x = pen;
                placed.scaleX = settings.scaleX * rubyShrink;
                placed.scaleY = settings.scaleY * rubyShrink;
                placed.style = first.style;
                placed.ruby = true;
                layout.glyphs.push_back(placed);
                pen += rubyFont.Advance(letter.code) * rubyShrink * settings.scaleX;
                cursor += letter.size;
            }

            layout.lines[first.line].rubyHeight = rubyHeight;
            groupBegin = groupEnd + 1;
        }
    }

    double top = settings.y;
    for (LaidOutLine &row : layout.lines) {
        row.top = top;
        row.height = lineHeight + row.rubyHeight;
        top += row.height;
        layout.width = std::max(layout.width, row.width);
    }
    layout.height = top - settings.y;

    for (PlacedGlyph &glyph : layout.glyphs) {
        const LaidOutLine &row = layout.lines[glyph.line];
        if (glyph.ruby) {
            glyph.y = row.top;
            continue;
        }
        glyph.y = row.top + row.rubyHeight;
        if (glyph.style.halfSize) {
            glyph.y += lineHeight * kHalfSizeDrop;
        }
    }

    return layout;
}

} // namespace TellerEngine::Base
