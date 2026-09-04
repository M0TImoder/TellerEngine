#pragma once

// 将来の標準で置き換わるものを薄い別名の裏に隠す
// 実体は機能検査マクロで自動的に切り替わる

#include <cstddef>
#include <span>
#include <version>

#if defined(__cpp_lib_expected)
#include <expected>
#else
#include <tl/expected.hpp>
#endif

namespace Teller {

#if defined(__cpp_lib_expected)

template <typename T, typename E> using Expected = std::expected<T, E>;

template <typename E> using Unexpected = std::unexpected<E>;

#else

template <typename T, typename E> using Expected = tl::expected<T, E>;

template <typename E> using Unexpected = tl::unexpected<E>;

#endif

template <typename T, std::size_t Extent = std::dynamic_extent>
using Span = std::span<T, Extent>;

} // namespace Teller
