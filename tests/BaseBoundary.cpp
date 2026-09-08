#include <Base/Boundary.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace Base = TellerEngine::Base;

namespace {

double Length(double x, double y) { return std::hypot(x, y); }

double Travelled(Base::Position from, Base::Position to) {
    return Length(to.x - from.x, to.y - from.y);
}

} // namespace

TEST_CASE("矩形の内と外を分ける") {
    const Base::RectangleBoundary board{{0.0, 0.0}, 50.0, 30.0};

    CHECK(board.Contains({0.0, 0.0}));
    CHECK(board.Contains({49.0, 29.0}));
    CHECK_FALSE(board.Contains({51.0, 0.0}));
    CHECK_FALSE(board.Contains({0.0, 31.0}));
    CHECK(board.Distance({0.0, 0.0}) < 0.0);
    CHECK(board.Distance({60.0, 0.0}) == doctest::Approx(10.0));
}

TEST_CASE("円の内と外を分ける") {
    const Base::CircleBoundary board{{10.0, 10.0}, 20.0};

    CHECK(board.Contains({10.0, 10.0}));
    CHECK(board.Contains({29.0, 10.0}));
    CHECK_FALSE(board.Contains({31.0, 10.0}));
    CHECK(board.Distance({40.0, 10.0}) == doctest::Approx(10.0));
}

TEST_CASE("波打つ枠は場所によって縁が動く") {
    const Base::WaveBoundary board{{0.0, 0.0}, 100.0, 20.0, 10.0, 80.0};

    CHECK(board.Contains({0.0, 0.0}));
    // 山と谷で上の縁の高さが変わる
    const bool atPeak = board.Contains({20.0, -25.0});
    const bool atTrough = board.Contains({-20.0, -25.0});
    CHECK(atPeak != atTrough);
}

TEST_CASE("外の点は縁まで戻される") {
    const Base::CircleBoundary board{{0.0, 0.0}, 20.0};
    const Base::Position pushed = board.Project({100.0, 0.0});

    CHECK(board.Distance(pushed) == doctest::Approx(0.0).epsilon(0.01));
    CHECK(pushed.x == doctest::Approx(20.0).epsilon(0.01));
}

TEST_CASE("枠の中の移動はそのまま通る") {
    const Base::RectangleBoundary board{{0.0, 0.0}, 50.0, 30.0};
    const Base::Position moved = Base::Slide(board, {0.0, 0.0}, 3.0, 4.0);

    CHECK(moved.x == doctest::Approx(3.0));
    CHECK(moved.y == doctest::Approx(4.0));
}

TEST_CASE("壁に沿って動いても速さが変わらない") {
    const Base::CircleBoundary board{{0.0, 0.0}, 50.0};
    Base::Position soul{49.0, 0.0};

    int slid = 0;
    for (int step = 0; step < 30; ++step) {
        const Base::Position before = soul;
        soul = Base::Slide(board, soul, 4.0, 4.0, Base::SlideMode::Preserve);
        const double moved = Travelled(before, soul);
        CHECK(board.Distance(soul) <= 0.01);
        if (moved > 0.0) {
            CHECK(moved == doctest::Approx(Length(4.0, 4.0)).epsilon(0.05));
            slid += 1;
        }
    }
    CHECK(slid > 5);
}

TEST_CASE("壁を真正面から押すところまで滑ると止まる") {
    const Base::CircleBoundary board{{0.0, 0.0}, 50.0};
    Base::Position soul{49.0, 0.0};

    for (int step = 0; step < 40; ++step) {
        soul = Base::Slide(board, soul, 4.0, 4.0, Base::SlideMode::Preserve);
    }

    const Base::Position resting = soul;
    soul = Base::Slide(board, soul, 4.0, 4.0, Base::SlideMode::Preserve);
    CHECK(Travelled(resting, soul) == doctest::Approx(0.0));

    // 止まった先は押している向きの壁
    const Base::Position normal = board.Normal(soul);
    CHECK(normal.x == doctest::Approx(std::sqrt(0.5)).epsilon(0.1));
    CHECK(normal.y == doctest::Approx(std::sqrt(0.5)).epsilon(0.1));
}

TEST_CASE("波打つ縁でも速さが変わらない") {
    const Base::WaveBoundary board{{0.0, 0.0}, 200.0, 20.0, 12.0, 60.0};
    Base::Position soul{-100.0, -8.0};

    int slid = 0;
    for (int step = 0; step < 40; ++step) {
        const Base::Position before = soul;
        soul = Base::Slide(board, soul, 3.0, -3.0, Base::SlideMode::Preserve);
        const double moved = Travelled(before, soul);
        CHECK(board.Distance(soul) <= 0.01);
        if (moved > 0.0) {
            CHECK(moved == doctest::Approx(Length(3.0, 3.0)).epsilon(0.05));
            slid += 1;
        }
    }
    CHECK(slid > 5);
}

TEST_CASE("縁をなぞって動き続けても外へ出ない") {
    const Base::CircleBoundary board{{0.0, 0.0}, 50.0};
    Base::Position soul{0.0, 0.0};

    // 壁の向きに合わせて押す向きを変える
    for (int step = 0; step < 200; ++step) {
        const Base::Position normal = board.Normal(soul);
        const double pushX = normal.x * 2.0 - normal.y * 3.0;
        const double pushY = normal.y * 2.0 + normal.x * 3.0;
        const Base::Position before = soul;
        soul = Base::Slide(board, soul, pushX, pushY, Base::SlideMode::Preserve);
        CHECK(board.Distance(soul) <= 0.01);
        if (step > 30) {
            CHECK(Travelled(before, soul) ==
                  doctest::Approx(Length(pushX, pushY)).epsilon(0.1));
        }
    }
}

TEST_CASE("止める側を選べば当たった向きだけ止まる") {
    const Base::RectangleBoundary board{{0.0, 0.0}, 50.0, 30.0};
    const Base::Position soul{0.0, -30.0};

    // 上に押した分が消え、横の3だけが残る
    const Base::Position blocked =
        Base::Slide(board, soul, 3.0, -3.0, Base::SlideMode::Block);
    CHECK(blocked.x == doctest::Approx(3.0));
    CHECK(blocked.y == doctest::Approx(-30.0).epsilon(0.05));

    // 斜めに押した速さのまま横へ進む
    const Base::Position preserved =
        Base::Slide(board, soul, 3.0, -3.0, Base::SlideMode::Preserve);
    CHECK(preserved.x == doctest::Approx(Length(3.0, 3.0)).epsilon(0.05));
    CHECK(preserved.y == doctest::Approx(-30.0).epsilon(0.05));
}

TEST_CASE("真正面から当たれば進まない") {
    const Base::RectangleBoundary board{{0.0, 0.0}, 50.0, 30.0};
    const Base::Position soul{49.9, 0.0};
    const Base::Position moved =
        Base::Slide(board, soul, 5.0, 0.0, Base::SlideMode::Preserve);

    CHECK(moved.x <= 50.01);
    CHECK(moved.y == doctest::Approx(0.0));
}

TEST_CASE("動かなければ枠の中に留まる") {
    const Base::CircleBoundary board{{0.0, 0.0}, 20.0};
    const Base::Position moved = Base::Slide(board, {5.0, 5.0}, 0.0, 0.0);
    CHECK(moved.x == doctest::Approx(5.0));
    CHECK(moved.y == doctest::Approx(5.0));
}

TEST_CASE("縁の形は当たり判定と同じ定義から出る") {
    const Base::CircleBoundary board{{0.0, 0.0}, 25.0};
    const std::vector<Base::Segment> outline = board.Outline({0.0, 0.0}, 40.0, 40.0, 1.0);

    REQUIRE_FALSE(outline.empty());
    for (const Base::Segment &segment : outline) {
        CHECK(board.Distance(segment.from) == doctest::Approx(0.0).epsilon(0.05));
        CHECK(Length(segment.from.x, segment.from.y) == doctest::Approx(25.0).epsilon(0.05));
    }
}

TEST_CASE("矩形の角が尖る") {
    const Base::RectangleBoundary board{{0.0, 0.0}, 40.0, 20.0};
    const std::vector<Base::Segment> outline = board.Outline({0.0, 0.0}, 60.0, 40.0, 1.0);

    REQUIRE_FALSE(outline.empty());

    // 4つの角の近くに縁が来ている
    const std::array<Base::Position, 4> corners{
        Base::Position{-40.0, -20.0}, Base::Position{40.0, -20.0},
        Base::Position{40.0, 20.0}, Base::Position{-40.0, 20.0}};
    for (const Base::Position &corner : corners) {
        double nearest = 1000.0;
        for (const Base::Segment &segment : outline) {
            nearest = std::min(nearest, Travelled(corner, segment.from));
        }
        CHECK(nearest < 1.5);
    }

    // 縁は矩形の外へ出ない
    for (const Base::Segment &segment : outline) {
        CHECK(std::abs(segment.from.x) <= 40.01);
        CHECK(std::abs(segment.from.y) <= 20.01);
    }
}

TEST_CASE("枠の外だけを見ても縁は出ない") {
    const Base::CircleBoundary board{{0.0, 0.0}, 10.0};
    CHECK(board.Outline({200.0, 200.0}, 20.0, 20.0, 1.0).empty());
    CHECK(board.Outline({0.0, 0.0}, 0.0, 0.0, 1.0).empty());
}

TEST_CASE("好きな式で枠を作れる") {
    // 十字の形
    const Base::FunctionBoundary board{[](Base::Position point) {
        const double horizontal =
            std::max(std::abs(point.x) - 40.0, std::abs(point.y) - 10.0);
        const double vertical =
            std::max(std::abs(point.x) - 10.0, std::abs(point.y) - 40.0);
        return std::min(horizontal, vertical);
    }};

    CHECK(board.Contains({0.0, 0.0}));
    CHECK(board.Contains({35.0, 0.0}));
    CHECK(board.Contains({0.0, 35.0}));
    CHECK_FALSE(board.Contains({30.0, 30.0}));

    Base::Position soul{0.0, 0.0};
    for (int step = 0; step < 20; ++step) {
        soul = Base::Slide(board, soul, 4.0, 4.0, Base::SlideMode::Preserve);
        CHECK(board.Distance(soul) <= 0.01);
    }
}

TEST_CASE("枠を動かしても追従する") {
    Base::RectangleBoundary board{{0.0, 0.0}, 20.0, 20.0};
    CHECK(board.Contains({15.0, 0.0}));

    board.SetCenter({100.0, 0.0});
    CHECK_FALSE(board.Contains({15.0, 0.0}));
    CHECK(board.Contains({95.0, 0.0}));

    board.Resize(5.0, 5.0);
    CHECK(board.Contains({98.0, 0.0}));
    CHECK_FALSE(board.Contains({93.0, 0.0}));
}

TEST_CASE("軸ごとに判定すれば角で震えない") {
    const Base::RectangleBoundary board{{0.0, 0.0}, 50.0, 30.0};
    Base::Position soul{-50.0, -30.0};

    // 左上の角へ押し続ける
    for (int step = 0; step < 20; ++step) {
        const Base::Position before = soul;
        soul = Base::Slide(board, soul, -4.0, -4.0);
        CHECK(soul.x == doctest::Approx(before.x));
        CHECK(soul.y == doctest::Approx(before.y));
    }
}

TEST_CASE("軸ごとに判定すれば通る軸だけ動く") {
    const Base::RectangleBoundary board{{0.0, 0.0}, 50.0, 30.0};

    // 上の壁に当たりながら右へ
    const Base::Position along = Base::Slide(board, {0.0, -30.0}, 4.0, -4.0);
    CHECK(along.x == doctest::Approx(4.0));
    CHECK(along.y == doctest::Approx(-30.0));

    // 右の壁に当たりながら下へ
    const Base::Position down = Base::Slide(board, {50.0, 0.0}, 4.0, 4.0);
    CHECK(down.x == doctest::Approx(50.0));
    CHECK(down.y == doctest::Approx(4.0));
}

TEST_CASE("既定は軸ごとの判定") {
    const Base::RectangleBoundary board{{0.0, 0.0}, 50.0, 30.0};

    // 速さを保つ方なら横へ4.24進むが、既定では3のまま
    const Base::Position moved = Base::Slide(board, {0.0, -30.0}, 3.0, -3.0);
    CHECK(moved.x == doctest::Approx(3.0));
    CHECK(moved.y == doctest::Approx(-30.0));
}

TEST_CASE("軸ごとの判定でも壁まで寄る") {
    const Base::RectangleBoundary board{{0.0, 0.0}, 50.0, 30.0};

    // 壁まで3しかないところへ4押しても、3だけ進む
    const Base::Position moved = Base::Slide(board, {47.0, 0.0}, 4.0, 0.0);
    CHECK(moved.x == doctest::Approx(50.0).epsilon(0.001));

    const Base::Position lowered = Base::Slide(board, {0.0, 28.5}, 0.0, 4.0);
    CHECK(lowered.y == doctest::Approx(30.0).epsilon(0.001));
}

TEST_CASE("割り切れない距離でも壁に密着する") {
    const Base::RectangleBoundary board{{0.0, 0.0}, 50.0, 29.0};
    Base::Position soul{0.0, 0.0};

    for (int step = 0; step < 20; ++step) {
        soul = Base::Slide(board, soul, 0.0, 4.0);
    }
    CHECK(soul.y == doctest::Approx(29.0).epsilon(0.001));
}
