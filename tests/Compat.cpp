#include <Teller/Compat.h>

#include <doctest/doctest.h>

#include <array>
#include <string>

TEST_CASE("Expectedは値とエラーを持ち分ける") {
    Teller::Expected<int, std::string> ok = 42;
    REQUIRE(ok.has_value());
    CHECK(*ok == 42);

    Teller::Expected<int, std::string> ng =
        Teller::Unexpected<std::string>("失敗");
    REQUIRE_FALSE(ng.has_value());
    CHECK(ng.error() == "失敗");
}

TEST_CASE("Spanは連続した領域を参照する") {
    std::array<int, 3> values{1, 2, 3};
    Teller::Span<int> view{values};
    REQUIRE(view.size() == 3);
    CHECK(view[2] == 3);
}
