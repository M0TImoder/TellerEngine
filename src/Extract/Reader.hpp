#pragma once

// data.winを読むための最小限の取り出し
// GameMakerのデータはリトルエンディアン

#include <Base/Compat.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>

namespace TellerEngine::Extract {

// 呼び出し側がoffset + 4以上の大きさを保証する
inline std::uint32_t ReadU32(Span<const std::byte> bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset + 0]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

inline std::uint16_t ReadU16(Span<const std::byte> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(bytes[offset + 0]) |
        (static_cast<std::uint16_t>(bytes[offset + 1]) << 8));
}

inline std::uint64_t ReadU64(Span<const std::byte> bytes, std::size_t offset) {
    return static_cast<std::uint64_t>(ReadU32(bytes, offset)) |
           (static_cast<std::uint64_t>(ReadU32(bytes, offset + 4)) << 32);
}

inline float ReadF32(Span<const std::byte> bytes, std::size_t offset) {
    return std::bit_cast<float>(ReadU32(bytes, offset));
}

inline std::int32_t ReadI32(Span<const std::byte> bytes, std::size_t offset) {
    return static_cast<std::int32_t>(ReadU32(bytes, offset));
}

inline std::int16_t ReadI16(Span<const std::byte> bytes, std::size_t offset) {
    return static_cast<std::int16_t>(ReadU16(bytes, offset));
}

// PNGの中だけはビッグエンディアン
inline std::uint32_t ReadU32Be(Span<const std::byte> bytes,
                               std::size_t offset) {
    return (static_cast<std::uint32_t>(bytes[offset + 0]) << 24) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
           static_cast<std::uint32_t>(bytes[offset + 3]);
}

// チャンク名などの4文字識別子
inline std::string ReadMagic(Span<const std::byte> bytes, std::size_t offset) {
    std::string magic(4, '\0');
    for (std::size_t index = 0; index < 4; ++index) {
        magic[index] = static_cast<char>(bytes[offset + index]);
    }
    return magic;
}

} // namespace TellerEngineEngine::Extract
