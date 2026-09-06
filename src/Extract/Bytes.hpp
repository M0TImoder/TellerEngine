#pragma once

// 名前付きのバイト列を供給する口

#include <Base/Compat.hpp>
#include <Base/Error.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace TellerEngine::Extract {

using ByteBuffer = std::vector<std::byte>;

class Bytes {
public:
    virtual ~Bytes() = default;

    virtual bool Has(std::string_view name) const = 0;

    virtual Expected<std::uint64_t, Base::Error>
    SizeOf(std::string_view name) const = 0;

    virtual Expected<ByteBuffer, Base::Error> Read(std::string_view name,
                                             std::uint64_t offset,
                                             std::uint64_t size) const = 0;

    // 全体を読む
    virtual Expected<ByteBuffer, Base::Error> ReadAll(std::string_view name) const {
        auto size = SizeOf(name);
        if (!size) {
            return Unexpected<Base::Error>(size.error());
        }
        return Read(name, 0, *size);
    }
};

} // namespace TellerEngineEngine::Base::Data
