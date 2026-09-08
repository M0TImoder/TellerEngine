#pragma once

// STRGチャンクの文字列テーブル
// 並びは u32の件数 / 件数ぶんのポインタ / 各実体
// 実体は u32の長さ / その長さぶんの本体 / NUL終端
// 他のチャンクからの文字列参照は、実体の先頭ではなく本体の先頭を指す

#include <Extract/Chunks.hpp>
#include <Extract/Reader.hpp>
#include <Extract/FileBytes.hpp>
#include <Base/Error.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace TellerEngine::Extract {

struct StringTable {
    // 出現順
    std::vector<std::string> strings;

    // 本体の先頭位置から添字を引く
    std::unordered_map<std::uint64_t, std::size_t> byPointer;

    const std::string *Find(std::uint64_t pointer) const {
        const auto found = byPointer.find(pointer);
        if (found == byPointer.end()) {
            return nullptr;
        }
        return &strings[found->second];
    }
};

inline Expected<StringTable, TellerEngine::Base::Error>
ReadStringTable(const TellerEngine::Extract::FileBytes &bytes, std::string_view name,
                const Chunk &chunk) {
    const auto contents = bytes.Read(name, chunk.offset, chunk.size);
    if (!contents) {
        return Unexpected<TellerEngine::Base::Error>(contents.error());
    }
    const Span<const std::byte> view(*contents);

    if (view.size() < 4) {
        return Unexpected<TellerEngine::Base::Error>(
            Malformed("STRG is smaller than its count field"));
    }

    const std::uint64_t count = ReadU32(view, 0);
    const std::uint64_t listEnd = 4 + count * 4;
    if (listEnd > view.size()) {
        return Unexpected<TellerEngine::Base::Error>(Malformed(
            "STRG declares " + std::to_string(count) + " strings but is only " +
            std::to_string(view.size()) + " bytes"));
    }

    StringTable table;
    table.strings.reserve(static_cast<std::size_t>(count));
    table.byPointer.reserve(static_cast<std::size_t>(count));

    for (std::uint64_t index = 0; index < count; ++index) {
        const std::uint64_t pointer =
            ReadU32(view, static_cast<std::size_t>(4 + index * 4));
        if (pointer < chunk.offset ||
            pointer - chunk.offset + 4 > view.size()) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed("STRG entry " + std::to_string(index) +
                          " points outside the chunk"));
        }

        const std::uint64_t local = pointer - chunk.offset;
        const std::uint64_t length =
            ReadU32(view, static_cast<std::size_t>(local));

        // 本体とNUL終端の分まで収まっている必要がある
        if (local + 4 + length + 1 > view.size()) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed("STRG entry " + std::to_string(index) +
                          " runs past the end of the chunk"));
        }

        const auto *first =
            reinterpret_cast<const char *>(view.data() + local + 4);
        table.byPointer.emplace(pointer + 4, table.strings.size());
        table.strings.emplace_back(first, static_cast<std::size_t>(length));
    }

    return table;
}

// 0は空文字列として扱い、それ以外で引けなければ壊れているとみなす
inline Expected<std::string, TellerEngine::Base::Error>
ResolveString(const StringTable &strings, std::uint64_t pointer,
              std::string_view field) {
    if (pointer == 0) {
        return std::string();
    }
    const auto *found = strings.Find(pointer);
    if (found == nullptr) {
        return Unexpected<TellerEngine::Base::Error>(
            Malformed(std::string(field) + " points to " +
                      std::to_string(pointer) + " which is not in STRG"));
    }
    return *found;
}

} // namespace TellerEngineEngine::Extract
