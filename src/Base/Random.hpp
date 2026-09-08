#pragma once

// WELL512a
// 状態は16語、種はLCGを16回回して展開する

#include <array>
#include <cstdint>
#include <initializer_list>

namespace TellerEngine::Base {

// 種を状態へ展開するときの丸め方
enum class SeedMode {
    Signed31,
    Bits15,
    Bits16,
};

class Random {
public:
    struct Snapshot {
        std::array<std::uint32_t, 16> words{};
        std::uint32_t index = 0;
        std::uint32_t seed = 0;
        std::uint64_t draws = 0;
    };

    constexpr explicit Random(std::uint32_t seed = 0, SeedMode mode = SeedMode::Signed31)
        : mode_(mode) {
        SetSeed(seed);
    }

    constexpr void SetSeed(std::uint32_t seed) {
        state_ = Snapshot{};
        state_.seed = seed;
        std::uint32_t value = seed;
        for (std::uint32_t &word : state_.words) {
            value = value * 0x343FDu + 0x269EC3u;
            switch (mode_) {
            case SeedMode::Signed31:
                value = static_cast<std::uint32_t>(static_cast<std::int32_t>(value) >> 16) &
                        0x7FFFFFFFu;
                break;
            case SeedMode::Bits15:
                value = (value >> 16) & 0x7FFFu;
                break;
            case SeedMode::Bits16:
                value = value >> 16;
                break;
            }
            word = value;
        }
    }

    constexpr std::uint32_t Seed() const { return state_.seed; }

    // 生の32ビットを1つ取り出す
    constexpr std::uint32_t Next() {
        std::array<std::uint32_t, 16> &words = state_.words;
        const std::uint32_t index = state_.index;

        std::uint32_t a = words[index];
        std::uint32_t b = words[(index + 13) & 15];
        const std::uint32_t c = a ^ b ^ (a << 16) ^ (b << 15);
        b = words[(index + 9) & 15];
        b ^= b >> 11;
        a = c ^ b;
        words[index] = a;
        const std::uint32_t d = a ^ ((a << 5) & kPolynomial);

        state_.index = (index + 15) & 15;
        a = words[state_.index];
        words[state_.index] = a ^ c ^ d ^ (a << 2) ^ (c << 18) ^ (b << 28);

        state_.draws += 1;
        return words[state_.index];
    }

    // 0以上1未満
    constexpr double Unit() { return static_cast<double>(Next()) * 0x1p-32; }

    // 上端を含まない
    constexpr double Real(double bound) { return Unit() * bound; }

    constexpr double RealRange(double low, double high) { return low + Real(high - low); }

    // 両端を含む
    constexpr std::int64_t Integer(std::int64_t bound) {
        return static_cast<std::int64_t>(Unit() * static_cast<double>(bound + 1));
    }

    constexpr std::int64_t IntegerRange(std::int64_t low, std::int64_t high) {
        return low + Integer(high - low);
    }

    template <typename T> constexpr T Choose(std::initializer_list<T> values) {
        if (values.size() == 0) {
            return T{};
        }
        const std::int64_t picked = Integer(static_cast<std::int64_t>(values.size()) - 1);
        return *(values.begin() + picked);
    }

    // 取り出した回数
    constexpr std::uint64_t Draws() const { return state_.draws; }

    constexpr SeedMode Mode() const { return mode_; }

    constexpr const Snapshot &Save() const { return state_; }
    constexpr void Restore(const Snapshot &snapshot) { state_ = snapshot; }

private:
    // WELL512aの多項式
    static constexpr std::uint32_t kPolynomial = 0xDA442D24u;

    Snapshot state_{};
    SeedMode mode_ = SeedMode::Signed31;
};

} // namespace TellerEngine::Base
