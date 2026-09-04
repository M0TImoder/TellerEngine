#include <Extract/Chunks.hpp>
#include <Teller/Data/FileBytes.hpp>

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

// 検証用のdata.winを組み立てる
std::string
MakeForm(const std::vector<std::pair<std::string, std::string>> &chunks) {
    std::string body;
    for (const auto &[name, contents] : chunks) {
        body += name;
        AppendU32(body, static_cast<std::uint32_t>(contents.size()));
        body += contents;
    }

    std::string form = "FORM";
    AppendU32(form, static_cast<std::uint32_t>(body.size()));
    form += body;
    return form;
}

std::filesystem::path WriteSample(const std::string &contents,
                                  const std::string &fileName) {
    const auto directory =
        std::filesystem::temp_directory_path() / "TellerExtractChunks";
    std::filesystem::create_directories(directory);
    std::ofstream stream(directory / fileName, std::ios::binary);
    stream.write(contents.data(),
                 static_cast<std::streamsize>(contents.size()));
    return directory;
}

} // namespace

TEST_CASE("FORMを走査してチャンクの位置と大きさを取れる") {
    const auto form =
        MakeForm({{"GEN8", "abcd"}, {"STRG", "efghij"}, {"TXTR", ""}});
    const auto directory = WriteSample(form, "good.win");
    const Teller::Data::FileBytes bytes(directory);

    const auto table = Teller::Extract::ReadChunkTable(bytes, "good.win");
    REQUIRE(table.has_value());
    REQUIRE(table->chunks.size() == 3);

    CHECK(table->chunks[0].name == "GEN8");
    CHECK(table->chunks[0].offset == 16);
    CHECK(table->chunks[0].size == 4);

    CHECK(table->chunks[1].name == "STRG");
    CHECK(table->chunks[1].offset == 28);
    CHECK(table->chunks[1].size == 6);

    CHECK(table->chunks[2].name == "TXTR");
    CHECK(table->chunks[2].size == 0);

    REQUIRE(table->Find("STRG") != nullptr);
    CHECK(table->Find("STRG")->size == 6);
    CHECK(table->Find("SHDR") == nullptr);
}

TEST_CASE("FORMで始まらないファイルは拒否する") {
    std::string bad = "NOPE";
    AppendU32(bad, 4);
    bad += "abcd";

    const auto directory = WriteSample(bad, "bad.win");
    const Teller::Data::FileBytes bytes(directory);

    const auto table = Teller::Extract::ReadChunkTable(bytes, "bad.win");
    REQUIRE_FALSE(table.has_value());
    CHECK(table.error().code == Teller::Data::ErrorCode::Malformed);
    CHECK(table.error().context.find("FORM") != std::string::npos);
}

TEST_CASE("FORMの大きさがファイルと合わなければ拒否する") {
    auto form = MakeForm({{"GEN8", "abcd"}});
    form.push_back('\0');
    const auto directory = WriteSample(form, "short.win");
    const Teller::Data::FileBytes bytes(directory);

    const auto table = Teller::Extract::ReadChunkTable(bytes, "short.win");
    REQUIRE_FALSE(table.has_value());
    CHECK(table.error().code == Teller::Data::ErrorCode::Malformed);
    CHECK(table.error().context.find("does not match file size") !=
          std::string::npos);
}

TEST_CASE("FORMの外へはみ出すチャンクは拒否する") {
    std::string body = "GEN8";
    body.push_back(static_cast<char>(0x40));
    body += std::string(3, '\0');

    std::string form = "FORM";
    form.push_back(static_cast<char>(body.size()));
    form += std::string(3, '\0');
    form += body;

    const auto directory = WriteSample(form, "over.win");
    const Teller::Data::FileBytes bytes(directory);

    const auto table = Teller::Extract::ReadChunkTable(bytes, "over.win");
    REQUIRE_FALSE(table.has_value());
    CHECK(table.error().context.find("runs past the end") != std::string::npos);
}
