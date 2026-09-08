#pragma once

// FONTチャンクのフォント
// 固定44バイトのあとに、グリフへのポインタ列とグリフ本体が続く
// グリフは16バイト固定
// チャンクの末尾には512バイトの詰め物が付く

#include <Extract/Chunks.hpp>
#include <Extract/Reader.hpp>
#include <Extract/Strings.hpp>
#include <Extract/Textures.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace TellerEngine::Extract {

inline constexpr std::uint64_t FontFixedSize = 44;
inline constexpr std::uint64_t GlyphSize = 16;

struct Kerning {
    std::int16_t character = 0;
    std::int16_t shiftModifier = 0;
};

struct Glyph {
    std::uint16_t character = 0;
    std::uint16_t sourceX = 0;
    std::uint16_t sourceY = 0;
    std::uint16_t sourceWidth = 0;
    std::uint16_t sourceHeight = 0;
    std::int16_t shift = 0;
    std::int16_t offset = 0;
    std::vector<Kerning> kerning;
};

struct Font {
    std::string name;
    std::string displayName;
    // 最上位ビットが立っていると、残りは浮動小数として読む
    std::uint32_t emSize = 0;
    float emSizeFloat = 0.0f;
    bool emSizeIsFloat = false;

    bool bold = false;
    bool italic = false;
    std::uint16_t rangeStart = 0;
    std::uint8_t charset = 0;
    std::uint8_t antiAliasing = 0;
    std::uint32_t rangeEnd = 0;
    std::uint64_t texture = 0;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    std::vector<Glyph> glyphs;
};

struct FontTable {
    std::vector<Font> fonts;
};

inline Expected<FontTable, TellerEngine::Base::Error>
ReadFonts(const TellerEngine::Extract::FileBytes &bytes, std::string_view name,
          const Chunk &chunk, const StringTable &strings,
          const TextureRegionTable &regions) {
    const auto contents = ReadChunk(bytes, name, chunk);
    if (!contents) {
        return Unexpected<TellerEngine::Base::Error>(contents.error());
    }
    const Span<const std::byte> view(*contents);

    const auto pointers = ReadPointerList(view, chunk, "FONT");
    if (!pointers) {
        return Unexpected<TellerEngine::Base::Error>(pointers.error());
    }

    FontTable table;
    table.fonts.reserve(pointers->size());

    for (const auto pointer : *pointers) {
        if (!InChunk(chunk, pointer, FontFixedSize)) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed("FONT entry runs past the end of the chunk"));
        }
        const auto at = LocalOffset(chunk, pointer);

        Font font;
        auto text = ResolveString(strings, ReadU32(view, at), "FONT name");
        if (!text) {
            return Unexpected<TellerEngine::Base::Error>(text.error());
        }
        font.name = std::move(*text);

        auto display =
            ResolveString(strings, ReadU32(view, at + 4), "FONT displayName");
        if (!display) {
            return Unexpected<TellerEngine::Base::Error>(display.error());
        }
        font.displayName = std::move(*display);

        const std::uint32_t rawEmSize = ReadU32(view, at + 8);
        font.emSizeIsFloat = (rawEmSize & 0x80000000u) != 0;
        if (font.emSizeIsFloat) {
            font.emSizeFloat = ReadF32(view, at + 8);
        } else {
            font.emSize = rawEmSize;
        }
        font.bold = ReadU32(view, at + 12) != 0;
        font.italic = ReadU32(view, at + 16) != 0;
        font.rangeStart = ReadU16(view, at + 20);
        font.charset = static_cast<std::uint8_t>(view[at + 22]);
        font.antiAliasing = static_cast<std::uint8_t>(view[at + 23]);
        font.rangeEnd = ReadU32(view, at + 24);
        font.texture = ReadU32(view, at + 28);
        font.scaleX = ReadF32(view, at + 32);
        font.scaleY = ReadF32(view, at + 36);

        if (font.texture != 0 && regions.Find(font.texture) == nullptr) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed(font.name +
                          " points to a texture region which is not in TPAG"));
        }

        const std::uint64_t count = ReadU32(view, at + 40);
        if (!InChunk(chunk, pointer, FontFixedSize + count * 4)) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed(font.name + " declares " + std::to_string(count) +
                          " glyphs which run past the end of the chunk"));
        }

        font.glyphs.reserve(static_cast<std::size_t>(count));
        for (std::uint64_t index = 0; index < count; ++index) {
            const std::uint64_t glyphPointer = ReadU32(
                view, static_cast<std::size_t>(at + FontFixedSize + index * 4));
            if (!InChunk(chunk, glyphPointer, GlyphSize)) {
                return Unexpected<TellerEngine::Base::Error>(
                    Malformed(font.name + " glyph " + std::to_string(index) +
                              " points outside the chunk"));
            }
            const auto glyphAt = LocalOffset(chunk, glyphPointer);

            Glyph glyph;
            glyph.character = ReadU16(view, glyphAt + 0);
            glyph.sourceX = ReadU16(view, glyphAt + 2);
            glyph.sourceY = ReadU16(view, glyphAt + 4);
            glyph.sourceWidth = ReadU16(view, glyphAt + 6);
            glyph.sourceHeight = ReadU16(view, glyphAt + 8);
            glyph.shift = ReadI16(view, glyphAt + 10);
            glyph.offset = ReadI16(view, glyphAt + 12);

            // カーニングは件数がu16の一覧
            const std::uint64_t kerningCount = ReadU16(view, glyphAt + 14);
            if (!InChunk(chunk, glyphPointer, GlyphSize + kerningCount * 4)) {
                return Unexpected<TellerEngine::Base::Error>(
                    Malformed(font.name +
                              " glyph kerning runs past the end of the chunk"));
            }
            glyph.kerning.reserve(static_cast<std::size_t>(kerningCount));
            for (std::uint64_t pair = 0; pair < kerningCount; ++pair) {
                const auto kerningAt =
                    static_cast<std::size_t>(glyphAt + GlyphSize + pair * 4);
                glyph.kerning.push_back(
                    {ReadI16(view, kerningAt), ReadI16(view, kerningAt + 2)});
            }

            font.glyphs.push_back(std::move(glyph));
        }

        table.fonts.push_back(std::move(font));
    }

    return table;
}

} // namespace TellerEngine::Extract
