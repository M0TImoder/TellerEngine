#pragma once

#include <Base/MarkupDiagnostic.hpp>
#include <Base/Text.hpp>
#include <Base/TextLiteral.hpp>
#include <Teller/Markup.hpp>
#include <Vanilla/LegacyMarkup.hpp>

namespace TellerEngine::Literals {

namespace Detail {

template <Base::MarkupDiagnostic kFound> constexpr void Check() {
    static_assert(kFound.problem != Base::MarkupProblem::Unclosed, "構文が閉じられていません");
    static_assert(kFound.problem != Base::MarkupProblem::UnknownName, "不明な名前です");
    static_assert(kFound.problem != Base::MarkupProblem::MissingArgument, "引数が必要です");
    static_assert(kFound.problem != Base::MarkupProblem::MissingRange, "範囲が必要です");
    static_assert(kFound.problem != Base::MarkupProblem::BadArgument, "引数の形式が不正です");
}

} // namespace Detail

template <Base::TextLiteral kSource> Base::Text operator""_legacy() {
    Detail::Check<Vanilla::LegacyMarkup::Diagnose(kSource.View())>();
    return Vanilla::Parse(kSource.View());
}

template <Base::TextLiteral kSource> Base::Text operator""_l() {
    return operator""_legacy<kSource>();
}

template <Base::TextLiteral kSource> Base::Text operator""_teller() {
    Detail::Check<Teller::Markup::Diagnose(kSource.View())>();
    return Teller::Parse(kSource.View());
}

template <Base::TextLiteral kSource> Base::Text operator""_t() {
    return operator""_teller<kSource>();
}

} // namespace TellerEngine::Literals
