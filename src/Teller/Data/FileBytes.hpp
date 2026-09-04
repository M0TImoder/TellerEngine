#pragma once

// ディレクトリの中のファイルをバイト列として供給する
// 場所は宣言時に渡す
// エンジンが既定の場所を決めることはない

#include <Teller/Data/Bytes.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace Teller::Data {

class FileBytes : public Bytes {
public:
    explicit FileBytes(std::filesystem::path directory)
        : root(std::move(directory)) {}

    std::filesystem::path root;

    std::filesystem::path PathOf(std::string_view name) const {
        return root / std::filesystem::path(name);
    }

    bool Has(std::string_view name) const override {
        std::error_code ignored;
        return std::filesystem::is_regular_file(PathOf(name), ignored);
    }

    Expected<std::uint64_t, Error>
    SizeOf(std::string_view name) const override {
        const auto path = PathOf(name);
        std::error_code failure;
        const auto size = std::filesystem::file_size(path, failure);
        if (failure) {
            return Unexpected<Error>(Error{ErrorCode::NotFound, path.string()});
        }
        return static_cast<std::uint64_t>(size);
    }

    Expected<ByteBuffer, Error> Read(std::string_view name,
                                     std::uint64_t offset,
                                     std::uint64_t size) const override {
        const auto path = PathOf(name);

        const auto total = SizeOf(name);
        if (!total) {
            return Unexpected<Error>(total.error());
        }
        if (offset > *total || size > *total - offset) {
            return Unexpected<Error>(Error{
                ErrorCode::OutOfRange,
                path.string() + " offset=" + std::to_string(offset) + " size=" +
                    std::to_string(size) + " total=" + std::to_string(*total)});
        }

        std::ifstream stream(path, std::ios::binary);
        if (!stream) {
            return Unexpected<Error>(
                Error{ErrorCode::ReadFailed, path.string()});
        }

        ByteBuffer buffer(static_cast<std::size_t>(size));
        if (size > 0) {
            stream.seekg(static_cast<std::streamoff>(offset));
            stream.read(reinterpret_cast<char *>(buffer.data()),
                        static_cast<std::streamsize>(size));
            if (!stream) {
                return Unexpected<Error>(
                    Error{ErrorCode::ReadFailed, path.string()});
            }
        }
        return buffer;
    }
};

} // namespace Teller::Data
