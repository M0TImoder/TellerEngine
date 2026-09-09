#pragma once

#include <cstddef>

namespace TellerEngine::Base {

enum class MarkupProblem {
    None,

    // 構文が閉じられていない
    Unclosed,

    // 不明な名前
    UnknownName,

    // 引数が必要
    MissingArgument,

    // 範囲が必要
    MissingRange,

    // 引数の形式が不正
    BadArgument,
};

// 記法の誤りとその位置
struct MarkupDiagnostic {
    MarkupProblem problem = MarkupProblem::None;

    // 誤りのある位置
    std::size_t at = 0;

    constexpr explicit operator bool() const { return problem == MarkupProblem::None; }
};

} // namespace TellerEngine::Base
