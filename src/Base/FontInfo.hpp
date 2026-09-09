#pragma once

#include <Base/Compat.hpp>
#include <Base/Draw.hpp>
#include <Base/Error.hpp>
#include <Base/Files.hpp>
#include <Base/Utf8.hpp>

#include <toml++/toml.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace TellerEngine::Base {

// フォントの絵の中の1文字
struct Glyph {
    char32_t character = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    // 次の文字までの送り
    int shift = 0;

    // 描き始めの横のずれ
    int offset = 0;
};

struct FontInfo {
    std::string displayName;
    int emSize = 0;

    // 0なら字から求める
    int height = 0;

    std::vector<Glyph> glyphs;

    const Glyph *Find(char32_t character) const {
        const auto found = std::lower_bound(
            glyphs.begin(), glyphs.end(), character,
            [](const Glyph &glyph, char32_t value) { return glyph.character < value; });
        if (found == glyphs.end() || found->character != character) {
            return nullptr;
        }
        return &*found;
    }

    // 次の文字までの送り
    double Advance(char32_t character) const {
        const Glyph *glyph = Find(character);
        return glyph == nullptr ? 0.0 : glyph->shift;
    }

    // 制御を含まない素の幅
    double Measure(std::string_view text) const {
        double width = 0.0;
        std::size_t position = 0;
        while (position < text.size()) {
            const Utf8Char letter = DecodeUtf8(text, position);
            width += Advance(letter.code);
            position += letter.size;
        }
        return width;
    }

    int Height() const {
        if (height > 0) {
            return height;
        }
        int found = 0;
        for (const Glyph &glyph : glyphs) {
            found = std::max(found, glyph.height);
        }
        return found;
    }
};

inline Expected<FontInfo, Error> ReadFontInfo(const std::filesystem::path &path) {
    const auto contents = Files::ReadText(path);
    if (!contents) {
        return Unexpected<Error>(contents.error());
    }

    toml::parse_result parsed = toml::parse(*contents);
    if (!parsed) {
        return Unexpected<Error>(
            Error{ErrorCode::Malformed,
                  path.string() + ": " + std::string(parsed.error().description())});
    }

    const toml::table &table = parsed.table();
    FontInfo info;
    info.displayName = table["display_name"].value_or(std::string{});
    info.emSize = static_cast<int>(table["em_size"].value_or(0));

    if (const toml::array *list = table["glyph"].as_array()) {
        info.glyphs.reserve(list->size());
        for (const toml::node &node : *list) {
            const toml::table *entry = node.as_table();
            if (entry == nullptr) {
                continue;
            }
            Glyph glyph;
            glyph.character =
                static_cast<char32_t>((*entry)["character"].value_or(std::int64_t{0}));
            glyph.x = static_cast<int>((*entry)["x"].value_or(0));
            glyph.y = static_cast<int>((*entry)["y"].value_or(0));
            glyph.width = static_cast<int>((*entry)["width"].value_or(0));
            glyph.height = static_cast<int>((*entry)["height"].value_or(0));
            glyph.shift = static_cast<int>((*entry)["shift"].value_or(0));
            glyph.offset = static_cast<int>((*entry)["offset"].value_or(0));
            info.glyphs.push_back(glyph);
        }
    }

    std::sort(info.glyphs.begin(), info.glyphs.end(),
              [](const Glyph &left, const Glyph &right) {
                  return left.character < right.character;
              });
    info.height = info.Height();
    return info;
}

// 字の形とそれが載っている絵
struct FontFace {
    const FontInfo *info = nullptr;
    ImageId image = ImageId::None;

    explicit operator bool() const { return info != nullptr; }
};

} // namespace TellerEngine::Base
