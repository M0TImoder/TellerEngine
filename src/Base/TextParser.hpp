#pragma once

#include <Base/Text.hpp>

#include <string_view>

namespace TellerEngine::Base {

// 記法を中間表現へ変える
// 記法ごとに実装を差し替える
class TextParser {
public:
    virtual ~TextParser() = default;

    virtual Text Parse(std::string_view source) const = 0;
};

} // namespace TellerEngine::Base
