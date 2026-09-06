#include "Fixture.hpp"

#include <Extract/FileBytes.hpp>
#include <Extract/GameData.hpp>

#include <doctest/doctest.h>

#include <string>
#include <utility>
#include <vector>

namespace {

using TellerTest::AppendU32;

std::string MakeMinimal(bool withRoom) {
    const std::vector<std::string> names{"room_start", ""};

    std::vector<std::pair<std::string, std::string>> chunks{
        {"STRG", TellerTest::MakeStrg(names, TellerTest::FirstChunkOffset)}};

    for (const auto *empty :
         {"TXTR", "TPAG", "SPRT", "BGND", "FONT", "SOND", "AUDO", "PATH",
          "SCPT", "OBJT", "CODE", "EXTN", "AGRP", "GLOB", "SHDR", "TMLN"}) {
        std::string body;
        AppendU32(body, 0);
        chunks.emplace_back(empty, body);
    }

    if (withRoom) {
        std::string body;
        AppendU32(body, 0);
        chunks.emplace_back("ROOM", body);
    }

    std::string optn;
    AppendU32(optn, 0);
    chunks.emplace_back("OPTN", optn);

    std::string lang;
    AppendU32(lang, 0);
    AppendU32(lang, 0);
    AppendU32(lang, 0);
    chunks.emplace_back("LANG", lang);

    chunks.emplace_back("DAFL", "");

    std::string vari;
    AppendU32(vari, 0);
    AppendU32(vari, 0);
    AppendU32(vari, 0);
    chunks.emplace_back("VARI", vari);

    std::string func;
    AppendU32(func, 0);
    chunks.emplace_back("FUNC", func);

    std::string gen8;
    gen8.push_back('\0');
    gen8.push_back(static_cast<char>(16));
    gen8.push_back('\0');
    gen8.push_back('\0');
    for (int i = 0; i < 31; ++i) {
        AppendU32(gen8, 0);
    }
    AppendU32(gen8, 0); // roomOrderCount
    chunks.emplace_back("GEN8", gen8);

    return TellerTest::MakeForm(chunks);
}

} // namespace

TEST_CASE("抽出ツールはdata.winを丸ごと読み込める") {
    const auto directory = TellerTest::WriteSample(
        "TellerExtractGameData", "data.win", MakeMinimal(true));
    const TellerEngine::Extract::FileBytes bytes(directory);

    const auto game = TellerEngine::Extract::LoadGameData(bytes);
    REQUIRE(game.has_value());
    CHECK(game->general.bytecodeVersion == 16);
    CHECK(game->sprites.sprites.empty());
    CHECK(game->rooms.rooms.empty());
    CHECK(game->FindSprite("spr_heart") == nullptr);
}

TEST_CASE("必須のチャンクが欠けていたら名前を添えて失敗する") {
    const auto directory = TellerTest::WriteSample(
        "TellerExtractGameData", "noroom.win", MakeMinimal(false));
    const TellerEngine::Extract::FileBytes bytes(directory);

    const auto game = TellerEngine::Extract::LoadGameData(bytes, "noroom.win");
    REQUIRE_FALSE(game.has_value());
    CHECK(game.error().code == TellerEngine::Base::ErrorCode::Malformed);
    CHECK(game.error().context == "ROOM chunk is missing");
}
