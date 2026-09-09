#pragma once

#include <cstddef>
#include <string_view>

namespace TellerEngine::Base {

// リテラルをテンプレート引数に載せる入れ物
template <std::size_t kSize> struct TextLiteral {
    char value[kSize]{};

    consteval TextLiteral(const char (&source)[kSize]) {
        for (std::size_t i = 0; i < kSize; ++i) {
            value[i] = source[i];
        }
    }

    constexpr std::string_view View() const { return std::string_view{value, kSize - 1}; }
};

} // namespace TellerEngine::Base
