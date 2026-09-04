#pragma once

// 名前付きのバイト列を供給する口
// エンジンは既定の場所を持たない
// ばらのファイルでも、Modのディレクトリでも、WASMのプリロードFSでも、同じ口に刺さる

#include <Teller/Compat.hpp>
#include <Teller/Data/Error.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace Teller::Data {

using ByteBuffer = std::vector<std::byte>;

class Bytes {
public:
    virtual ~Bytes() = default;

    virtual bool Has(std::string_view name) const = 0;

    virtual Expected<std::uint64_t, Error>
    SizeOf(std::string_view name) const = 0;

    virtual Expected<ByteBuffer, Error> Read(std::string_view name,
                                             std::uint64_t offset,
                                             std::uint64_t size) const = 0;

    // 全体を読む
    virtual Expected<ByteBuffer, Error> ReadAll(std::string_view name) const {
        auto size = SizeOf(name);
        if (!size) {
            return Unexpected<Error>(size.error());
        }
        return Read(name, 0, *size);
    }
};

} // namespace Teller::Data
