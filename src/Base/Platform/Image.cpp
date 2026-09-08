#include <Base/Files.hpp>
#include <Base/Platform/Image.hpp>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb_image.h>

namespace TellerEngine::Base::Platform {

Expected<Pixels, Error> DecodePng(Span<const std::byte> contents) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc *decoded =
        stbi_load_from_memory(reinterpret_cast<const stbi_uc *>(contents.data()),
                              static_cast<int>(contents.size()), &width, &height, &channels, 4);
    if (decoded == nullptr) {
        return Unexpected<Error>(Error{ErrorCode::Malformed, stbi_failure_reason()});
    }

    Pixels pixels;
    pixels.width = width;
    pixels.height = height;
    pixels.data.assign(decoded, decoded + static_cast<std::size_t>(width) * height * 4);
    stbi_image_free(decoded);
    return pixels;
}

Expected<Pixels, Error> LoadPng(const std::filesystem::path &path) {
    const auto contents = Files::ReadBytes(path);
    if (!contents) {
        return Unexpected<Error>(contents.error());
    }
    return DecodePng(Span<const std::byte>{*contents});
}

} // namespace TellerEngine::Base::Platform
