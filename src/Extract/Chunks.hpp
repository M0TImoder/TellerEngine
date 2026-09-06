#pragma once

// data.winのFORMを走査してチャンクの一覧を作る
// FORMは8バイトの見出しの並びで、中身の解釈はチャンクごとに異なる

#include <Extract/Reader.hpp>
#include <Extract/Bytes.hpp>
#include <Base/Error.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace TellerEngine::Extract {

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

inline TellerEngine::Base::Error Malformed(std::string context) {
    return TellerEngine::Base::Error{TellerEngine::Base::ErrorCode::Malformed, std::move(context)};
}

inline Expected<ChunkTable, TellerEngine::Base::Error>
ReadChunkTable(const TellerEngine::Extract::Bytes &bytes, std::string_view name) {
    const auto total = bytes.SizeOf(name);
    if (!total) {
        return Unexpected<TellerEngine::Base::Error>(total.error());
    }
    if (*total < ChunkHeaderSize) {
        return Unexpected<TellerEngine::Base::Error>(
            Malformed(std::string(name) + " is smaller than a header"));
    }

    const auto header = bytes.Read(name, 0, ChunkHeaderSize);
    if (!header) {
        return Unexpected<TellerEngine::Base::Error>(header.error());
    }
    const Span<const std::byte> headerView(*header);

    if (ReadMagic(headerView, 0) != "FORM") {
        return Unexpected<TellerEngine::Base::Error>(
            Malformed(std::string(name) + " does not begin with FORM"));
    }

    ChunkTable table;
    table.formSize = ReadU32(headerView, 4);
    if (table.formSize + ChunkHeaderSize != *total) {
        return Unexpected<TellerEngine::Base::Error>(Malformed(
            std::string(name) + " FORM size " + std::to_string(table.formSize) +
            " does not match file size " + std::to_string(*total)));
    }

    std::uint64_t cursor = ChunkHeaderSize;
    const std::uint64_t end = ChunkHeaderSize + table.formSize;
    while (cursor < end) {
        if (end - cursor < ChunkHeaderSize) {
            return Unexpected<TellerEngine::Base::Error>(Malformed(
                std::string(name) + " has a truncated chunk header at " +
                std::to_string(cursor)));
        }

        const auto entry = bytes.Read(name, cursor, ChunkHeaderSize);
        if (!entry) {
            return Unexpected<TellerEngine::Base::Error>(entry.error());
        }
        const Span<const std::byte> entryView(*entry);

        Chunk chunk;
        chunk.name = ReadMagic(entryView, 0);
        chunk.size = ReadU32(entryView, 4);
        chunk.offset = cursor + ChunkHeaderSize;

        if (chunk.size > end - chunk.offset) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed(chunk.name + " runs past the end of FORM"));
        }

        cursor = chunk.offset + chunk.size;
        table.chunks.push_back(std::move(chunk));
    }

    return table;
}

// チャンクの中身を丸ごと読む
inline Expected<TellerEngine::Extract::ByteBuffer, TellerEngine::Base::Error>
ReadChunk(const TellerEngine::Extract::Bytes &bytes, std::string_view name, const Chunk &chunk) {
    return bytes.Read(name, chunk.offset, chunk.size);
}

inline bool InChunk(const Chunk &chunk, std::uint64_t pointer,
                    std::uint64_t size) {
    return pointer >= chunk.offset &&
           pointer - chunk.offset + size <= chunk.size;
}

inline std::size_t LocalOffset(const Chunk &chunk, std::uint64_t pointer) {
    return static_cast<std::size_t>(pointer - chunk.offset);
}

// 件数+ポインタ列を読む
inline Expected<std::vector<std::uint64_t>, TellerEngine::Base::Error>
ReadPointerList(Span<const std::byte> view, const Chunk &chunk,
                std::string_view label) {
    if (view.size() < 4) {
        return Unexpected<TellerEngine::Base::Error>(
            Malformed(std::string(label) + " is smaller than its count field"));
    }

    const std::uint64_t count = ReadU32(view, 0);
    if (4 + count * 4 > view.size()) {
        return Unexpected<TellerEngine::Base::Error>(Malformed(
            std::string(label) + " declares " + std::to_string(count) +
            " entries but is only " + std::to_string(view.size()) + " bytes"));
    }

    std::vector<std::uint64_t> pointers;
    pointers.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t index = 0; index < count; ++index) {
        const std::uint64_t pointer =
            ReadU32(view, static_cast<std::size_t>(4 + index * 4));
        if (!InChunk(chunk, pointer, 0)) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed(std::string(label) + " entry " +
                          std::to_string(index) + " points outside the chunk"));
        }
        pointers.push_back(pointer);
    }
    return pointers;
}

} // namespace TellerEngineEngine::Extract
