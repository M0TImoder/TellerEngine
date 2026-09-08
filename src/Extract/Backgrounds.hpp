#pragma once

// BGNDチャンクの背景
// 実体は20バイト固定で、テクスチャ矩形をひとつ指す

#include <Extract/Chunks.hpp>
#include <Extract/Reader.hpp>
#include <Extract/Strings.hpp>
#include <Extract/Textures.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace TellerEngine::Extract {

inline constexpr std::uint64_t BackgroundSize = 20;

struct Background {
    std::string name;
    bool transparent = false;
    bool smooth = false;
    bool preload = false;
    std::uint64_t texture = 0;
};

struct BackgroundTable {
    std::vector<Background> backgrounds;
};

inline Expected<BackgroundTable, TellerEngine::Base::Error>
ReadBackgrounds(const TellerEngine::Extract::FileBytes &bytes, std::string_view name,
                const Chunk &chunk, const StringTable &strings,
                const TextureRegionTable &regions) {
    const auto contents = ReadChunk(bytes, name, chunk);
    if (!contents) {
        return Unexpected<TellerEngine::Base::Error>(contents.error());
    }
    const Span<const std::byte> view(*contents);

    const auto pointers = ReadPointerList(view, chunk, "BGND");
    if (!pointers) {
        return Unexpected<TellerEngine::Base::Error>(pointers.error());
    }

    BackgroundTable table;
    table.backgrounds.reserve(pointers->size());

    for (const auto pointer : *pointers) {
        if (!InChunk(chunk, pointer, BackgroundSize)) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed("BGND entry runs past the end of the chunk"));
        }
        const auto at = LocalOffset(chunk, pointer);

        Background background;
        auto text = ResolveString(strings, ReadU32(view, at), "BGND name");
        if (!text) {
            return Unexpected<TellerEngine::Base::Error>(text.error());
        }
        background.name = std::move(*text);
        background.transparent = ReadU32(view, at + 4) != 0;
        background.smooth = ReadU32(view, at + 8) != 0;
        background.preload = ReadU32(view, at + 12) != 0;
        background.texture = ReadU32(view, at + 16);

        if (background.texture != 0 &&
            regions.Find(background.texture) == nullptr) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed(background.name +
                          " points to a texture region which is not in TPAG"));
        }
        table.backgrounds.push_back(std::move(background));
    }

    return table;
}

} // namespace TellerEngineEngine::Extract
