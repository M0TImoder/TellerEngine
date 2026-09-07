#include <Base/Math.hpp>

#include <doctest/doctest.h>

#include <cmath>

namespace Base = TellerEngine::Base;

TEST_CASE("丁度半分は偶数側へ丸める") {
    CHECK(Base::Round(0.5) == doctest::Approx(0.0));
    CHECK(Base::Round(1.5) == doctest::Approx(2.0));
    CHECK(Base::Round(2.5) == doctest::Approx(2.0));
    CHECK(Base::Round(3.5) == doctest::Approx(4.0));
    CHECK(Base::Round(4.5) == doctest::Approx(4.0));
}

TEST_CASE("負の側でも偶数へ丸める") {
    CHECK(Base::Round(-0.5) == doctest::Approx(0.0));
    CHECK(Base::Round(-1.5) == doctest::Approx(-2.0));
    CHECK(Base::Round(-2.5) == doctest::Approx(-2.0));
    CHECK(Base::Round(-3.5) == doctest::Approx(-4.0));
}

TEST_CASE("半分でなければ近い側へ丸める") {
    CHECK(Base::Round(2.4) == doctest::Approx(2.0));
    CHECK(Base::Round(2.6) == doctest::Approx(3.0));
    CHECK(Base::Round(-2.4) == doctest::Approx(-2.0));
    CHECK(Base::Round(-2.6) == doctest::Approx(-3.0));
    CHECK(Base::Round(7.0) == doctest::Approx(7.0));
}

TEST_CASE("標準の丸めとは結果が違う") {
    CHECK(std::round(2.5) == doctest::Approx(3.0));
    CHECK(Base::Round(2.5) == doctest::Approx(2.0));
    CHECK(std::round(-0.5) == doctest::Approx(-1.0));
    CHECK(Base::Round(-0.5) == doctest::Approx(0.0));
}

TEST_CASE("余りの符号は割られる側に従う") {
    CHECK(Base::Mod(7.0, 3.0) == doctest::Approx(1.0));
    CHECK(Base::Mod(-7.0, 3.0) == doctest::Approx(-1.0));
    CHECK(Base::Mod(7.5, 2.0) == doctest::Approx(1.5));
}

TEST_CASE("度と弧度を行き来できる") {
    CHECK(Base::DegToRad(180.0) == doctest::Approx(Base::kPi));
    CHECK(Base::RadToDeg(Base::kPi) == doctest::Approx(180.0));
    CHECK(Base::Sqr(4.0) == doctest::Approx(16.0));
}

TEST_CASE("向きは度で、y軸は下を向く") {
    CHECK(Base::LengthDirX(10.0, 0.0) == doctest::Approx(10.0));
    CHECK(Base::LengthDirY(10.0, 0.0) == doctest::Approx(0.0));

    CHECK(Base::LengthDirX(10.0, 90.0) == doctest::Approx(0.0));
    CHECK(Base::LengthDirY(10.0, 90.0) == doctest::Approx(-10.0));

    CHECK(Base::LengthDirX(10.0, 180.0) == doctest::Approx(-10.0));
    CHECK(Base::LengthDirY(10.0, 270.0) == doctest::Approx(10.0));
}

TEST_CASE("2点の向きは0以上360未満") {
    CHECK(Base::PointDirection(0.0, 0.0, 10.0, 0.0) == doctest::Approx(0.0));
    CHECK(Base::PointDirection(0.0, 0.0, 0.0, -10.0) == doctest::Approx(90.0));
    CHECK(Base::PointDirection(0.0, 0.0, -10.0, 0.0) == doctest::Approx(180.0));
    CHECK(Base::PointDirection(0.0, 0.0, 0.0, 10.0) == doctest::Approx(270.0));
    CHECK(Base::PointDirection(0.0, 0.0, 0.0, 0.0) == doctest::Approx(0.0));
}

TEST_CASE("向きと長さから戻ってこられる") {
    const double direction = Base::PointDirection(3.0, 4.0, 9.0, 12.0);
    const double distance = Base::PointDistance(3.0, 4.0, 9.0, 12.0);
    CHECK(distance == doctest::Approx(10.0));
    CHECK(3.0 + Base::LengthDirX(distance, direction) == doctest::Approx(9.0));
    CHECK(4.0 + Base::LengthDirY(distance, direction) == doctest::Approx(12.0));
}
