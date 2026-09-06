#pragma once

// OBJTチャンクのオブジェクト定義
// 固定80バイトのあとに、イベント種別ごとのリストが13本続く
// 各リストは 件数 + ポインタ列 + 実体
// イベントは 種別番号 + アクション数 + ポインタ列 + 実体
// アクションは56バイト固定

#include <Extract/Chunks.hpp>
#include <Extract/Reader.hpp>
#include <Extract/Strings.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace TellerEngine::Extract {

inline constexpr std::uint64_t ObjectFixedSize = 80;
inline constexpr std::uint64_t ActionSize = 56;

enum class EventKind : std::uint32_t {
    Create = 0,
    Destroy = 1,
    Alarm = 2,
    Step = 3,
    Collision = 4,
    Keyboard = 5,
    Mouse = 6,
    Other = 7,
    Draw = 8,
    KeyPress = 9,
    KeyRelease = 10,
    Trigger = 11,
    CleanUp = 12,
};

struct Action {
    std::uint32_t libraryId = 0;
    std::uint32_t id = 0;
    std::uint32_t kind = 0;
    bool useRelative = false;
    bool isQuestion = false;
    bool useApplyTo = false;
    std::uint32_t executionType = 0;
    std::string name;
    std::int32_t code = -1;
    std::uint32_t argumentCount = 0;
    std::int32_t who = -1;
    bool relative = false;
    bool isNot = false;
    std::uint32_t unknown = 0;
};

struct Event {
    EventKind kind = EventKind::Create;

    // Alarmなら番号、Collisionなら相手のオブジェクト、KeyPressならキーコード
    std::uint32_t subtype = 0;

    std::vector<Action> actions;
};

struct Object {
    std::string name;
    std::int32_t sprite = -1;
    bool visible = false;
    bool solid = false;
    std::int32_t depth = 0;
    bool persistent = false;
    std::int32_t parent = -1;
    std::int32_t textureMask = -1;

    bool usesPhysics = false;
    bool isSensor = false;
    std::uint32_t collisionShape = 0;
    float density = 0.0f;
    float restitution = 0.0f;
    std::uint32_t group = 0;
    float linearDamping = 0.0f;
    float angularDamping = 0.0f;
    std::int32_t physicsVertexCount = 0;
    float friction = 0.0f;
    bool awake = false;
    bool kinematic = false;

    std::vector<Event> events;
};

struct ObjectTable {
    std::vector<Object> objects;
};

inline Expected<ObjectTable, TellerEngine::Base::Error>
ReadObjects(const TellerEngine::Extract::Bytes &bytes, std::string_view name, const Chunk &chunk,
            const StringTable &strings) {
    const auto contents = ReadChunk(bytes, name, chunk);
    if (!contents) {
        return Unexpected<TellerEngine::Base::Error>(contents.error());
    }
    const Span<const std::byte> view(*contents);

    const auto pointers = ReadPointerList(view, chunk, "OBJT");
    if (!pointers) {
        return Unexpected<TellerEngine::Base::Error>(pointers.error());
    }

    ObjectTable table;
    table.objects.reserve(pointers->size());

    for (const auto pointer : *pointers) {
        if (!InChunk(chunk, pointer, ObjectFixedSize + 4)) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed("OBJT entry runs past the end of the chunk"));
        }
        const auto at = LocalOffset(chunk, pointer);

        Object object;
        auto text = ResolveString(strings, ReadU32(view, at), "OBJT name");
        if (!text) {
            return Unexpected<TellerEngine::Base::Error>(text.error());
        }
        object.name = std::move(*text);

        object.sprite = ReadI32(view, at + 4);
        object.visible = ReadU32(view, at + 8) != 0;
        object.solid = ReadU32(view, at + 12) != 0;
        object.depth = ReadI32(view, at + 16);
        object.persistent = ReadU32(view, at + 20) != 0;
        object.parent = ReadI32(view, at + 24);
        object.textureMask = ReadI32(view, at + 28);
        object.usesPhysics = ReadU32(view, at + 32) != 0;
        object.isSensor = ReadU32(view, at + 36) != 0;
        object.collisionShape = ReadU32(view, at + 40);
        object.density = ReadF32(view, at + 44);
        object.restitution = ReadF32(view, at + 48);
        object.group = ReadU32(view, at + 52);
        object.linearDamping = ReadF32(view, at + 56);
        object.angularDamping = ReadF32(view, at + 60);
        object.physicsVertexCount = ReadI32(view, at + 64);
        object.friction = ReadF32(view, at + 68);
        object.awake = ReadU32(view, at + 72) != 0;
        object.kinematic = ReadU32(view, at + 76) != 0;

        const std::uint64_t kindCount = ReadU32(view, at + ObjectFixedSize);
        if (!InChunk(chunk, pointer, ObjectFixedSize + 4 + kindCount * 4)) {
            return Unexpected<TellerEngine::Base::Error>(Malformed(
                object.name + " declares " + std::to_string(kindCount) +
                " event kinds which run past the end of the chunk"));
        }

        for (std::uint64_t kind = 0; kind < kindCount; ++kind) {
            const std::uint64_t listPointer =
                ReadU32(view, static_cast<std::size_t>(at + ObjectFixedSize +
                                                       4 + kind * 4));
            if (!InChunk(chunk, listPointer, 4)) {
                return Unexpected<TellerEngine::Base::Error>(Malformed(
                    object.name + " event list " + std::to_string(kind) +
                    " points outside the chunk"));
            }
            const auto listAt = LocalOffset(chunk, listPointer);
            const std::uint64_t eventCount = ReadU32(view, listAt);
            if (!InChunk(chunk, listPointer, 4 + eventCount * 4)) {
                return Unexpected<TellerEngine::Base::Error>(Malformed(
                    object.name + " event list " + std::to_string(kind) +
                    " runs past the end of the chunk"));
            }

            for (std::uint64_t index = 0; index < eventCount; ++index) {
                const std::uint64_t eventPointer = ReadU32(
                    view, static_cast<std::size_t>(listAt + 4 + index * 4));
                if (!InChunk(chunk, eventPointer, 8)) {
                    return Unexpected<TellerEngine::Base::Error>(Malformed(
                        object.name + " event points outside the chunk"));
                }
                const auto eventAt = LocalOffset(chunk, eventPointer);

                Event event;
                event.kind = static_cast<EventKind>(kind);
                event.subtype = ReadU32(view, eventAt);

                const std::uint64_t actionCount = ReadU32(view, eventAt + 4);
                if (!InChunk(chunk, eventPointer, 8 + actionCount * 4)) {
                    return Unexpected<TellerEngine::Base::Error>(
                        Malformed(object.name +
                                  " event has actions which run past the end"));
                }

                event.actions.reserve(static_cast<std::size_t>(actionCount));
                for (std::uint64_t action = 0; action < actionCount; ++action) {
                    const std::uint64_t actionPointer =
                        ReadU32(view, static_cast<std::size_t>(eventAt + 8 +
                                                               action * 4));
                    if (!InChunk(chunk, actionPointer, ActionSize)) {
                        return Unexpected<TellerEngine::Base::Error>(Malformed(
                            object.name + " action points outside the chunk"));
                    }
                    const auto actionAt = LocalOffset(chunk, actionPointer);

                    Action entry;
                    auto actionName =
                        ResolveString(strings, ReadU32(view, actionAt + 28),
                                      "OBJT action name");
                    if (!actionName) {
                        return Unexpected<TellerEngine::Base::Error>(actionName.error());
                    }
                    entry.name = std::move(*actionName);
                    entry.libraryId = ReadU32(view, actionAt + 0);
                    entry.id = ReadU32(view, actionAt + 4);
                    entry.kind = ReadU32(view, actionAt + 8);
                    entry.useRelative = ReadU32(view, actionAt + 12) != 0;
                    entry.isQuestion = ReadU32(view, actionAt + 16) != 0;
                    entry.useApplyTo = ReadU32(view, actionAt + 20) != 0;
                    entry.executionType = ReadU32(view, actionAt + 24);
                    entry.code = ReadI32(view, actionAt + 32);
                    entry.argumentCount = ReadU32(view, actionAt + 36);
                    entry.who = ReadI32(view, actionAt + 40);
                    entry.relative = ReadU32(view, actionAt + 44) != 0;
                    entry.isNot = ReadU32(view, actionAt + 48) != 0;
                    entry.unknown = ReadU32(view, actionAt + 52);
                    event.actions.push_back(std::move(entry));
                }
                object.events.push_back(std::move(event));
            }
        }
        table.objects.push_back(std::move(object));
    }

    return table;
}

} // namespace TellerEngineEngine::Extract
