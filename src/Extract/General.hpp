#pragma once

// GEN8チャンクのゲーム設定
// 固定部128バイトのあとに、ルームの並び順が続く
// 文字列はSTRGの本体を指すポインタとして入っている

#include <Extract/FileBytes.hpp>
#include <Base/Error.hpp>
#include <Extract/Chunks.hpp>
#include <Extract/Reader.hpp>
#include <Extract/Strings.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace TellerEngine::Extract {

inline constexpr std::uint64_t GeneralFixedSize = 128;

struct GeneralInfo {
    bool debuggerDisabled = false;
    std::uint8_t bytecodeVersion = 0;
    std::uint16_t padding = 0;

    std::string filename;
    std::string config;
    std::string name;
    std::string displayName;

    // 16バイトずつの塊もそのまま持つ
    std::array<std::byte, 16> guid{};
    std::array<std::byte, 16> licenseMd5{};

    std::uint32_t lastObject = 0;
    std::uint32_t lastTile = 0;
    std::uint32_t gameId = 0;

    std::uint32_t major = 0;
    std::uint32_t minor = 0;
    std::uint32_t release = 0;
    std::uint32_t build = 0;

    std::uint32_t defaultWindowWidth = 0;
    std::uint32_t defaultWindowHeight = 0;
    std::uint32_t info = 0;

    std::uint32_t licenseCrc32 = 0;
    std::uint64_t timestamp = 0;
    std::uint64_t activeTargets = 0;
    std::uint64_t functionClassifications = 0;
    std::int32_t steamAppId = 0;
    std::uint32_t debuggerPort = 0;

    std::vector<std::uint32_t> roomOrder;
};

inline Expected<GeneralInfo, TellerEngine::Base::Error>
ReadGeneralInfo(const TellerEngine::Extract::FileBytes &bytes,
                std::string_view name, const Chunk &chunk,
                const StringTable &strings) {
    if (chunk.size < GeneralFixedSize + 4) {
        return Unexpected<TellerEngine::Base::Error>(
            Malformed("GEN8 is only " + std::to_string(chunk.size) + " bytes"));
    }

    const auto fixed = bytes.Read(name, chunk.offset, GeneralFixedSize + 4);
    if (!fixed) {
        return Unexpected<TellerEngine::Base::Error>(fixed.error());
    }
    const Span<const std::byte> view(*fixed);

    GeneralInfo general;
    general.debuggerDisabled = view[0] != std::byte{0};
    general.bytecodeVersion = static_cast<std::uint8_t>(view[1]);
    general.padding = ReadU16(view, 2);

    struct StringField {
        std::string *target;
        std::size_t at;
        const char *label;
    };
    const StringField stringFields[] = {
        {&general.filename, 4, "GEN8 filename"},
        {&general.config, 8, "GEN8 config"},
        {&general.name, 40, "GEN8 name"},
        {&general.displayName, 100, "GEN8 displayName"},
    };
    for (const auto &field : stringFields) {
        auto text =
            ResolveString(strings, ReadU32(view, field.at), field.label);
        if (!text) {
            return Unexpected<TellerEngine::Base::Error>(text.error());
        }
        *field.target = std::move(*text);
    }

    for (std::size_t index = 0; index < 16; ++index) {
        general.guid[index] = view[24 + index];
        general.licenseMd5[index] = view[72 + index];
    }

    general.lastObject = ReadU32(view, 12);
    general.lastTile = ReadU32(view, 16);
    general.gameId = ReadU32(view, 20);
    general.major = ReadU32(view, 44);
    general.minor = ReadU32(view, 48);
    general.release = ReadU32(view, 52);
    general.build = ReadU32(view, 56);
    general.defaultWindowWidth = ReadU32(view, 60);
    general.defaultWindowHeight = ReadU32(view, 64);
    general.info = ReadU32(view, 68);
    general.licenseCrc32 = ReadU32(view, 88);
    general.timestamp = ReadU64(view, 92);
    general.activeTargets = ReadU64(view, 104);
    general.functionClassifications = ReadU64(view, 112);
    general.steamAppId = static_cast<std::int32_t>(ReadU32(view, 120));
    general.debuggerPort = ReadU32(view, 124);

    const std::uint64_t count = ReadU32(view, GeneralFixedSize);
    if (GeneralFixedSize + 4 + count * 4 != chunk.size) {
        return Unexpected<TellerEngine::Base::Error>(
            Malformed("GEN8 declares " + std::to_string(count) +
                      " rooms in its order but is " +
                      std::to_string(chunk.size) + " bytes"));
    }

    const auto order =
        bytes.Read(name, chunk.offset + GeneralFixedSize + 4, count * 4);
    if (!order) {
        return Unexpected<TellerEngine::Base::Error>(order.error());
    }
    const Span<const std::byte> orderView(*order);

    general.roomOrder.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t index = 0; index < count; ++index) {
        general.roomOrder.push_back(
            ReadU32(orderView, static_cast<std::size_t>(index * 4)));
    }

    return general;
}

} // namespace TellerEngine::Extract
