#pragma once

// data.winのFORMを走査してチャンクの一覧を作る
// FORMは8バイトの見出しの並びで、中身の解釈はチャンクごとに異なる

#include <Extract/Reader.hpp>
#include <Teller/Data/Bytes.hpp>
#include <Teller/Data/Error.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Teller::Extract {

// 見出しの大きさ
inline constexpr std::uint64_t ChunkHeaderSize = 8;

struct Chunk {
    std::string name;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
};

struct ChunkTable {
    std::uint64_t formSize = 0;
    std::vector<Chunk> chunks;

    const Chunk *Find(std::string_view name) const {
        for (const auto &chunk : chunks) {
            if (chunk.name == name) {
                return &chunk;
            }
        }
        return nullptr;
    }
};

inline Data::Error Malformed(std::string context) {
    return Data::Error{Data::ErrorCode::Malformed, std::move(context)};
}

inline Expected<ChunkTable, Data::Error>
ReadChunkTable(const Data::Bytes &bytes, std::string_view name) {
    const auto total = bytes.SizeOf(name);
    if (!total) {
        return Unexpected<Data::Error>(total.error());
    }
    if (*total < ChunkHeaderSize) {
        return Unexpected<Data::Error>(
            Malformed(std::string(name) + " is smaller than a header"));
    }

    const auto header = bytes.Read(name, 0, ChunkHeaderSize);
    if (!header) {
        return Unexpected<Data::Error>(header.error());
    }
    const Span<const std::byte> headerView(*header);

    if (ReadMagic(headerView, 0) != "FORM") {
        return Unexpected<Data::Error>(
            Malformed(std::string(name) + " does not begin with FORM"));
    }

    ChunkTable table;
    table.formSize = ReadU32(headerView, 4);
    if (table.formSize + ChunkHeaderSize != *total) {
        return Unexpected<Data::Error>(Malformed(
            std::string(name) + " FORM size " + std::to_string(table.formSize) +
            " does not match file size " + std::to_string(*total)));
    }

    std::uint64_t cursor = ChunkHeaderSize;
    const std::uint64_t end = ChunkHeaderSize + table.formSize;
    while (cursor < end) {
        if (end - cursor < ChunkHeaderSize) {
            return Unexpected<Data::Error>(Malformed(
                std::string(name) + " has a truncated chunk header at " +
                std::to_string(cursor)));
        }

        const auto entry = bytes.Read(name, cursor, ChunkHeaderSize);
        if (!entry) {
            return Unexpected<Data::Error>(entry.error());
        }
        const Span<const std::byte> entryView(*entry);

        Chunk chunk;
        chunk.name = ReadMagic(entryView, 0);
        chunk.size = ReadU32(entryView, 4);
        chunk.offset = cursor + ChunkHeaderSize;

        if (chunk.size > end - chunk.offset) {
            return Unexpected<Data::Error>(
                Malformed(chunk.name + " runs past the end of FORM"));
        }

        cursor = chunk.offset + chunk.size;
        table.chunks.push_back(std::move(chunk));
    }

    return table;
}

} // namespace Teller::Extract
