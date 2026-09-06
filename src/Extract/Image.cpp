// stb はヘッダ1枚に実装が入るので、この翻訳単位だけで展開する

#include <Extract/Image.hpp>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#include <cstring>

namespace TellerEngine::Extract {

Expected<Image, Base::Error> DecodePng(Span<const std::byte> contents) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc *decoded = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc *>(contents.data()),
        static_cast<int>(contents.size()), &width, &height, &channels, 4);
    if (decoded == nullptr) {
        return Unexpected<Base::Error>(Base::Error{
            Base::ErrorCode::Malformed,
            std::string("PNG decode failed: ") +
                (stbi_failure_reason() != nullptr ? stbi_failure_reason()
                                                  : "unknown")});
    }

    Image image;
    image.width = static_cast<std::uint32_t>(width);
    image.height = static_cast<std::uint32_t>(height);
    image.pixels.resize(static_cast<std::size_t>(width) * height * 4);
    std::memcpy(image.pixels.data(), decoded, image.pixels.size());
    stbi_image_free(decoded);
    return image;
}

Expected<Image, Base::Error> CropImage(const Image &source, std::uint32_t x,
                                       std::uint32_t y, std::uint32_t width,
                                       std::uint32_t height) {
    if (!source.Contains(x, y, width, height)) {
        return Unexpected<Base::Error>(Base::Error{
            Base::ErrorCode::OutOfRange,
            "crop (" + std::to_string(x) + "," + std::to_string(y) + "," +
                std::to_string(width) + "x" + std::to_string(height) +
                ") is outside " + std::to_string(source.width) + "x" +
                std::to_string(source.height)});
    }

    Image cropped;
    cropped.width = width;
    cropped.height = height;
    cropped.pixels.resize(static_cast<std::size_t>(width) * height * 4);
    for (std::uint32_t row = 0; row < height; ++row) {
        const auto from =
            (static_cast<std::size_t>(y + row) * source.width + x) * 4;
        const auto to = static_cast<std::size_t>(row) * width * 4;
        std::memcpy(cropped.pixels.data() + to, source.pixels.data() + from,
                    static_cast<std::size_t>(width) * 4);
    }
    return cropped;
}

Expected<Image, Base::Error> PlaceInBounds(const Image &source,
                                           const TextureRegion &region) {
    Image placed;
    placed.width = region.boundingWidth;
    placed.height = region.boundingHeight;
    placed.pixels.assign(
        static_cast<std::size_t>(placed.width) * placed.height * 4, 0);

    if (!placed.Contains(region.targetX, region.targetY, source.width,
                         source.height)) {
        return Unexpected<Base::Error>(
            Base::Error{Base::ErrorCode::OutOfRange,
                        "target (" + std::to_string(region.targetX) + "," +
                            std::to_string(region.targetY) + ") + " +
                            std::to_string(source.width) + "x" +
                            std::to_string(source.height) +
                            " does not fit in " + std::to_string(placed.width) +
                            "x" + std::to_string(placed.height)});
    }

    for (std::uint32_t row = 0; row < source.height; ++row) {
        const auto from = static_cast<std::size_t>(row) * source.width * 4;
        const auto to =
            (static_cast<std::size_t>(region.targetY + row) * placed.width +
             region.targetX) *
            4;
        std::memcpy(placed.pixels.data() + to, source.pixels.data() + from,
                    static_cast<std::size_t>(source.width) * 4);
    }
    return placed;
}

void ClearTransparentColor(Image &image) {
    for (std::size_t at = 0; at + 3 < image.pixels.size(); at += 4) {
        if (image.pixels[at + 3] == 0) {
            image.pixels[at + 0] = 0;
            image.pixels[at + 1] = 0;
            image.pixels[at + 2] = 0;
        }
    }
}

Expected<void, Base::Error> WritePng(const Image &image,
                                     const std::filesystem::path &path) {
    std::error_code failure;
    std::filesystem::create_directories(path.parent_path(), failure);

    const int written =
        stbi_write_png(path.string().c_str(), static_cast<int>(image.width),
                       static_cast<int>(image.height), 4, image.pixels.data(),
                       static_cast<int>(image.width) * 4);
    if (written == 0) {
        return Unexpected<Base::Error>(Base::Error{
            Base::ErrorCode::ReadFailed, "PNG write failed: " + path.string()});
    }
    return {};
}

} // namespace TellerEngine::Extract
