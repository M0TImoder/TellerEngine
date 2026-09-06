#pragma once

// TXTRチャンクのテクスチャページと、TPAGチャンクのその中の矩形
// 並びは u32の件数 / 件数ぶんのポインタ / 各実体
// 実体は u32の拡大フラグ / PNG本体へのポインタ
// PNG本体の長さは記録されていないので、PNGのチャンクをIENDまで辿って求める

#include <Extract/Bytes.hpp>
#include <Base/Error.hpp>
#include <Extract/Chunks.hpp>
#include <Extract/Reader.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace TellerEngine::Extract {

// 署名8バイトとIHDRの見出し8バイトの次に幅と高さが並ぶ
inline constexpr std::uint64_t PngSignatureSize = 8;
inline constexpr std::uint64_t PngChunkOverhead = 12;
inline constexpr std::uint64_t PngHeaderSize = 24;

struct TexturePage {
    std::uint32_t scaled = 0;
    std::uint64_t pngOffset = 0;
    std::uint64_t pngSize = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct TextureTable {
    std::vector<TexturePage> pages;
};

inline Expected<TextureTable, TellerEngine::Base::Error>
ReadTextureTable(const TellerEngine::Extract::Bytes &bytes,
                 std::string_view name, const Chunk &chunk) {
    const std::uint64_t chunkEnd = chunk.offset + chunk.size;

    const auto head = bytes.Read(name, chunk.offset, 4);
    if (!head) {
        return Unexpected<TellerEngine::Base::Error>(head.error());
    }
    const std::uint64_t count = ReadU32(Span<const std::byte>(*head), 0);

    if (4 + count * 4 > chunk.size) {
        return Unexpected<TellerEngine::Base::Error>(Malformed(
            "TXTR declares " + std::to_string(count) + " pages but is only " +
            std::to_string(chunk.size) + " bytes"));
    }

    const auto list = bytes.Read(name, chunk.offset + 4, count * 4);
    if (!list) {
        return Unexpected<TellerEngine::Base::Error>(list.error());
    }
    const Span<const std::byte> listView(*list);

    TextureTable table;
    table.pages.reserve(static_cast<std::size_t>(count));

    for (std::uint64_t index = 0; index < count; ++index) {
        const std::uint64_t entryAt =
            ReadU32(listView, static_cast<std::size_t>(index * 4));
        if (entryAt < chunk.offset || entryAt + 8 > chunkEnd) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed("TXTR entry " + std::to_string(index) +
                          " points outside the chunk"));
        }

        const auto entry = bytes.Read(name, entryAt, 8);
        if (!entry) {
            return Unexpected<TellerEngine::Base::Error>(entry.error());
        }
        const Span<const std::byte> entryView(*entry);

        TexturePage page;
        page.scaled = ReadU32(entryView, 0);
        page.pngOffset = ReadU32(entryView, 4);

        if (page.pngOffset < chunk.offset ||
            page.pngOffset + PngHeaderSize > chunkEnd) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed("TXTR page " + std::to_string(index) +
                          " points outside the chunk"));
        }

        const auto header = bytes.Read(name, page.pngOffset, PngHeaderSize);
        if (!header) {
            return Unexpected<TellerEngine::Base::Error>(header.error());
        }
        const Span<const std::byte> headerView(*header);

        if (ReadMagic(headerView, 1) != "PNG\r") {
            return Unexpected<TellerEngine::Base::Error>(Malformed(
                "TXTR page " + std::to_string(index) + " is not a PNG"));
        }
        page.width = ReadU32Be(headerView, 16);
        page.height = ReadU32Be(headerView, 20);

        // IENDまでのチャンクを辿って長さを求める
        std::uint64_t cursor = page.pngOffset + PngSignatureSize;
        while (true) {
            if (cursor + PngChunkOverhead > chunkEnd) {
                return Unexpected<TellerEngine::Base::Error>(Malformed(
                    "TXTR page " + std::to_string(index) + " has no IEND"));
            }
            const auto pngChunk = bytes.Read(name, cursor, 8);
            if (!pngChunk) {
                return Unexpected<TellerEngine::Base::Error>(
                    pngChunk.error());
            }
            const Span<const std::byte> pngView(*pngChunk);
            const std::uint64_t length = ReadU32Be(pngView, 0);
            const std::string type = ReadMagic(pngView, 4);

            cursor += PngChunkOverhead + length;
            if (type == "IEND") {
                break;
            }
        }
        page.pngSize = cursor - page.pngOffset;

        table.pages.push_back(page);
    }

    return table;
}

inline Expected<TellerEngine::Extract::ByteBuffer,
                TellerEngine::Base::Error>
ReadTexturePng(const TellerEngine::Extract::Bytes &bytes,
               std::string_view name, const TexturePage &page) {
    return bytes.Read(name, page.pngOffset, page.pngSize);
}

// TPAGの1件は22バイト
inline constexpr std::uint64_t TextureRegionSize = 22;

// テクスチャページの中の矩形
// スプライトや背景やフォントがこれを指す
struct TextureRegion {
    std::uint16_t sourceX = 0;
    std::uint16_t sourceY = 0;
    std::uint16_t sourceWidth = 0;
    std::uint16_t sourceHeight = 0;
    std::uint16_t targetX = 0;
    std::uint16_t targetY = 0;
    std::uint16_t targetWidth = 0;
    std::uint16_t targetHeight = 0;
    std::uint16_t boundingWidth = 0;
    std::uint16_t boundingHeight = 0;

    std::int16_t page = 0;
};

struct TextureRegionTable {
    std::vector<TextureRegion> regions;

    // 他のチャンクは矩形を絶対位置で指すので、そこから添字を引けるようにする
    std::unordered_map<std::uint64_t, std::size_t> byPointer;

    const TextureRegion *Find(std::uint64_t pointer) const {
        const auto found = byPointer.find(pointer);
        if (found == byPointer.end()) {
            return nullptr;
        }
        return &regions[found->second];
    }
};

// pageCountはTXTRのページ数
// 矩形が存在しないページを指していないかを確かめるために受け取る
inline Expected<TextureRegionTable, TellerEngine::Base::Error>
ReadTextureRegions(const TellerEngine::Extract::Bytes &bytes,
                   std::string_view name, const Chunk &chunk,
                   std::size_t pageCount) {
    const auto contents = bytes.Read(name, chunk.offset, chunk.size);
    if (!contents) {
        return Unexpected<TellerEngine::Base::Error>(contents.error());
    }
    const Span<const std::byte> view(*contents);

    if (view.size() < 4) {
        return Unexpected<TellerEngine::Base::Error>(
            Malformed("TPAG is smaller than its count field"));
    }

    const std::uint64_t count = ReadU32(view, 0);
    if (4 + count * 4 > view.size()) {
        return Unexpected<TellerEngine::Base::Error>(Malformed(
            "TPAG declares " + std::to_string(count) + " regions but is only " +
            std::to_string(view.size()) + " bytes"));
    }

    TextureRegionTable table;
    table.regions.reserve(static_cast<std::size_t>(count));
    table.byPointer.reserve(static_cast<std::size_t>(count));

    for (std::uint64_t index = 0; index < count; ++index) {
        const std::uint64_t pointer =
            ReadU32(view, static_cast<std::size_t>(4 + index * 4));
        if (pointer < chunk.offset ||
            pointer - chunk.offset + TextureRegionSize > view.size()) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed("TPAG entry " + std::to_string(index) +
                          " points outside the chunk"));
        }

        const auto at = static_cast<std::size_t>(pointer - chunk.offset);

        TextureRegion region;
        region.sourceX = ReadU16(view, at + 0);
        region.sourceY = ReadU16(view, at + 2);
        region.sourceWidth = ReadU16(view, at + 4);
        region.sourceHeight = ReadU16(view, at + 6);
        region.targetX = ReadU16(view, at + 8);
        region.targetY = ReadU16(view, at + 10);
        region.targetWidth = ReadU16(view, at + 12);
        region.targetHeight = ReadU16(view, at + 14);
        region.boundingWidth = ReadU16(view, at + 16);
        region.boundingHeight = ReadU16(view, at + 18);
        region.page = ReadI16(view, at + 20);

        if (region.page < 0 ||
            static_cast<std::size_t>(region.page) >= pageCount) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed("TPAG entry " + std::to_string(index) +
                          " uses page " + std::to_string(region.page) +
                          " but there are only " + std::to_string(pageCount)));
        }

        table.byPointer.emplace(pointer, table.regions.size());
        table.regions.push_back(region);
    }

    return table;
}

} // namespace TellerEngine::Extract
