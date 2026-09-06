#pragma once

// アセットの名前と抽出先からの相対位置
// 生成された一覧がこの型で並ぶ

#include <string_view>

namespace TellerEngine::Base {

struct AssetLocation {
    std::string_view name;
    std::string_view relative;
};

} // namespace TellerEngine::Base
