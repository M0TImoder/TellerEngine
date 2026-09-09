#pragma once

#include <cstdint>
#include <string_view>

namespace TellerEngine::Base {

struct Utf8Char {
    char32_t code = 0;

    // この文字が占めるバイト数
    std::uint32_t size = 0;
};

// 壊れた並びは1バイト分の置換文字として返す
inline constexpr Utf8Char DecodeUtf8(std::string_view text, std::size_t position) {
    if (position >= text.size()) {
        return Utf8Char{};
    }

    const auto first = static_cast<unsigned char>(text[position]);
    if (first < 0x80) {
        return Utf8Char{first, 1};
    }

    std::uint32_t length = 0;
    char32_t code = 0;
    if ((first & 0xE0) == 0xC0) {
        length = 2;
        code = first & 0x1F;
    } else if ((first & 0xF0) == 0xE0) {
        length = 3;
        code = first & 0x0F;
    } else if ((first & 0xF8) == 0xF0) {
        length = 4;
        code = first & 0x07;
    } else {
        return Utf8Char{0xFFFD, 1};
    }

    if (position + length > text.size()) {
        return Utf8Char{0xFFFD, 1};
    }
    for (std::uint32_t i = 1; i < length; ++i) {
        const auto next = static_cast<unsigned char>(text[position + i]);
        if ((next & 0xC0) != 0x80) {
            return Utf8Char{0xFFFD, 1};
        }
        code = (code << 6) | (next & 0x3F);
    }
    return Utf8Char{code, length};
}

inline constexpr std::size_t CountUtf8(std::string_view text) {
    std::size_t count = 0;
    std::size_t position = 0;
    while (position < text.size()) {
        position += DecodeUtf8(text, position).size;
        count += 1;
    }
    return count;
}

} // namespace TellerEngine::Base
