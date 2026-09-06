#include "Fixture.hpp"

#include <Extract/Chunks.hpp>
#include <Extract/Sprites.hpp>
#include <Extract/Strings.hpp>
#include <Extract/Textures.hpp>
#include <Extract/FileBytes.hpp>

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

const std::vector<std::string> Names{"spr_heart"};

// 16x16、2フレーム、マスク1枚
std::string MakeSprt(std::uint32_t strgBase, std::uint32_t tpagBase,
                     std::uint32_t sprtBase, std::uint32_t badFrame) {
    const std::uint32_t entryAt = sprtBase + 4 + 4;

    std::string entry;
    TellerTest::AppendU32(entry, TellerTest::StrgPointer(Names, 0, strgBase));
    TellerTest::AppendU32(entry, 16); // width
    TellerTest::AppendU32(entry, 16); // height
    TellerTest::AppendU32(entry, 0);  // marginLeft
    TellerTest::AppendU32(entry, 15); // marginRight
    TellerTest::AppendU32(entry, 15); // marginBottom
    TellerTest::AppendU32(entry, 0);  // marginTop
    TellerTest::AppendU32(entry, 0);  // transparent
    TellerTest::AppendU32(entry, 1);  // smooth
    TellerTest::AppendU32(entry, 0);  // preload
    TellerTest::AppendU32(entry, 2);  // boundingBoxMode
    TellerTest::AppendU32(entry, 1);  // separateMasks
    TellerTest::AppendU32(entry, 8);  // originX
    TellerTest::AppendU32(entry, 9);  // originY

    TellerTest::AppendU32(entry, 2); // frameCount
    TellerTest::AppendU32(entry, badFrame != 0
                                     ? badFrame
                                     : TellerTest::TpagPointer(2, 0, tpagBase));
    TellerTest::AppendU32(entry, TellerTest::TpagPointer(2, 1, tpagBase));

    TellerTest::AppendU32(entry, 1);      // maskCount
    entry += std::string(2 * 16, '\xaa'); // マスク本体 stride 2 * 16行

    std::string chunk;
    TellerTest::AppendU32(chunk, 1);
    TellerTest::AppendU32(chunk, entryAt);
    chunk += entry;
    return chunk;
}

struct Sample {
    TellerEngine::Extract::FileBytes bytes;
    TellerEngine::Extract::ChunkTable table;
    TellerEngine::Extract::StringTable strings;
    TellerEngine::Extract::TextureRegionTable regions;
};

Sample Load(const std::string &fileName, std::uint32_t badFrame) {
    const std::vector<std::vector<std::uint16_t>> tpagEntries{
        {0, 0, 16, 16, 0, 0, 16, 16, 16, 16, 0},
        {16, 0, 16, 16, 0, 0, 16, 16, 16, 16, 0}};

    std::vector<std::pair<std::string, std::string>> chunks{
        {"STRG", TellerTest::MakeStrg(Names, TellerTest::FirstChunkOffset)},
        {"TPAG", ""},
        {"SPRT", ""}};
    chunks[1].second =
        TellerTest::MakeTpag(tpagEntries, TellerTest::ChunkOffset(chunks, 1));
    chunks[2].second = MakeSprt(TellerTest::ChunkOffset(chunks, 0),
                                TellerTest::ChunkOffset(chunks, 1),
                                TellerTest::ChunkOffset(chunks, 2), badFrame);

    const auto directory = TellerTest::WriteSample(
        "TellerExtractSprites", fileName, TellerTest::MakeForm(chunks));

    TellerEngine::Extract::FileBytes bytes(directory);
    auto table = TellerEngine::Extract::ReadChunkTable(bytes, fileName);
    REQUIRE(table.has_value());
    auto strings =
        TellerEngine::Extract::ReadStringTable(bytes, fileName, *table->Find("STRG"));
    REQUIRE(strings.has_value());
    auto regions = TellerEngine::Extract::ReadTextureRegions(bytes, fileName,
                                                       *table->Find("TPAG"), 1);
    REQUIRE(regions.has_value());
    return Sample{std::move(bytes), std::move(*table), std::move(*strings),
                  std::move(*regions)};
}

} // namespace

TEST_CASE("SPRTから原点とフレームとマスクを読める") {
    const auto sample = Load("sprites.win", 0);

    const auto sprites = TellerEngine::Extract::ReadSprites(
        sample.bytes, "sprites.win", *sample.table.Find("SPRT"), sample.strings,
        sample.regions);
    REQUIRE(sprites.has_value());
    REQUIRE(sprites->sprites.size() == 1);

    const auto &sprite = sprites->sprites[0];
    CHECK(sprite.name == "spr_heart");
    CHECK(sprite.width == 16);
    CHECK(sprite.height == 16);
    CHECK(sprite.originX == 8);
    CHECK(sprite.originY == 9);
    CHECK(sprite.marginRight == 15);
    CHECK(sprite.smooth);
    CHECK_FALSE(sprite.transparent);
    CHECK(sprite.boundingBoxMode == 2);
    CHECK(sprite.separateMasks == 1);
    REQUIRE(sprite.frames.size() == 2);
    CHECK(sprite.maskCount == 1);
    CHECK(sprite.maskStride == 2);

    // フレームがTPAGの矩形として引ける
    const auto *region = sample.regions.Find(sprite.frames[1]);
    REQUIRE(region != nullptr);
    CHECK(region->sourceX == 16);

    const auto mask =
        TellerEngine::Extract::ReadSpriteMask(sample.bytes, "sprites.win", sprite, 0);
    REQUIRE(mask.has_value());
    CHECK(mask->size() == 32);
    CHECK(static_cast<unsigned char>(mask->front()) == 0xaa);

    const auto missing =
        TellerEngine::Extract::ReadSpriteMask(sample.bytes, "sprites.win", sprite, 1);
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error().code == TellerEngine::Base::ErrorCode::OutOfRange);
}

TEST_CASE("TPAGに無い矩形を指すフレームは拒否する") {
    const auto sample = Load("badframe.win", 12345);

    const auto sprites = TellerEngine::Extract::ReadSprites(
        sample.bytes, "badframe.win", *sample.table.Find("SPRT"),
        sample.strings, sample.regions);
    REQUIRE_FALSE(sprites.has_value());
    CHECK(sprites.error().code == TellerEngine::Base::ErrorCode::Malformed);
    CHECK(sprites.error().context.find("not in TPAG") != std::string::npos);
}
