#pragma once

// data.winを読むための最小限の取り出し
// GameMakerのデータはリトルエンディアン

#include <Teller/Compat.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

namespace Teller::Extract {

// 呼び出し側がoffset + 4以上の大きさを保証する
inline std::uint32_t ReadU32(Span<const std::byte> bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset + 0]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

// チャンク名などの4文字識別子
inline std::string ReadMagic(Span<const std::byte> bytes, std::size_t offset) {
    std::string magic(4, '\0');
    for (std::size_t index = 0; index < 4; ++index) {
        magic[index] = static_cast<char>(bytes[offset + index]);
    }
    return magic;
}

} // namespace Teller::Extract
