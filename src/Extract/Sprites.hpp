#pragma once

// SPRTチャンクのスプライト定義
// 固定部60バイトのあとに、テクスチャ矩形へのポインタと当たり判定マスクが続く
// 実体は可変長で、マスクの末尾は4バイト境界に揃えられる

#include <Extract/FileBytes.hpp>
#include <Base/Error.hpp>
#include <Extract/Chunks.hpp>
#include <Extract/Reader.hpp>
#include <Extract/Strings.hpp>
#include <Extract/Textures.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace TellerEngine::Extract {

inline constexpr std::uint64_t SpriteFixedSize = 60;

struct Sprite {
    std::string name;

    std::uint32_t width = 0;
    std::uint32_t height = 0;

    // 余白を落とした結果の縁
    std::int32_t marginLeft = 0;
    std::int32_t marginRight = 0;
    std::int32_t marginBottom = 0;
    std::int32_t marginTop = 0;

    bool transparent = false;
    bool smooth = false;
    bool preload = false;

    std::uint32_t boundingBoxMode = 0;
    std::uint32_t separateMasks = 0;

    // 描画の基準点
    std::int32_t originX = 0;
    std::int32_t originY = 0;

    // TPAGの実体を指す
    std::vector<std::uint64_t> frames;

    // 当たり判定マスク
    // 1行あたりmaskStrideバイト、それがheight行、それがmaskCount枚
    std::uint32_t maskCount = 0;
    std::uint64_t maskOffset = 0;
    std::uint64_t maskStride = 0;
};

struct SpriteTable {
    std::vector<Sprite> sprites;
};

// regionsはTPAGの読み取り結果
// フレームが実在する矩形を指しているかを確かめるために受け取る
inline Expected<SpriteTable, TellerEngine::Base::Error>
ReadSprites(const TellerEngine::Extract::FileBytes &bytes, std::string_view name,
            const Chunk &chunk, const StringTable &strings,
            const TextureRegionTable &regions) {
    const auto contents = bytes.Read(name, chunk.offset, chunk.size);
    if (!contents) {
        return Unexpected<TellerEngine::Base::Error>(contents.error());
    }
    const Span<const std::byte> view(*contents);

    if (view.size() < 4) {
        return Unexpected<TellerEngine::Base::Error>(
            Malformed("SPRT is smaller than its count field"));
    }

    const std::uint64_t count = ReadU32(view, 0);
    if (4 + count * 4 > view.size()) {
        return Unexpected<TellerEngine::Base::Error>(Malformed(
            "SPRT declares " + std::to_string(count) + " sprites but is only " +
            std::to_string(view.size()) + " bytes"));
    }

    SpriteTable table;
    table.sprites.reserve(static_cast<std::size_t>(count));

    for (std::uint64_t index = 0; index < count; ++index) {
        const std::uint64_t pointer =
            ReadU32(view, static_cast<std::size_t>(4 + index * 4));
        if (pointer < chunk.offset ||
            pointer - chunk.offset + SpriteFixedSize + 4 > view.size()) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed("SPRT entry " + std::to_string(index) +
                          " points outside the chunk"));
        }
        const auto at = static_cast<std::size_t>(pointer - chunk.offset);

        Sprite sprite;
        auto text = ResolveString(strings, ReadU32(view, at), "SPRT name");
        if (!text) {
            return Unexpected<TellerEngine::Base::Error>(text.error());
        }
        sprite.name = std::move(*text);

        sprite.width = ReadU32(view, at + 4);
        sprite.height = ReadU32(view, at + 8);
        sprite.marginLeft = static_cast<std::int32_t>(ReadU32(view, at + 12));
        sprite.marginRight = static_cast<std::int32_t>(ReadU32(view, at + 16));
        sprite.marginBottom = static_cast<std::int32_t>(ReadU32(view, at + 20));
        sprite.marginTop = static_cast<std::int32_t>(ReadU32(view, at + 24));
        sprite.transparent = ReadU32(view, at + 28) != 0;
        sprite.smooth = ReadU32(view, at + 32) != 0;
        sprite.preload = ReadU32(view, at + 36) != 0;
        sprite.boundingBoxMode = ReadU32(view, at + 40);
        sprite.separateMasks = ReadU32(view, at + 44);
        sprite.originX = static_cast<std::int32_t>(ReadU32(view, at + 48));
        sprite.originY = static_cast<std::int32_t>(ReadU32(view, at + 52));

        const std::int32_t marker = ReadI32(view, at + 56);
        if (marker == -1) {
            return Unexpected<TellerEngine::Base::Error>(Malformed(
                sprite.name +
                " is a special sprite type which is not implemented"));
        }
        const std::uint64_t frameCount = static_cast<std::uint64_t>(marker);
        if (at + SpriteFixedSize + frameCount * 4 + 4 > view.size()) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed("SPRT entry " + std::to_string(index) +
                          " runs past the end of the chunk"));
        }

        sprite.frames.reserve(static_cast<std::size_t>(frameCount));
        for (std::uint64_t frame = 0; frame < frameCount; ++frame) {
            const std::uint64_t framePointer =
                ReadU32(view, static_cast<std::size_t>(at + SpriteFixedSize +
                                                       frame * 4));
            if (regions.Find(framePointer) == nullptr) {
                return Unexpected<TellerEngine::Base::Error>(
                    Malformed(sprite.name + " frame " + std::to_string(frame) +
                              " points to " + std::to_string(framePointer) +
                              " which is not in TPAG"));
            }
            sprite.frames.push_back(framePointer);
        }

        const std::uint64_t maskAt = at + SpriteFixedSize + frameCount * 4;
        sprite.maskCount = ReadU32(view, static_cast<std::size_t>(maskAt));
        sprite.maskStride = (sprite.width + 7) / 8;
        sprite.maskOffset = chunk.offset + maskAt + 4;

        const std::uint64_t maskBytes =
            sprite.maskStride * sprite.height * sprite.maskCount;
        if (maskAt + 4 + maskBytes > view.size()) {
            return Unexpected<TellerEngine::Base::Error>(Malformed(
                sprite.name + " has " + std::to_string(sprite.maskCount) +
                " masks which run past the end of the chunk"));
        }

        table.sprites.push_back(std::move(sprite));
    }

    return table;
}

// マスク1枚を生のビット列として取り出す
// 1行あたりmaskStrideバイト、1ビットが1ピクセル
inline Expected<TellerEngine::Extract::ByteBuffer,
                TellerEngine::Base::Error>
ReadSpriteMask(const TellerEngine::Extract::FileBytes &bytes,
               std::string_view name, const Sprite &sprite,
               std::uint32_t index) {
    if (index >= sprite.maskCount) {
        return Unexpected<TellerEngine::Base::Error>(
            TellerEngine::Base::Error{
                TellerEngine::Base::ErrorCode::OutOfRange,
                sprite.name + " has " + std::to_string(sprite.maskCount) +
                    " masks"});
    }
    const std::uint64_t size = sprite.maskStride * sprite.height;
    return bytes.Read(name, sprite.maskOffset + size * index, size);
}

} // namespace TellerEngine::Extract
