#include <Extract/Chunks.hpp>
#include <Extract/Strings.hpp>
#include <Extract/FileBytes.hpp>

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

void AppendU32(std::string &out, std::uint32_t value) {
    out.push_back(static_cast<char>(value & 0xff));
    out.push_back(static_cast<char>((value >> 8) & 0xff));
    out.push_back(static_cast<char>((value >> 16) & 0xff));
    out.push_back(static_cast<char>((value >> 24) & 0xff));
}

// FORMの先頭にSTRGを1つだけ置くので、中身は16バイト目から始まる
constexpr std::uint32_t StrgOffset = 16;

std::string MakeStrg(const std::vector<std::string> &values) {
    std::string entries;
    std::vector<std::uint32_t> pointers;
    const std::uint32_t entriesBase =
        StrgOffset + 4 + static_cast<std::uint32_t>(values.size()) * 4;

    for (const auto &value : values) {
        pointers.push_back(entriesBase +
                           static_cast<std::uint32_t>(entries.size()));
        AppendU32(entries, static_cast<std::uint32_t>(value.size()));
        entries += value;
        entries.push_back('\0');
    }

    std::string chunk;
    AppendU32(chunk, static_cast<std::uint32_t>(values.size()));
    for (const auto pointer : pointers) {
        AppendU32(chunk, pointer);
    }
    chunk += entries;
    return chunk;
}

std::string MakeForm(const std::string &strg) {
    std::string body = "STRG";
    AppendU32(body, static_cast<std::uint32_t>(strg.size()));
    body += strg;

    std::string form = "FORM";
    AppendU32(form, static_cast<std::uint32_t>(body.size()));
    form += body;
    return form;
}

std::filesystem::path WriteSample(const std::string &contents,
                                  const std::string &fileName) {
    const auto directory =
        std::filesystem::temp_directory_path() / "TellerExtractStrings";
    std::filesystem::create_directories(directory);
    std::ofstream stream(directory / fileName, std::ios::binary);
    stream.write(contents.data(),
                 static_cast<std::streamsize>(contents.size()));
    return directory;
}

} // namespace

TEST_CASE("STRGを読んで出現順とポインタの両方で引ける") {
    const std::vector<std::string> values{"prototype", "@@array@@",
                                          "Greetings.", ""};
    const auto directory =
        WriteSample(MakeForm(MakeStrg(values)), "strings.win");
    const TellerEngine::Extract::FileBytes bytes(directory);

    const auto table = TellerEngine::Extract::ReadChunkTable(bytes, "strings.win");
    REQUIRE(table.has_value());
    const auto *chunk = table->Find("STRG");
    REQUIRE(chunk != nullptr);
    CHECK(chunk->offset == TellerEngine::Extract::ChunkHeaderSize * 2);

    const auto strings =
        TellerEngine::Extract::ReadStringTable(bytes, "strings.win", *chunk);
    REQUIRE(strings.has_value());
    REQUIRE(strings->strings.size() == 4);
    CHECK(strings->strings[0] == "prototype");
    CHECK(strings->strings[2] == "Greetings.");
    CHECK(strings->strings[3].empty());

    // 他のチャンクからの参照は本体の先頭を指す
    const std::uint64_t entriesBase = StrgOffset + 4 + values.size() * 4;
    const auto *first = strings->Find(entriesBase + 4);
    REQUIRE(first != nullptr);
    CHECK(*first == "prototype");

    CHECK(strings->Find(0) == nullptr);
}

TEST_CASE("STRGのポインタがチャンクの外を指していたら拒否する") {
    auto strg = MakeStrg({"abc"});
    // 先頭のポインタを壊す
    strg[4] = static_cast<char>(0x00);
    strg[5] = static_cast<char>(0x00);
    strg[6] = static_cast<char>(0x00);
    strg[7] = static_cast<char>(0x00);

    const auto directory = WriteSample(MakeForm(strg), "broken.win");
    const TellerEngine::Extract::FileBytes bytes(directory);

    const auto table = TellerEngine::Extract::ReadChunkTable(bytes, "broken.win");
    REQUIRE(table.has_value());

    const auto strings = TellerEngine::Extract::ReadStringTable(bytes, "broken.win",
                                                          *table->Find("STRG"));
    REQUIRE_FALSE(strings.has_value());
    CHECK(strings.error().code == TellerEngine::Base::ErrorCode::Malformed);
    CHECK(strings.error().context.find("points outside") != std::string::npos);
}

TEST_CASE("STRGの件数がチャンクの大きさと合わなければ拒否する") {
    std::string strg;
    AppendU32(strg, 1000);
    AppendU32(strg, StrgOffset + 8);

    const auto directory = WriteSample(MakeForm(strg), "count.win");
    const TellerEngine::Extract::FileBytes bytes(directory);

    const auto table = TellerEngine::Extract::ReadChunkTable(bytes, "count.win");
    REQUIRE(table.has_value());

    const auto strings = TellerEngine::Extract::ReadStringTable(bytes, "count.win",
                                                          *table->Find("STRG"));
    REQUIRE_FALSE(strings.has_value());
    CHECK(strings.error().context.find("declares 1000 strings") !=
          std::string::npos);
}
