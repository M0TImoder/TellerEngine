#pragma once

// 抽出済みアセットの置き場所を持ち、相対位置を解決する
// 生成された一覧がこれを継承して、アセット名をメンバとして生やす

#include <Base/AssetLocation.hpp>
#include <Base/Compat.hpp>
#include <Base/Error.hpp>
#include <Base/Files.hpp>

#include <cstddef>
#include <filesystem>
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

    virtual bool Has(AssetLocation location) const { return Files::Exists(PathOf(location)); }

    // 生のパスを書かせないための内部用
    virtual Expected<std::vector<std::byte>, Error>
    Read(const std::filesystem::path &path) const {
        return Files::ReadBytes(path);
    }
};

} // namespace TellerEngine::Base
