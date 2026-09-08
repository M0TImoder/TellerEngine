#pragma once

// PATHチャンクの経路
// 固定20バイトのあとに、点が12バイトずつ並ぶ

#include <Extract/Chunks.hpp>
#include <Extract/Reader.hpp>
#include <Extract/Strings.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace TellerEngine::Extract {

inline constexpr std::uint64_t PathFixedSize = 20;
inline constexpr std::uint64_t PathPointSize = 12;

struct PathPoint {
    float x = 0.0f;
    float y = 0.0f;
    float speed = 0.0f;
};

struct Path {
    std::string name;
    bool smooth = false;
    bool closed = false;
    std::uint32_t precision = 0;
    std::vector<PathPoint> points;
};

struct PathTable {
    std::vector<Path> paths;
};

inline Expected<PathTable, TellerEngine::Base::Error> ReadPaths(const TellerEngine::Extract::FileBytes &bytes,
                                                  std::string_view name,
                                                  const Chunk &chunk,
                                                  const StringTable &strings) {
    const auto contents = ReadChunk(bytes, name, chunk);
    if (!contents) {
        return Unexpected<TellerEngine::Base::Error>(contents.error());
    }
    const Span<const std::byte> view(*contents);

    const auto pointers = ReadPointerList(view, chunk, "PATH");
    if (!pointers) {
        return Unexpected<TellerEngine::Base::Error>(pointers.error());
    }

    PathTable table;
    table.paths.reserve(pointers->size());

    for (const auto pointer : *pointers) {
        if (!InChunk(chunk, pointer, PathFixedSize)) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed("PATH entry runs past the end of the chunk"));
        }
        const auto at = LocalOffset(chunk, pointer);

        Path path;
        auto text = ResolveString(strings, ReadU32(view, at), "PATH name");
        if (!text) {
            return Unexpected<TellerEngine::Base::Error>(text.error());
        }
        path.name = std::move(*text);
        path.smooth = ReadU32(view, at + 4) != 0;
        path.closed = ReadU32(view, at + 8) != 0;
        path.precision = ReadU32(view, at + 12);

        const std::uint64_t count = ReadU32(view, at + 16);
        if (!InChunk(chunk, pointer, PathFixedSize + count * PathPointSize)) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed(path.name + " has " + std::to_string(count) +
                          " points which run past the end of the chunk"));
        }

        path.points.reserve(static_cast<std::size_t>(count));
        for (std::uint64_t index = 0; index < count; ++index) {
            const auto point = at + PathFixedSize + index * PathPointSize;
            path.points.push_back(
                {ReadF32(view, static_cast<std::size_t>(point)),
                 ReadF32(view, static_cast<std::size_t>(point + 4)),
                 ReadF32(view, static_cast<std::size_t>(point + 8))});
        }
        table.paths.push_back(std::move(path));
    }

    return table;
}

} // namespace TellerEngineEngine::Extract
