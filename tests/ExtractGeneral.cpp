#include "Fixture.hpp"

#include <Extract/Chunks.hpp>
#include <Extract/General.hpp>
#include <Extract/Strings.hpp>
#include <Extract/FileBytes.hpp>

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

const std::vector<std::string> Values{"UNDERTALE", "Default",
                                      "UNDERTALE the Musical"};

std::string MakeGen8(std::uint32_t roomCount, std::uint32_t declaredCount) {
    const std::uint32_t base = TellerTest::FirstChunkOffset;

    std::string chunk;
    chunk.push_back(static_cast<char>(1));  // debug
    chunk.push_back(static_cast<char>(16)); // bytecode
    chunk.push_back('\0');
    chunk.push_back('\0');

    TellerTest::AppendU32(chunk,
                          TellerTest::StrgPointer(Values, 0, base)); // filename
    TellerTest::AppendU32(chunk,
                          TellerTest::StrgPointer(Values, 1, base)); // config
    TellerTest::AppendU32(chunk, 113926);    // lastObject
    TellerTest::AppendU32(chunk, 10068815);  // lastTile
    TellerTest::AppendU32(chunk, 864738521); // gameId
    chunk += std::string(16, '\0');          // guid
    TellerTest::AppendU32(chunk,
                          TellerTest::StrgPointer(Values, 0, base)); // name
    TellerTest::AppendU32(chunk, 1);                                 // major
    TellerTest::AppendU32(chunk, 0);                                 // minor
    TellerTest::AppendU32(chunk, 0);                                 // release
    TellerTest::AppendU32(chunk, 1539);                              // build
    TellerTest::AppendU32(chunk, 640);                               // width
    TellerTest::AppendU32(chunk, 480);                               // height
    TellerTest::AppendU32(chunk, 2226);                              // info
    chunk += std::string(16, '\0');           // licenseMd5
    TellerTest::AppendU32(chunk, 3656893235); // licenseCrc32
    TellerTest::AppendU64(chunk, 1510935285); // timestamp
    TellerTest::AppendU32(
        chunk, TellerTest::StrgPointer(Values, 2, base)); // displayName
    TellerTest::AppendU64(chunk, 0);                      // activeTargets
    TellerTest::AppendU64(chunk, 3476975234697801462);
    TellerTest::AppendU32(chunk,
                          static_cast<std::uint32_t>(-391540)); // steamAppId
    TellerTest::AppendU32(chunk, 6502);                         // debuggerPort

    TellerTest::AppendU32(chunk, declaredCount);
    for (std::uint32_t index = 0; index < roomCount; ++index) {
        TellerTest::AppendU32(chunk, index);
    }
    return chunk;
}

struct Sample {
    TellerEngine::Extract::FileBytes bytes;
    TellerEngine::Extract::ChunkTable table;
    TellerEngine::Extract::StringTable strings;
};

Sample Load(const std::string &fileName, const std::string &gen8) {
    const auto strg =
        TellerTest::MakeStrg(Values, TellerTest::FirstChunkOffset);
    const auto directory = TellerTest::WriteSample(
        "TellerExtractGeneral", fileName,
        TellerTest::MakeForm({{"STRG", strg}, {"GEN8", gen8}}));

    TellerEngine::Extract::FileBytes bytes(directory);
    auto table = TellerEngine::Extract::ReadChunkTable(bytes, fileName);
    REQUIRE(table.has_value());
    auto strings =
        TellerEngine::Extract::ReadStringTable(bytes, fileName, *table->Find("STRG"));
    REQUIRE(strings.has_value());
    return Sample{std::move(bytes), std::move(*table), std::move(*strings)};
}

} // namespace

TEST_CASE("GEN8の固定部とルームの並び順を読める") {
    const auto sample = Load("general.win", MakeGen8(4, 4));

    const auto general = TellerEngine::Extract::ReadGeneralInfo(
        sample.bytes, "general.win", *sample.table.Find("GEN8"),
        sample.strings);
    REQUIRE(general.has_value());

    CHECK(general->debuggerDisabled);
    CHECK(general->bytecodeVersion == 16);
    CHECK(general->filename == "UNDERTALE");
    CHECK(general->config == "Default");
    CHECK(general->name == "UNDERTALE");
    CHECK(general->displayName == "UNDERTALE the Musical");
    CHECK(general->gameId == 864738521);
    CHECK(general->major == 1);
    CHECK(general->build == 1539);
    CHECK(general->defaultWindowWidth == 640);
    CHECK(general->defaultWindowHeight == 480);
    CHECK(general->info == 2226);
    CHECK(general->licenseCrc32 == 3656893235);
    CHECK(general->timestamp == 1510935285);
    CHECK(general->steamAppId == -391540);
    CHECK(general->debuggerPort == 6502);
    REQUIRE(general->roomOrder.size() == 4);
    CHECK(general->roomOrder[3] == 3);
}

TEST_CASE("GEN8のルーム件数がチャンクの大きさと合わなければ拒否する") {
    const auto sample = Load("count.win", MakeGen8(4, 99));

    const auto general = TellerEngine::Extract::ReadGeneralInfo(
        sample.bytes, "count.win", *sample.table.Find("GEN8"), sample.strings);
    REQUIRE_FALSE(general.has_value());
    CHECK(general.error().code == TellerEngine::Base::ErrorCode::Malformed);
    CHECK(general.error().context.find("declares 99 rooms") !=
          std::string::npos);
}

TEST_CASE("GEN8の文字列がSTRGに無ければ拒否する") {
    auto gen8 = MakeGen8(1, 1);
    // filenameのポインタを壊す
    gen8[4] = static_cast<char>(0xff);
    gen8[5] = static_cast<char>(0xff);

    const auto sample = Load("string.win", gen8);
    const auto general = TellerEngine::Extract::ReadGeneralInfo(
        sample.bytes, "string.win", *sample.table.Find("GEN8"), sample.strings);
    REQUIRE_FALSE(general.has_value());
    CHECK(general.error().context.find("not in STRG") != std::string::npos);
}
