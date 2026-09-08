#pragma once

#include <Extract/Chunks.hpp>
#include <Extract/Reader.hpp>
#include <Extract/Strings.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace TellerEngine::Extract {

// 中身の並びがバージョンによって変わるチャンクは、生のまま持って落とさない
struct RawChunk {
    std::string name;
    ByteBuffer contents;
};

inline Expected<RawChunk, Base::Error>
ReadRawChunk(const FileBytes &bytes, std::string_view name,
             const Chunk &chunk) {
    auto contents = ReadChunk(bytes, name, chunk);
    if (!contents) {
        return Unexpected<Base::Error>(contents.error());
    }
    return RawChunk{chunk.name, std::move(*contents)};
}

struct Language {
    std::uint32_t unknown = 0;
    std::uint32_t languageCount = 0;
    std::uint32_t entryCount = 0;
    std::vector<std::string> entryIds;
};

inline Expected<Language, Base::Error>
ReadLanguage(const FileBytes &bytes, std::string_view name,
             const Chunk &chunk, const StringTable &strings) {
    const auto contents = ReadChunk(bytes, name, chunk);
    if (!contents) {
        return Unexpected<Base::Error>(contents.error());
    }
    const Span<const std::byte> view(*contents);

    if (view.size() < 12) {
        return Unexpected<Base::Error>(
            Malformed("LANG is smaller than its header"));
    }

    Language language;
    language.unknown = ReadU32(view, 0);
    language.languageCount = ReadU32(view, 4);
    language.entryCount = ReadU32(view, 8);

    if (12 + static_cast<std::uint64_t>(language.entryCount) * 4 >
        view.size()) {
        return Unexpected<Base::Error>(Malformed(
            "LANG declares " + std::to_string(language.entryCount) +
            " entries but is only " + std::to_string(view.size()) + " bytes"));
    }

    language.entryIds.reserve(language.entryCount);
    for (std::uint32_t index = 0; index < language.entryCount; ++index) {
        auto text =
            ResolveString(strings, ReadU32(view, 12 + index * 4), "LANG entry");
        if (!text) {
            return Unexpected<Base::Error>(text.error());
        }
        language.entryIds.push_back(std::move(*text));
    }
    return language;
}

// 件数だけを持つ一覧
struct CountedList {
    std::string name;
    std::uint64_t count = 0;
    std::vector<std::uint64_t> pointers;
};

inline Expected<CountedList, Base::Error>
ReadCountedList(const FileBytes &bytes, std::string_view name,
                const Chunk &chunk) {
    const auto contents = ReadChunk(bytes, name, chunk);
    if (!contents) {
        return Unexpected<Base::Error>(contents.error());
    }
    const Span<const std::byte> view(*contents);

    const auto pointers = ReadPointerList(view, chunk, chunk.name);
    if (!pointers) {
        return Unexpected<Base::Error>(pointers.error());
    }
    return CountedList{chunk.name, pointers->size(), *pointers};
}

} // namespace TellerEngine::Extract
