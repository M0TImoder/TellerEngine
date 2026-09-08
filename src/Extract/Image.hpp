#pragma once

// テクスチャページから矩形を切り出してPNGとして書き出す

#include <Base/Compat.hpp>
#include <Base/Error.hpp>
#include <Extract/FileBytes.hpp>
#include <Extract/Textures.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct TellerStbImage;

namespace TellerEngine::Extract {

// 復号したテクスチャページ
// 1画素4バイトのRGBA
struct Image {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> pixels;

    bool Contains(std::uint32_t x, std::uint32_t y, std::uint32_t w,
                  std::uint32_t h) const {
        return static_cast<std::uint64_t>(x) + w <= width &&
               static_cast<std::uint64_t>(y) + h <= height;
    }
};

Expected<Image, Base::Error> DecodePng(Span<const std::byte> contents);

// 矩形をそのまま切り出す
Expected<Image, Base::Error> CropImage(const Image &source, std::uint32_t x,
                                       std::uint32_t y, std::uint32_t width,
                                       std::uint32_t height);

// 切り出した絵を、元の大きさの中の正しい位置に置き直す
// TPAG が持つ target と bounding を使う
Expected<Image, Base::Error> PlaceInBounds(const Image &source,
                                           const TextureRegion &region);

// テクスチャアトラスの隙間の色を消す
void ClearTransparentColor(Image &image);

Expected<void, Base::Error> WritePng(const Image &image,
                                     const std::filesystem::path &path);

} // namespace TellerEngine::Extract
