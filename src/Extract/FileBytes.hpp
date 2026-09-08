#pragma once

// ディレクトリの中のファイルをバイト列として供給する

#include <Base/Compat.hpp>
#include <Base/Error.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace TellerEngine::Extract {

using ByteBuffer = std::vector<std::byte>;

class FileBytes {
public:
    explicit FileBytes(std::filesystem::path directory) : root(std::move(directory)) {}

    std::filesystem::path root;

    std::filesystem::path PathOf(std::string_view name) const {
        return root / std::filesystem::path(name);
    }

    bool Has(std::string_view name) const {
        std::error_code ignored;
        return std::filesystem::is_regular_file(PathOf(name), ignored);
    }

    Expected<std::uint64_t, Base::Error> SizeOf(std::string_view name) const {
        const auto path = PathOf(name);
        std::error_code failure;
        const auto size = std::filesystem::file_size(path, failure);
        if (failure) {
            return Unexpected<Base::Error>(
                Base::Error{Base::ErrorCode::NotFound, path.string()});
        }
        return static_cast<std::uint64_t>(size);
    }

    Expected<ByteBuffer, Base::Error> Read(std::string_view name, std::uint64_t offset,
                                           std::uint64_t size) const {
        const auto path = PathOf(name);

        const auto total = SizeOf(name);
        if (!total) {
            return Unexpected<Base::Error>(total.error());
        }
        if (offset > *total || size > *total - offset) {
            return Unexpected<Base::Error>(Base::Error{
                Base::ErrorCode::OutOfRange,
                path.string() + " offset=" + std::to_string(offset) +
                    " size=" + std::to_string(size) + " total=" + std::to_string(*total)});
        }

        std::ifstream stream(path, std::ios::binary);
        if (!stream) {
            return Unexpected<Base::Error>(
                Base::Error{Base::ErrorCode::ReadFailed, path.string()});
        }

        ByteBuffer buffer(static_cast<std::size_t>(size));
        if (size > 0) {
            stream.seekg(static_cast<std::streamoff>(offset));
            stream.read(reinterpret_cast<char *>(buffer.data()),
                        static_cast<std::streamsize>(size));
            if (!stream) {
                return Unexpected<Base::Error>(
                    Base::Error{Base::ErrorCode::ReadFailed, path.string()});
            }
        }
        return buffer;
    }

    Expected<ByteBuffer, Base::Error> ReadAll(std::string_view name) const {
        auto size = SizeOf(name);
        if (!size) {
            return Unexpected<Base::Error>(size.error());
        }
        return Read(name, 0, *size);
    }
};

} // namespace TellerEngine::Extract
