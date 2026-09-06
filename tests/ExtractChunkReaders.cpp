#include "Fixture.hpp"

#include <Extract/FileBytes.hpp>
#include <Extract/Backgrounds.hpp>
#include <Extract/Chunks.hpp>
#include <Extract/Code.hpp>
#include <Extract/Fonts.hpp>
#include <Extract/Objects.hpp>
#include <Extract/Paths.hpp>
#include <Extract/Rooms.hpp>
#include <Extract/Scripts.hpp>
#include <Extract/Sounds.hpp>
#include <Extract/Strings.hpp>
#include <Extract/Textures.hpp>

#include <doctest/doctest.h>

#include <bit>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace {

using TellerTest::AppendU32;

const std::vector<std::string> Names{"first", "second", "third", "fourth"};

void AppendF32(std::string &out, float value) {
    AppendU32(out, std::bit_cast<std::uint32_t>(value));
}

void AppendU16(std::string &out, std::uint16_t value) {
    out.push_back(static_cast<char>(value & 0xff));
    out.push_back(static_cast<char>((value >> 8) & 0xff));
}

// 件数 + ポインタ列 + 固定長の実体
std::string MakeList(const std::vector<std::string> &entries,
                     std::uint32_t base) {
    const auto count = static_cast<std::uint32_t>(entries.size());
    std::uint32_t at = base + 4 + count * 4;

    std::string pointers;
    std::string bodies;
    for (const auto &entry : entries) {
        AppendU32(pointers, at);
        at += static_cast<std::uint32_t>(entry.size());
        bodies += entry;
    }

    std::string chunk;
    AppendU32(chunk, count);
    chunk += pointers;
    chunk += bodies;
    return chunk;
}

struct World {
    TellerEngine::Extract::FileBytes bytes;
    TellerEngine::Extract::ChunkTable chunks;
    TellerEngine::Extract::StringTable strings;
    std::string file;
};

// STRGを先頭に置いたFORMを組み立てて読み込む
World Build(const std::string &file,
            std::vector<std::pair<std::string, std::string>> chunks,
            const std::function<std::string(std::uint32_t)> &makeSecond) {
    chunks.insert(
        chunks.begin(),
        {"STRG", TellerTest::MakeStrg(Names, TellerTest::FirstChunkOffset)});
    chunks[1].second = makeSecond(TellerTest::ChunkOffset(chunks, 1));

    const auto directory = TellerTest::WriteSample(
        "TellerExtractReaders", file, TellerTest::MakeForm(chunks));

    TellerEngine::Extract::FileBytes bytes(directory);
    auto table = TellerEngine::Extract::ReadChunkTable(bytes, file);
    REQUIRE(table.has_value());
    auto strings = TellerEngine::Extract::ReadStringTable(bytes, file,
                                                          *table->Find("STRG"));
    REQUIRE(strings.has_value());
    return World{std::move(bytes), std::move(*table), std::move(*strings),
                 file};
}

std::uint32_t Name(std::size_t index) {
    return TellerTest::StrgPointer(Names, index, TellerTest::FirstChunkOffset);
}

} // namespace

TEST_CASE("SCPTを読める") {
    const auto world =
        Build("scpt.win", {{"SCPT", ""}}, [](std::uint32_t base) {
            std::string a, b;
            AppendU32(a, Name(0));
            AppendU32(a, 7);
            AppendU32(b, Name(1));
            AppendU32(b, -1);
            return MakeList({a, b}, base);
        });

    const auto scripts = TellerEngine::Extract::ReadScripts(
        world.bytes, world.file, *world.chunks.Find("SCPT"), world.strings);
    REQUIRE(scripts.has_value());
    REQUIRE(scripts->scripts.size() == 2);
    CHECK(scripts->scripts[0].name == "first");
    CHECK(scripts->scripts[0].code == 7);
    CHECK(scripts->scripts[1].code == -1);
}

TEST_CASE("PATHの点を読める") {
    const auto world =
        Build("path.win", {{"PATH", ""}}, [](std::uint32_t base) {
            std::string entry;
            AppendU32(entry, Name(0));
            AppendU32(entry, 1);
            AppendU32(entry, 0);
            AppendU32(entry, 4);
            AppendU32(entry, 2);
            AppendF32(entry, 10.0f);
            AppendF32(entry, 20.0f);
            AppendF32(entry, 100.0f);
            AppendF32(entry, 30.0f);
            AppendF32(entry, 40.0f);
            AppendF32(entry, 50.0f);
            return MakeList({entry}, base);
        });

    const auto paths = TellerEngine::Extract::ReadPaths(
        world.bytes, world.file, *world.chunks.Find("PATH"), world.strings);
    REQUIRE(paths.has_value());
    REQUIRE(paths->paths.size() == 1);
    CHECK(paths->paths[0].smooth);
    CHECK_FALSE(paths->paths[0].closed);
    CHECK(paths->paths[0].precision == 4);
    REQUIRE(paths->paths[0].points.size() == 2);
    CHECK(paths->paths[0].points[1].x == doctest::Approx(30.0f));
    CHECK(paths->paths[0].points[1].speed == doctest::Approx(50.0f));
}

TEST_CASE("SONDの埋め込みと外部を見分けられる") {
    const auto world =
        Build("sond.win", {{"SOND", ""}}, [](std::uint32_t base) {
            std::string embedded, external;
            for (auto *entry : {&embedded, &external}) {
                AppendU32(*entry, Name(0));
                AppendU32(*entry, entry == &embedded ? 101 : 100);
                AppendU32(*entry, Name(1));
                AppendU32(*entry, Name(2));
                AppendU32(*entry, 0);
                AppendF32(*entry, 0.93f);
                AppendF32(*entry, 0.0f);
                AppendU32(*entry, static_cast<std::uint32_t>(-1));
                AppendU32(*entry, entry == &embedded
                                      ? 5
                                      : static_cast<std::uint32_t>(-1));
            }
            return MakeList({embedded, external}, base);
        });

    const auto sounds = TellerEngine::Extract::ReadSounds(
        world.bytes, world.file, *world.chunks.Find("SOND"), world.strings, 16);
    REQUIRE(sounds.has_value());
    REQUIRE(sounds->sounds.size() == 2);
    CHECK(sounds->sounds[0].IsEmbedded());
    CHECK(sounds->sounds[0].audio == 5);
    CHECK(sounds->sounds[0].volume == doctest::Approx(0.93f));
    CHECK_FALSE(sounds->sounds[1].IsEmbedded());
    CHECK(sounds->sounds[1].audio == -1);
}

TEST_CASE("AUDOの実体を取り出せる") {
    const auto world =
        Build("audo.win", {{"AUDO", ""}}, [](std::uint32_t base) {
            std::string first, second;
            AppendU32(first, 5);
            first += "RIFF!";
            first.push_back('\0'); // 4バイト境界への詰め物
            first.push_back('\0');
            first.push_back('\0');
            AppendU32(second, 3);
            second += "abc";
            return MakeList({first, second}, base);
        });

    const auto audio = TellerEngine::Extract::ReadAudio(
        world.bytes, world.file, *world.chunks.Find("AUDO"));
    REQUIRE(audio.has_value());
    REQUIRE(audio->clips.size() == 2);
    CHECK(audio->clips[0].size == 5);
    CHECK(audio->clips[1].size == 3);

    const auto blob = TellerEngine::Extract::ReadAudioClip(
        world.bytes, world.file, audio->clips[0]);
    REQUIRE(blob.has_value());
    const std::string copied(reinterpret_cast<const char *>(blob->data()),
                             blob->size());
    CHECK(copied == "RIFF!");
}

TEST_CASE("FONTのグリフを読める") {
    const auto world =
        Build("font.win", {{"FONT", ""}}, [](std::uint32_t base) {
            // 実体 44 + グリフポインタ 2*4 = 52 の位置からグリフが並ぶ
            const std::uint32_t entryAt = base + 4 + 4;
            std::string entry;
            AppendU32(entry, Name(0));
            AppendU32(entry, Name(1));
            AppendU32(entry, 24);
            AppendU32(entry, 0);
            AppendU32(entry, 0);
            AppendU16(entry, 32);
            entry.push_back(static_cast<char>(1));
            entry.push_back(static_cast<char>(0));
            AppendU32(entry, 127);
            AppendU32(entry, 0);
            AppendF32(entry, 1.0f);
            AppendF32(entry, 1.0f);
            AppendU32(entry, 2);
            AppendU32(entry, entryAt + 52);
            AppendU32(entry, entryAt + 52 + 16);
            for (std::uint16_t ch : {32, 33}) {
                AppendU16(entry, ch);
                AppendU16(entry, static_cast<std::uint16_t>(ch * 2));
                AppendU16(entry, 5);
                AppendU16(entry, 6);
                AppendU16(entry, 7);
                AppendU16(entry, 8);
                AppendU16(entry, 9);
                AppendU16(entry, 0);
            }
            return MakeList({entry}, base);
        });

    TellerEngine::Extract::TextureRegionTable regions;
    const auto fonts = TellerEngine::Extract::ReadFonts(
        world.bytes, world.file, *world.chunks.Find("FONT"), world.strings,
        regions);
    REQUIRE(fonts.has_value());
    REQUIRE(fonts->fonts.size() == 1);
    CHECK(fonts->fonts[0].emSize == 24);
    CHECK(fonts->fonts[0].rangeStart == 32);
    CHECK(fonts->fonts[0].rangeEnd == 127);
    REQUIRE(fonts->fonts[0].glyphs.size() == 2);
    CHECK(fonts->fonts[0].glyphs[1].character == 33);
    CHECK(fonts->fonts[0].glyphs[1].sourceX == 66);
    CHECK(fonts->fonts[0].glyphs[1].shift == 8);
    CHECK(fonts->fonts[0].glyphs[1].offset == 9);
}

TEST_CASE("OBJTのイベントとアクションを読める") {
    const auto world =
        Build("objt.win", {{"OBJT", ""}}, [](std::uint32_t base) {
            const std::uint32_t entryAt = base + 4 + 4;
            // 固定80 + 件数4 + ポインタ2*4 = 92
            const std::uint32_t listAt = entryAt + 92;
            const std::uint32_t eventAt =
                listAt + 4 + 4 + 4; // 空リスト4 + リスト見出し4+4
            const std::uint32_t actionAt = eventAt + 12;

            std::string entry;
            AppendU32(entry, Name(0));
            AppendU32(entry, 42);                               // sprite
            AppendU32(entry, 1);                                // visible
            AppendU32(entry, 0);                                // solid
            AppendU32(entry, static_cast<std::uint32_t>(-7));   // depth
            AppendU32(entry, 1);                                // persistent
            AppendU32(entry, static_cast<std::uint32_t>(-100)); // parent
            AppendU32(entry, static_cast<std::uint32_t>(-1));   // textureMask
            for (int i = 0; i < 12; ++i) {
                AppendU32(entry, 0);
            }
            AppendU32(entry, 2); // イベント種別の数
            AppendU32(entry, listAt);
            AppendU32(entry, listAt + 4);

            AppendU32(entry, 0); // 種別0は空
            AppendU32(entry, 1); // 種別1に1件
            AppendU32(entry, eventAt);

            AppendU32(entry, 11); // subtype
            AppendU32(entry, 1);  // actionCount
            AppendU32(entry, actionAt);

            for (int i = 0; i < 7; ++i) {
                AppendU32(entry, 0);
            }
            AppendU32(entry, Name(1));                        // actionName
            AppendU32(entry, 123);                            // code
            AppendU32(entry, 2);                              // argumentCount
            AppendU32(entry, static_cast<std::uint32_t>(-1)); // who
            AppendU32(entry, 1);                              // relative
            for (int i = 0; i < 2; ++i) {
                AppendU32(entry, 0);
            }
            return MakeList({entry}, base);
        });

    const auto objects = TellerEngine::Extract::ReadObjects(
        world.bytes, world.file, *world.chunks.Find("OBJT"), world.strings);
    REQUIRE(objects.has_value());
    REQUIRE(objects->objects.size() == 1);

    const auto &object = objects->objects[0];
    CHECK(object.name == "first");
    CHECK(object.sprite == 42);
    CHECK(object.depth == -7);
    CHECK(object.parent == -100);
    CHECK(object.persistent);
    REQUIRE(object.events.size() == 1);
    CHECK(object.events[0].kind == TellerEngine::Extract::EventKind::Destroy);
    CHECK(object.events[0].subtype == 11);
    REQUIRE(object.events[0].actions.size() == 1);
    CHECK(object.events[0].actions[0].name == "second");
    CHECK(object.events[0].actions[0].code == 123);
    CHECK(object.events[0].actions[0].relative);
}

TEST_CASE("ROOMの4つのリストを読める") {
    const auto world =
        Build("room.win", {{"ROOM", ""}}, [](std::uint32_t base) {
            const std::uint32_t entryAt = base + 4 + 4;
            const std::uint32_t bgAt = entryAt + 88;
            const std::uint32_t viewAt = bgAt + 4;
            const std::uint32_t instAt = viewAt + 4;
            const std::uint32_t tileAt = instAt + 4 + 4 + 40;

            std::string entry;
            AppendU32(entry, Name(0));
            AppendU32(entry, Name(1));
            AppendU32(entry, 320);
            AppendU32(entry, 240);
            AppendU32(entry, 30);
            AppendU32(entry, 0);
            AppendU32(entry, 0);
            AppendU32(entry, 1);
            AppendU32(entry, static_cast<std::uint32_t>(-1));
            AppendU32(entry, 2);
            AppendU32(entry, bgAt);
            AppendU32(entry, viewAt);
            AppendU32(entry, instAt);
            AppendU32(entry, tileAt);
            for (int i = 0; i < 8; ++i) {
                AppendU32(entry, 0);
            }

            AppendU32(entry, 0); // backgrounds
            AppendU32(entry, 0); // views

            AppendU32(entry, 1); // instances
            AppendU32(entry, instAt + 8);
            AppendU32(entry, 100);
            AppendU32(entry, 200);
            AppendU32(entry, 5);
            AppendU32(entry, 100001);
            AppendU32(entry, static_cast<std::uint32_t>(-1));
            AppendF32(entry, 1.0f);
            AppendF32(entry, 2.0f);
            AppendU32(entry, 0xffffffff);
            AppendF32(entry, 90.0f);
            AppendU32(entry, static_cast<std::uint32_t>(-1)); // preCreateCode

            AppendU32(entry, 1); // tiles
            AppendU32(entry, tileAt + 8);
            AppendU32(entry, 16);
            AppendU32(entry, 32);
            AppendU32(entry, 3);
            AppendU32(entry, 8);
            AppendU32(entry, 9);
            AppendU32(entry, 20);
            AppendU32(entry, 21);
            AppendU32(entry, static_cast<std::uint32_t>(-100));
            AppendU32(entry, 10000001);
            AppendF32(entry, 1.0f);
            AppendF32(entry, 1.0f);
            AppendU32(entry, 0);
            return MakeList({entry}, base);
        });

    const auto rooms = TellerEngine::Extract::ReadRooms(
        world.bytes, world.file, *world.chunks.Find("ROOM"), world.strings);
    REQUIRE(rooms.has_value());
    REQUIRE(rooms->rooms.size() == 1);

    const auto &room = rooms->rooms[0];
    CHECK(room.name == "first");
    CHECK(room.width == 320);
    CHECK(room.speed == 30);
    CHECK(room.drawBackgroundColor);
    REQUIRE(room.instances.size() == 1);
    CHECK(room.instances[0].x == 100);
    CHECK(room.instances[0].object == 5);
    CHECK(room.instances[0].id == 100001);
    CHECK(room.instances[0].scaleY == doctest::Approx(2.0f));
    CHECK(room.instances[0].preCreateCode == -1);
    REQUIRE(room.tiles.size() == 1);
    CHECK(room.tiles[0].background == 3);
    CHECK(room.tiles[0].depth == -100);
    CHECK(room.tiles[0].id == 10000001);
}

TEST_CASE("CODEはバイトコードの位置を相対値から求める") {
    const auto world =
        Build("code.win", {{"CODE", ""}}, [](std::uint32_t base) {
            const std::uint32_t bytecodeAt = base + 4 + 4;
            const std::uint32_t entryAt = bytecodeAt + 8;

            std::string chunk;
            AppendU32(chunk, 1);
            AppendU32(chunk, entryAt);
            chunk += "12345678"; // バイトコード本体
            AppendU32(chunk, Name(0));
            AppendU32(chunk, 8);
            AppendU16(chunk, 3);
            AppendU16(chunk, 1);
            AppendU32(chunk, static_cast<std::uint32_t>(
                                 static_cast<std::int32_t>(bytecodeAt) -
                                 static_cast<std::int32_t>(entryAt + 12)));
            AppendU32(chunk, 0);
            return chunk;
        });

    const auto code = TellerEngine::Extract::ReadCode(
        world.bytes, world.file, *world.chunks.Find("CODE"), world.strings);
    REQUIRE(code.has_value());
    REQUIRE(code->entries.size() == 1);
    CHECK(code->entries[0].length == 8);
    CHECK(code->entries[0].localCount == 3);
    CHECK(code->entries[0].argumentCount == 1);

    const auto blob = TellerEngine::Extract::ReadBytecode(
        world.bytes, world.file, code->entries[0]);
    REQUIRE(blob.has_value());
    const std::string copied(reinterpret_cast<const char *>(blob->data()),
                             blob->size());
    CHECK(copied == "12345678");
}

TEST_CASE("VARIとFUNCを読める") {
    const auto world =
        Build("vari.win", {{"VARI", ""}, {"FUNC", ""}}, [](std::uint32_t) {
            std::string chunk;
            AppendU32(chunk, 3907);
            AppendU32(chunk, 3907);
            AppendU32(chunk, 40);
            AppendU32(chunk, Name(0));
            AppendU32(chunk, static_cast<std::uint32_t>(-1));
            AppendU32(chunk, 0);
            AppendU32(chunk, 12);
            AppendU32(chunk, static_cast<std::uint32_t>(-1));
            return chunk;
        });

    const auto variables = TellerEngine::Extract::ReadVariables(
        world.bytes, world.file, *world.chunks.Find("VARI"), world.strings);
    REQUIRE(variables.has_value());
    CHECK(variables->declaredCount == 3907);
    CHECK(variables->maxLocalCount == 40);
    REQUIRE(variables->variables.size() == 1);
    CHECK(variables->variables[0].name == "first");
    CHECK(variables->variables[0].instanceType == -1);
    CHECK(variables->variables[0].occurrences == 12);
}

TEST_CASE("VARIの端数は拒否する") {
    const auto world = Build("varibad.win", {{"VARI", ""}}, [](std::uint32_t) {
        std::string chunk;
        AppendU32(chunk, 0);
        AppendU32(chunk, 0);
        AppendU32(chunk, 0);
        chunk += "xyz";
        return chunk;
    });

    const auto variables = TellerEngine::Extract::ReadVariables(
        world.bytes, world.file, *world.chunks.Find("VARI"), world.strings);
    REQUIRE_FALSE(variables.has_value());
    CHECK(variables.error().context.find("not a multiple of 20") !=
          std::string::npos);
}
