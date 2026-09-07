#pragma once

#include <Base/Compat.hpp>
#include <Base/Error.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace TellerEngine::Base::Files {

inline bool Exists(const std::filesystem::path &path) {
    std::error_code ignored;
    return std::filesystem::is_regular_file(path, ignored);
}

inline Expected<void, Error> EnsureDirectory(const std::filesystem::path &path) {
    std::error_code failure;
    std::filesystem::create_directories(path, failure);
    if (failure && !std::filesystem::is_directory(path)) {
        return Unexpected<Error>(Error{ErrorCode::Unavailable, path.string()});
    }
    return {};
}

inline Expected<std::vector<std::byte>, Error> ReadBytes(const std::filesystem::path &path) {
    std::error_code failure;
    const auto size = std::filesystem::file_size(path, failure);
    if (failure) {
        return Unexpected<Error>(Error{ErrorCode::NotFound, path.string()});
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return Unexpected<Error>(Error{ErrorCode::ReadFailed, path.string()});
    }

    std::vector<std::byte> buffer(static_cast<std::size_t>(size));
    if (size > 0) {
        stream.read(reinterpret_cast<char *>(buffer.data()),
                    static_cast<std::streamsize>(size));
        if (!stream) {
            return Unexpected<Error>(Error{ErrorCode::ReadFailed, path.string()});
        }
    }
    return buffer;
}

inline Expected<std::string, Error> ReadText(const std::filesystem::path &path) {
    const auto bytes = ReadBytes(path);
    if (!bytes) {
        return Unexpected<Error>(bytes.error());
    }
    return std::string(reinterpret_cast<const char *>(bytes->data()), bytes->size());
}

// 書き込み中に落ちても元のファイルが壊れない
// 別名で書き切ってから置き換える
inline Expected<void, Error> WriteAtomic(const std::filesystem::path &path,
                                         Span<const std::byte> content) {
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        const auto ready = EnsureDirectory(parent);
        if (!ready) {
            return Unexpected<Error>(ready.error());
        }
    }

    std::filesystem::path temporary = path;
    temporary += ".writing";

    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) {
            return Unexpected<Error>(Error{ErrorCode::Unavailable, temporary.string()});
        }
        if (!content.empty()) {
            stream.write(reinterpret_cast<const char *>(content.data()),
                         static_cast<std::streamsize>(content.size()));
        }
        stream.flush();
        if (!stream) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return Unexpected<Error>(Error{ErrorCode::Unavailable, temporary.string()});
        }
    }

    std::error_code failure;
    std::filesystem::rename(temporary, path, failure);
    if (failure) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return Unexpected<Error>(Error{ErrorCode::Unavailable, path.string()});
    }
    return {};
}

inline Expected<void, Error> WriteAtomicText(const std::filesystem::path &path,
                                             std::string_view content) {
    return WriteAtomic(path, Span<const std::byte>{
                                 reinterpret_cast<const std::byte *>(content.data()),
                                 content.size()});
}

inline bool Remove(const std::filesystem::path &path) {
    std::error_code ignored;
    return std::filesystem::remove(path, ignored);
}

} // namespace TellerEngine::Base::Files
