#pragma once

// 抽出済みアセットの置き場所を持ち、相対位置を解決する
// 生成された一覧がこれを継承して、アセット名をメンバとして生やす

#include <Base/AssetLocation.hpp>
#include <Base/Compat.hpp>
#include <Base/Error.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace TellerEngine::Base {

class Assets {
public:
    explicit Assets(std::filesystem::path root) : root(std::move(root)) {}
    virtual ~Assets() = default;

    // 抽出先
    // 宣言時に一度だけ渡す
    std::filesystem::path root;

    // 相対位置を解決する
    virtual std::filesystem::path PathOf(AssetLocation location) const {
        return root / std::filesystem::path(location.relative);
    }

    // スプライトのコマは定義ファイルと同じ名前のフォルダに0.pngから並ぶ
    virtual std::filesystem::path FrameOf(AssetLocation location,
                                          std::size_t frame) const {
        auto directory = PathOf(location);
        directory.replace_extension();
        return directory / (std::to_string(frame) + ".png");
    }

    virtual bool Has(AssetLocation location) const {
        std::error_code ignored;
        const auto path = PathOf(location);
        return std::filesystem::exists(path, ignored);
    }

    // 生のパスを書かせないための内部用
    virtual Expected<std::vector<std::byte>, Error>
    Read(const std::filesystem::path &path) const {
        std::error_code failure;
        const auto size = std::filesystem::file_size(path, failure);
        if (failure) {
            return Unexpected<Error>(Error{ErrorCode::NotFound, path.string()});
        }

        std::ifstream stream(path, std::ios::binary);
        if (!stream) {
            return Unexpected<Error>(
                Error{ErrorCode::ReadFailed, path.string()});
        }

        std::vector<std::byte> buffer(static_cast<std::size_t>(size));
        if (size > 0) {
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

} // namespace TellerEngine::Base
