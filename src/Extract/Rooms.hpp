#pragma once

// ROOMチャンクのルーム
// 固定88バイトのあとに、背景・ビュー・インスタンス・タイルの4つのリストが続く
// 各リストは 件数 + ポインタ列 + 実体

#include <Extract/Chunks.hpp>
#include <Extract/Reader.hpp>
#include <Extract/Strings.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace TellerEngine::Extract {

inline constexpr std::uint64_t RoomFixedSize = 88;
inline constexpr std::uint64_t RoomBackgroundSize = 40;
inline constexpr std::uint64_t RoomViewSize = 56;
inline constexpr std::uint64_t RoomInstanceSize = 40;
inline constexpr std::uint64_t RoomTileSize = 48;

struct RoomBackground {
    bool enabled = false;
    bool foreground = false;
    std::int32_t background = -1;
    std::int32_t x = 0;
    std::int32_t y = 0;
    bool tileX = false;
    bool tileY = false;
    std::int32_t speedX = 0;
    std::int32_t speedY = 0;
    bool stretch = false;
};

struct RoomView {
    bool enabled = false;
    std::int32_t viewX = 0;
    std::int32_t viewY = 0;
    std::int32_t viewWidth = 0;
    std::int32_t viewHeight = 0;
    std::int32_t portX = 0;
    std::int32_t portY = 0;
    std::int32_t portWidth = 0;
    std::int32_t portHeight = 0;
    std::int32_t borderX = 0;
    std::int32_t borderY = 0;
    std::int32_t speedX = 0;
    std::int32_t speedY = 0;
    std::int32_t follows = -1;
};

struct RoomInstance {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t object = -1;
    std::uint32_t id = 0;
    std::int32_t creationCode = -1;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    std::uint32_t color = 0;
    float rotation = 0.0f;
    std::int32_t preCreateCode = -1;
};

struct RoomTile {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t background = -1;
    std::uint32_t sourceX = 0;
    std::uint32_t sourceY = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::int32_t depth = 0;
    std::uint32_t id = 0;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    std::uint32_t color = 0;
};

struct Room {
    std::string name;
    std::string caption;
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    // 論理レートの初期値
    std::uint32_t speed = 30;

    bool persistent = false;
    std::uint32_t backgroundColor = 0;
    bool drawBackgroundColor = false;
    std::int32_t creationCode = -1;
    std::uint32_t flags = 0;

    std::vector<RoomBackground> backgrounds;
    std::vector<RoomView> views;
    std::vector<RoomInstance> instances;
    std::vector<RoomTile> tiles;
};

struct RoomTable {
    std::vector<Room> rooms;
};

namespace Detail {

// 件数 + ポインタ列 + 固定長の実体、という並びを読む
template <typename T, typename Read>
inline Expected<std::vector<T>, TellerEngine::Base::Error>
ReadRoomList(Span<const std::byte> view, const Chunk &chunk,
             std::uint64_t listPointer, std::uint64_t entrySize,
             std::string_view label, Read read) {
    if (!InChunk(chunk, listPointer, 4)) {
        return Unexpected<TellerEngine::Base::Error>(
            Malformed(std::string(label) + " points outside the chunk"));
    }
    const auto listAt = LocalOffset(chunk, listPointer);
    const std::uint64_t count = ReadU32(view, listAt);
    if (!InChunk(chunk, listPointer, 4 + count * 4)) {
        return Unexpected<TellerEngine::Base::Error>(
            Malformed(std::string(label) + " runs past the end of the chunk"));
    }

    std::vector<T> entries;
    entries.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t index = 0; index < count; ++index) {
        const std::uint64_t pointer =
            ReadU32(view, static_cast<std::size_t>(listAt + 4 + index * 4));
        if (!InChunk(chunk, pointer, entrySize)) {
            return Unexpected<TellerEngine::Base::Error>(Malformed(
                std::string(label) + " entry points outside the chunk"));
        }
        entries.push_back(read(view, LocalOffset(chunk, pointer)));
    }
    return entries;
}

} // namespace Detail

inline Expected<RoomTable, TellerEngine::Base::Error> ReadRooms(const TellerEngine::Extract::Bytes &bytes,
                                                  std::string_view name,
                                                  const Chunk &chunk,
                                                  const StringTable &strings) {
    const auto contents = ReadChunk(bytes, name, chunk);
    if (!contents) {
        return Unexpected<TellerEngine::Base::Error>(contents.error());
    }
    const Span<const std::byte> view(*contents);

    const auto pointers = ReadPointerList(view, chunk, "ROOM");
    if (!pointers) {
        return Unexpected<TellerEngine::Base::Error>(pointers.error());
    }

    RoomTable table;
    table.rooms.reserve(pointers->size());

    for (const auto pointer : *pointers) {
        if (!InChunk(chunk, pointer, RoomFixedSize)) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed("ROOM entry runs past the end of the chunk"));
        }
        const auto at = LocalOffset(chunk, pointer);

        Room room;
        auto text = ResolveString(strings, ReadU32(view, at), "ROOM name");
        if (!text) {
            return Unexpected<TellerEngine::Base::Error>(text.error());
        }
        room.name = std::move(*text);

        auto caption =
            ResolveString(strings, ReadU32(view, at + 4), "ROOM caption");
        if (!caption) {
            return Unexpected<TellerEngine::Base::Error>(caption.error());
        }
        room.caption = std::move(*caption);

        room.width = ReadU32(view, at + 8);
        room.height = ReadU32(view, at + 12);
        room.speed = ReadU32(view, at + 16);
        room.persistent = ReadU32(view, at + 20) != 0;
        room.backgroundColor = ReadU32(view, at + 24);
        room.drawBackgroundColor = ReadU32(view, at + 28) != 0;
        room.creationCode = ReadI32(view, at + 32);
        room.flags = ReadU32(view, at + 36);

        auto backgrounds = Detail::ReadRoomList<RoomBackground>(
            view, chunk, ReadU32(view, at + 40), RoomBackgroundSize,
            "ROOM backgrounds",
            [](Span<const std::byte> body, std::size_t entry) {
                RoomBackground value;
                value.enabled = ReadU32(body, entry) != 0;
                value.foreground = ReadU32(body, entry + 4) != 0;
                value.background = ReadI32(body, entry + 8);
                value.x = ReadI32(body, entry + 12);
                value.y = ReadI32(body, entry + 16);
                value.tileX = ReadU32(body, entry + 20) != 0;
                value.tileY = ReadU32(body, entry + 24) != 0;
                value.speedX = ReadI32(body, entry + 28);
                value.speedY = ReadI32(body, entry + 32);
                value.stretch = ReadU32(body, entry + 36) != 0;
                return value;
            });
        if (!backgrounds) {
            return Unexpected<TellerEngine::Base::Error>(backgrounds.error());
        }
        room.backgrounds = std::move(*backgrounds);

        auto views = Detail::ReadRoomList<RoomView>(
            view, chunk, ReadU32(view, at + 44), RoomViewSize, "ROOM views",
            [](Span<const std::byte> body, std::size_t entry) {
                RoomView value;
                value.enabled = ReadU32(body, entry) != 0;
                value.viewX = ReadI32(body, entry + 4);
                value.viewY = ReadI32(body, entry + 8);
                value.viewWidth = ReadI32(body, entry + 12);
                value.viewHeight = ReadI32(body, entry + 16);
                value.portX = ReadI32(body, entry + 20);
                value.portY = ReadI32(body, entry + 24);
                value.portWidth = ReadI32(body, entry + 28);
                value.portHeight = ReadI32(body, entry + 32);
                value.borderX = ReadI32(body, entry + 36);
                value.borderY = ReadI32(body, entry + 40);
                value.speedX = ReadI32(body, entry + 44);
                value.speedY = ReadI32(body, entry + 48);
                value.follows = ReadI32(body, entry + 52);
                return value;
            });
        if (!views) {
            return Unexpected<TellerEngine::Base::Error>(views.error());
        }
        room.views = std::move(*views);

        auto instances = Detail::ReadRoomList<RoomInstance>(
            view, chunk, ReadU32(view, at + 48), RoomInstanceSize,
            "ROOM instances",
            [](Span<const std::byte> body, std::size_t entry) {
                RoomInstance value;
                value.x = ReadI32(body, entry);
                value.y = ReadI32(body, entry + 4);
                value.object = ReadI32(body, entry + 8);
                value.id = ReadU32(body, entry + 12);
                value.creationCode = ReadI32(body, entry + 16);
                value.scaleX = ReadF32(body, entry + 20);
                value.scaleY = ReadF32(body, entry + 24);
                value.color = ReadU32(body, entry + 28);
                value.rotation = ReadF32(body, entry + 32);
                value.preCreateCode = ReadI32(body, entry + 36);
                return value;
            });
        if (!instances) {
            return Unexpected<TellerEngine::Base::Error>(instances.error());
        }
        room.instances = std::move(*instances);

        auto tiles = Detail::ReadRoomList<RoomTile>(
            view, chunk, ReadU32(view, at + 52), RoomTileSize, "ROOM tiles",
            [](Span<const std::byte> body, std::size_t entry) {
                RoomTile value;
                value.x = ReadI32(body, entry);
                value.y = ReadI32(body, entry + 4);
                value.background = ReadI32(body, entry + 8);
                value.sourceX = ReadU32(body, entry + 12);
                value.sourceY = ReadU32(body, entry + 16);
                value.width = ReadU32(body, entry + 20);
                value.height = ReadU32(body, entry + 24);
                value.depth = ReadI32(body, entry + 28);
                value.id = ReadU32(body, entry + 32);
                value.scaleX = ReadF32(body, entry + 36);
                value.scaleY = ReadF32(body, entry + 40);
                value.color = ReadU32(body, entry + 44);
                return value;
            });
        if (!tiles) {
            return Unexpected<TellerEngine::Base::Error>(tiles.error());
        }
        room.tiles = std::move(*tiles);

        table.rooms.push_back(std::move(room));
    }

    return table;
}

} // namespace TellerEngineEngine::Extract
