#pragma once

#include <Base/Compat.hpp>
#include <Base/Error.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace TellerEngine::Base::Platform {

// 1画素4バイトのRGBA
struct Pixels {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> data;
};

Expected<Pixels, Error> DecodePng(Span<const std::byte> contents);

Expected<Pixels, Error> LoadPng(const std::filesystem::path &path);

} // namespace TellerEngine::Base::Platform
