#include <Extract/Chunks.hpp>
#include <Extract/Textures.hpp>
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

void AppendU32Be(std::string &out, std::uint32_t value) {
    out.push_back(static_cast<char>((value >> 24) & 0xff));
    out.push_back(static_cast<char>((value >> 16) & 0xff));
    out.push_back(static_cast<char>((value >> 8) & 0xff));
    out.push_back(static_cast<char>(value & 0xff));
}

// PNGのチャンク1つ
// CRCは読み取り側が見ないので0で埋める
void AppendPngChunk(std::string &out, const std::string &type,
                    const std::string &body) {
    AppendU32Be(out, static_cast<std::uint32_t>(body.size()));
    out += type;
    out += body;
    AppendU32Be(out, 0);
}

std::string MakePng(std::uint32_t width, std::uint32_t height,
                    std::size_t payload) {
    std::string png = "\x89PNG\r\n\x1a\n";

    std::string ihdr;
    AppendU32Be(ihdr, width);
    AppendU32Be(ihdr, height);
    ihdr += std::string(5, '\0');
    AppendPngChunk(png, "IHDR", ihdr);

    AppendPngChunk(png, "IDAT", std::string(payload, 'x'));
    AppendPngChunk(png, "IEND", "");
    return png;
}

constexpr std::uint32_t TxtrOffset = 16;

std::string MakeTxtr(const std::vector<std::string> &pngs) {
    const auto count = static_cast<std::uint32_t>(pngs.size());
    const std::uint32_t entriesBase = TxtrOffset + 4 + count * 4;
    const std::uint32_t blobsBase = entriesBase + count * 8;

    std::string entries;
    std::string blobs;
    std::string pointers;

    for (std::uint32_t index = 0; index < count; ++index) {
        AppendU32(pointers, entriesBase + index * 8);
        AppendU32(entries, 0);
        AppendU32(entries,
                  blobsBase + static_cast<std::uint32_t>(blobs.size()));
        blobs += pngs[index];
    }

    std::string chunk;
    AppendU32(chunk, count);
    chunk += pointers;
    chunk += entries;
    chunk += blobs;
    return chunk;
}

std::string MakeForm(const std::string &txtr) {
    std::string body = "TXTR";
    AppendU32(body, static_cast<std::uint32_t>(txtr.size()));
    body += txtr;

    std::string form = "FORM";
    AppendU32(form, static_cast<std::uint32_t>(body.size()));
    form += body;
    return form;
}

std::filesystem::path WriteSample(const std::string &contents,
                                  const std::string &fileName) {
    const auto directory =
        std::filesystem::temp_directory_path() / "TellerExtractTextures";
    std::filesystem::create_directories(directory);
    std::ofstream stream(directory / fileName, std::ios::binary);
    stream.write(contents.data(),
                 static_cast<std::streamsize>(contents.size()));
    return directory;
}

} // namespace

TEST_CASE("TXTRから寸法とPNGの範囲を取れる") {
    const auto first = MakePng(1024, 512, 40);
    const auto second = MakePng(256, 256, 10);
    const auto directory =
        WriteSample(MakeForm(MakeTxtr({first, second})), "textures.win");
    const TellerEngine::Extract::FileBytes bytes(directory);

    const auto table = TellerEngine::Extract::ReadChunkTable(bytes, "textures.win");
    REQUIRE(table.has_value());

    const auto textures = TellerEngine::Extract::ReadTextureTable(
        bytes, "textures.win", *table->Find("TXTR"));
    REQUIRE(textures.has_value());
    REQUIRE(textures->pages.size() == 2);

    CHECK(textures->pages[0].width == 1024);
    CHECK(textures->pages[0].height == 512);
    CHECK(textures->pages[0].pngSize == first.size());

    CHECK(textures->pages[1].width == 256);
    CHECK(textures->pages[1].height == 256);
    CHECK(textures->pages[1].pngSize == second.size());
}

TEST_CASE("取り出したPNGが元のバイト列と一致する") {
    const auto png = MakePng(64, 32, 7);
    const auto directory = WriteSample(MakeForm(MakeTxtr({png})), "one.win");
    const TellerEngine::Extract::FileBytes bytes(directory);

    const auto table = TellerEngine::Extract::ReadChunkTable(bytes, "one.win");
    REQUIRE(table.has_value());
    const auto textures = TellerEngine::Extract::ReadTextureTable(
        bytes, "one.win", *table->Find("TXTR"));
    REQUIRE(textures.has_value());

    const auto blob =
        TellerEngine::Extract::ReadTexturePng(bytes, "one.win", textures->pages[0]);
    REQUIRE(blob.has_value());
    REQUIRE(blob->size() == png.size());

    const std::string copied(reinterpret_cast<const char *>(blob->data()),
                             blob->size());
    CHECK(copied == png);
}

TEST_CASE("PNGでない実体は拒否する") {
    std::string notPng(64, 'z');
    const auto directory = WriteSample(MakeForm(MakeTxtr({notPng})), "bad.win");
    const TellerEngine::Extract::FileBytes bytes(directory);

    const auto table = TellerEngine::Extract::ReadChunkTable(bytes, "bad.win");
    REQUIRE(table.has_value());
    const auto textures = TellerEngine::Extract::ReadTextureTable(
        bytes, "bad.win", *table->Find("TXTR"));
    REQUIRE_FALSE(textures.has_value());
    CHECK(textures.error().context.find("is not a PNG") != std::string::npos);
}

TEST_CASE("IENDが無ければ拒否する") {
    std::string png = "\x89PNG\r\n\x1a\n";
    std::string ihdr;
    AppendU32Be(ihdr, 8);
    AppendU32Be(ihdr, 8);
    ihdr += std::string(5, '\0');
    AppendPngChunk(png, "IHDR", ihdr);
    AppendPngChunk(png, "IDAT", std::string(4, 'x'));

    const auto directory = WriteSample(MakeForm(MakeTxtr({png})), "noend.win");
    const TellerEngine::Extract::FileBytes bytes(directory);

    const auto table = TellerEngine::Extract::ReadChunkTable(bytes, "noend.win");
    REQUIRE(table.has_value());
    const auto textures = TellerEngine::Extract::ReadTextureTable(
        bytes, "noend.win", *table->Find("TXTR"));
    REQUIRE_FALSE(textures.has_value());
    CHECK(textures.error().context.find("no IEND") != std::string::npos);
}

namespace {

std::string MakeTpag(const std::vector<std::vector<std::uint16_t>> &entries) {
    const auto count = static_cast<std::uint32_t>(entries.size());
    const std::uint32_t entriesBase = TxtrOffset + 4 + count * 4;

    std::string pointers;
    std::string bodies;
    for (std::uint32_t index = 0; index < count; ++index) {
        AppendU32(pointers, entriesBase + index * 22);
        for (const auto field : entries[index]) {
            bodies.push_back(static_cast<char>(field & 0xff));
            bodies.push_back(static_cast<char>((field >> 8) & 0xff));
        }
    }

    std::string chunk;
    AppendU32(chunk, count);
    chunk += pointers;
    chunk += bodies;
    return chunk;
}

std::string MakeTpagForm(const std::string &tpag) {
    std::string body = "TPAG";
    AppendU32(body, static_cast<std::uint32_t>(tpag.size()));
    body += tpag;

    std::string form = "FORM";
    AppendU32(form, static_cast<std::uint32_t>(body.size()));
    form += body;
    return form;
}

} // namespace

TEST_CASE("TPAGから矩形を読める") {
    const auto tpag = MakeTpag({{902, 826, 110, 42, 0, 0, 110, 42, 110, 42, 2},
                                {10, 20, 30, 40, 1, 2, 30, 40, 32, 44, 0}});
    const auto directory = WriteSample(MakeTpagForm(tpag), "regions.win");
    const TellerEngine::Extract::FileBytes bytes(directory);

    const auto table = TellerEngine::Extract::ReadChunkTable(bytes, "regions.win");
    REQUIRE(table.has_value());

    const auto regions = TellerEngine::Extract::ReadTextureRegions(
        bytes, "regions.win", *table->Find("TPAG"), 26);
    REQUIRE(regions.has_value());
    REQUIRE(regions->regions.size() == 2);

    CHECK(regions->regions[0].sourceX == 902);
    CHECK(regions->regions[0].sourceHeight == 42);
    CHECK(regions->regions[0].page == 2);
    CHECK(regions->regions[1].targetX == 1);
    CHECK(regions->regions[1].boundingHeight == 44);

    const std::uint64_t entriesBase = TxtrOffset + 4 + 2 * 4;
    const auto *found = regions->Find(entriesBase + 22);
    REQUIRE(found != nullptr);
    CHECK(found->sourceWidth == 30);
    CHECK(regions->Find(0) == nullptr);
}

TEST_CASE("存在しないページを指す矩形は拒否する") {
    const auto tpag = MakeTpag({{0, 0, 8, 8, 0, 0, 8, 8, 8, 8, 99}});
    const auto directory = WriteSample(MakeTpagForm(tpag), "page.win");
    const TellerEngine::Extract::FileBytes bytes(directory);

    const auto table = TellerEngine::Extract::ReadChunkTable(bytes, "page.win");
    REQUIRE(table.has_value());

    const auto regions = TellerEngine::Extract::ReadTextureRegions(
        bytes, "page.win", *table->Find("TPAG"), 26);
    REQUIRE_FALSE(regions.has_value());
    CHECK(regions.error().context.find("uses page 99") != std::string::npos);
}
