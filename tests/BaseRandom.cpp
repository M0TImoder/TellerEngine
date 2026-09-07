#include <Base/Random.hpp>

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <vector>

using TellerEngine::Base::Random;
using TellerEngine::Base::SeedMode;

namespace {

std::vector<std::uint32_t> Take(Random &random, std::size_t count) {
    std::vector<std::uint32_t> values;
    values.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        values.push_back(random.Next());
    }
    return values;
}

} // namespace

TEST_CASE("Signed31の並びが一致する") {
    Random zero{0, SeedMode::Signed31};
    CHECK(Take(zero, 6) == std::vector<std::uint32_t>{0xCDC05DCFu, 0xF6780AEFu, 0x41457DF1u,
                                                      0xA5BC7F31u, 0xE07406E7u, 0x89D19EA2u});

    Random other{12345, SeedMode::Signed31};
    CHECK(Take(other, 6) == std::vector<std::uint32_t>{0x6645F496u, 0x2034EB28u, 0x82082B01u,
                                                       0x7A69ECF3u, 0x6CE1A31Eu, 0x0AB9B14Fu});
}

TEST_CASE("Bits15の並びが一致する") {
    Random zero{0, SeedMode::Bits15};
    CHECK(Take(zero, 6) == std::vector<std::uint32_t>{0x27C5F82Du, 0x15B1730Fu, 0xB48C6B74u,
                                                      0x6DE81C14u, 0xA6E448CDu, 0x23381BA2u});

    Random other{12345, SeedMode::Bits15};
    CHECK(Take(other, 6) == std::vector<std::uint32_t>{0xD240F5C2u, 0x6FB53C88u, 0xE7913336u,
                                                       0x41100C7Cu, 0xF628D27Bu, 0xC0685223u});
}

TEST_CASE("Bits16の並びが一致する") {
    Random zero{0, SeedMode::Bits16};
    CHECK(Take(zero, 6) == std::vector<std::uint32_t>{0xCF0C5FECu, 0xE6E40FCEu, 0x4D8D590Eu,
                                                      0xEE447205u, 0x8CA676E8u, 0x53EA9F82u});

    Random other{12345, SeedMode::Bits16};
    CHECK(Take(other, 6) == std::vector<std::uint32_t>{0x9F2E1BC8u, 0xEA6D9B00u, 0x3610B680u,
                                                       0x57740F7Fu, 0x855F164Bu, 0x7C933449u});
}

TEST_CASE("丸め方が変われば並びも変わる") {
    Random signed31{7, SeedMode::Signed31};
    Random bits15{7, SeedMode::Bits15};
    Random bits16{7, SeedMode::Bits16};
    CHECK(Take(signed31, 4) != Take(bits15, 4));
    CHECK(Take(bits15, 4) != Take(bits16, 4));
}

TEST_CASE("同じ種からは同じ並びが出る") {
    Random first{999};
    Random second{999};
    CHECK(Take(first, 32) == Take(second, 32));
}

TEST_CASE("種を入れ直すと最初から始まる") {
    Random random{4};
    const std::vector<std::uint32_t> head = Take(random, 5);
    Take(random, 20);
    random.SetSeed(4);
    CHECK(Take(random, 5) == head);
    CHECK(random.Seed() == 4);
}

TEST_CASE("状態を保存して戻すと続きが一致する") {
    Random random{2468};
    Take(random, 37);

    const Random::Snapshot saved = random.Save();
    const std::vector<std::uint32_t> expected = Take(random, 16);

    Take(random, 100);
    random.Restore(saved);
    CHECK(Take(random, 16) == expected);
}

TEST_CASE("取り出した回数を数える") {
    Random random{1};
    CHECK(random.Draws() == 0);
    Take(random, 9);
    CHECK(random.Draws() == 9);

    random.Real(10.0);
    random.Integer(5);
    random.Choose({1, 2, 3});
    CHECK(random.Draws() == 12);

    random.SetSeed(1);
    CHECK(random.Draws() == 0);
}

TEST_CASE("Realは上端を含まない") {
    Random random{31337};
    for (int i = 0; i < 4096; ++i) {
        const double value = random.Real(10.0);
        CHECK(value >= 0.0);
        CHECK(value < 10.0);
    }
}

TEST_CASE("Integerは両端を含む") {
    Random random{555};
    std::array<int, 6> seen{};
    for (int i = 0; i < 8192; ++i) {
        const std::int64_t value = random.Integer(5);
        REQUIRE(value >= 0);
        REQUIRE(value <= 5);
        seen[static_cast<std::size_t>(value)] += 1;
    }
    for (const int count : seen) {
        CHECK(count > 0);
    }
}

TEST_CASE("Chooseは渡した値のどれかを返す") {
    Random random{88};
    std::array<int, 3> seen{};
    for (int i = 0; i < 1024; ++i) {
        const int value = random.Choose({10, 20, 30});
        REQUIRE((value == 10 || value == 20 || value == 30));
        seen[static_cast<std::size_t>(value / 10 - 1)] += 1;
    }
    for (const int count : seen) {
        CHECK(count > 0);
    }
}

TEST_CASE("コンパイル時にも回る") {
    constexpr std::uint32_t first = [] {
        Random random{0, SeedMode::Signed31};
        return random.Next();
    }();
    static_assert(first == 0xCDC05DCFu);
    CHECK(first == 0xCDC05DCFu);
}
