#pragma once

// SCPTチャンクのスクリプト
// 実体は8バイト固定で、名前とコードの番号を持つ

#include <Extract/Chunks.hpp>
#include <Extract/Reader.hpp>
#include <Extract/Strings.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace TellerEngine::Extract {

inline constexpr std::uint64_t ScriptSize = 8;

struct Script {
    std::string name;
    std::int32_t code = -1;
};

struct ScriptTable {
    std::vector<Script> scripts;
};

inline Expected<ScriptTable, TellerEngine::Base::Error>
ReadScripts(const TellerEngine::Extract::FileBytes &bytes, std::string_view name, const Chunk &chunk,
            const StringTable &strings) {
    const auto contents = ReadChunk(bytes, name, chunk);
    if (!contents) {
        return Unexpected<TellerEngine::Base::Error>(contents.error());
    }
    const Span<const std::byte> view(*contents);

    const auto pointers = ReadPointerList(view, chunk, "SCPT");
    if (!pointers) {
        return Unexpected<TellerEngine::Base::Error>(pointers.error());
    }

    ScriptTable table;
    table.scripts.reserve(pointers->size());

    for (const auto pointer : *pointers) {
        if (!InChunk(chunk, pointer, ScriptSize)) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed("SCPT entry runs past the end of the chunk"));
        }
        const auto at = LocalOffset(chunk, pointer);

        Script script;
        auto text = ResolveString(strings, ReadU32(view, at), "SCPT name");
        if (!text) {
            return Unexpected<TellerEngine::Base::Error>(text.error());
        }
        script.name = std::move(*text);
        script.code = ReadI32(view, at + 4);
        table.scripts.push_back(std::move(script));
    }

    return table;
}

} // namespace TellerEngineEngine::Extract
