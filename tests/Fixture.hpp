#pragma once

// テスト用の検証データを組み立てる

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace TellerTest {

inline void AppendU32(std::string &out, std::uint32_t value) {
    out.push_back(static_cast<char>(value & 0xff));
    out.push_back(static_cast<char>((value >> 8) & 0xff));
    out.push_back(static_cast<char>((value >> 16) & 0xff));
    out.push_back(static_cast<char>((value >> 24) & 0xff));
}

inline void AppendU32Be(std::string &out, std::uint32_t value) {
    out.push_back(static_cast<char>((value >> 24) & 0xff));
    out.push_back(static_cast<char>((value >> 16) & 0xff));
    out.push_back(static_cast<char>((value >> 8) & 0xff));
    out.push_back(static_cast<char>(value & 0xff));
}

inline void AppendU64(std::string &out, std::uint64_t value) {
    AppendU32(out, static_cast<std::uint32_t>(value & 0xffffffff));
    AppendU32(out, static_cast<std::uint32_t>(value >> 32));
}

// FORMの見出しは8バイト
// 先頭のチャンクの中身は16バイト目から始まる
inline constexpr std::uint32_t FirstChunkOffset = 16;

inline std::string
MakeForm(const std::vector<std::pair<std::string, std::string>> &chunks) {
    std::string body;
    for (const auto &[name, contents] : chunks) {
        body += name;
        AppendU32(body, static_cast<std::uint32_t>(contents.size()));
        body += contents;
    }

    std::string form = "FORM";
    AppendU32(form, static_cast<std::uint32_t>(body.size()));
    form += body;
    return form;
}

// baseはチャンクの中身がファイルのどこから始まるか
inline std::string MakeStrg(const std::vector<std::string> &values,
                            std::uint32_t base) {
    std::string entries;
    std::string pointers;
    const std::uint32_t entriesBase =
        base + 4 + static_cast<std::uint32_t>(values.size()) * 4;

    for (const auto &value : values) {
        AppendU32(pointers,
                  entriesBase + static_cast<std::uint32_t>(entries.size()));
        AppendU32(entries, static_cast<std::uint32_t>(value.size()));
        entries += value;
        entries.push_back('\0');
    }

    std::string chunk;
    AppendU32(chunk, static_cast<std::uint32_t>(values.size()));
    chunk += pointers;
    chunk += entries;
    return chunk;
}

// STRG内の何番目の文字列が、他チャンクからどのポインタで参照されるか
inline std::uint32_t StrgPointer(const std::vector<std::string> &values,
                                 std::size_t index, std::uint32_t base) {
    std::uint32_t at = base + 4 + static_cast<std::uint32_t>(values.size()) * 4;
    for (std::size_t i = 0; i < index; ++i) {
        at += static_cast<std::uint32_t>(4 + values[i].size() + 1);
    }
    return at + 4;
}

// TPAGの実体は22バイト
inline std::string
MakeTpag(const std::vector<std::vector<std::uint16_t>> &entries,
         std::uint32_t base) {
    const auto count = static_cast<std::uint32_t>(entries.size());
    const std::uint32_t entriesBase = base + 4 + count * 4;

    std::string pointers;
    std::string bodies;
    for (std::uint32_t index = 0; index < count; ++index) {
        AppendU32(pointers, entriesBase + index * 22);
        for (const auto field : entries[index]) {
            bodies.push_back(static_cast<char>(field & 0xff));
            bodies.push_back(static_cast<char>((field >> 8) & 0xff));
        }
    }

    std::string chunk;
    AppendU32(chunk, count);
    chunk += pointers;
    chunk += bodies;
    return chunk;
}

// TPAG内の何番目の矩形が、他チャンクからどのポインタで参照されるか
inline std::uint32_t TpagPointer(std::size_t count, std::size_t index,
                                 std::uint32_t base) {
    return base + 4 + static_cast<std::uint32_t>(count) * 4 +
           static_cast<std::uint32_t>(index) * 22;
}

// FORMに並べたときの、各チャンクの中身の開始位置
inline std::uint32_t
ChunkOffset(const std::vector<std::pair<std::string, std::string>> &chunks,
            std::size_t index) {
    std::uint32_t at = FirstChunkOffset;
    for (std::size_t i = 0; i < index; ++i) {
        at += static_cast<std::uint32_t>(chunks[i].second.size()) + 8;
    }
    return at;
}

inline std::filesystem::path WriteSample(const std::string &directoryName,
                                         const std::string &fileName,
                                         const std::string &contents) {
    const auto directory =
        std::filesystem::temp_directory_path() / directoryName;
    std::filesystem::create_directories(directory);
    std::ofstream stream(directory / fileName, std::ios::binary);
    stream.write(contents.data(),
                 static_cast<std::streamsize>(contents.size()));
    return directory;
}

} // namespace TellerEngineTest
