#include <Base/Canvas.hpp>
#include <Base/Draw.hpp>
#include <Base/GameObject.hpp>
#include <Base/Loop.hpp>
#include <Base/Present.hpp>

#include <doctest/doctest.h>

#include <cstdint>

namespace Base = TellerEngine::Base;

namespace {

Base::InstanceId Id(std::uint32_t value) { return static_cast<Base::InstanceId>(value); }

// 1つのインスタンスが矩形を1枚積んだ状態を作る
Base::DrawList Single(std::uint32_t source, double x, double y) {
    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SetSource(Id(source));
    canvas.Rectangle(x, y, 10.0, 10.0);
    return list;
}

} // namespace

TEST_CASE("既定は素のまま出す") {
    Base::Presenter presenter;
    CHECK(presenter.Mode() == Base::DisplayMode::Native);

    presenter.Submit(Single(1, 0.0, 0.0));
    presenter.Submit(Single(1, 10.0, 0.0));

    CHECK(presenter.Frame(0.5).Commands()[0].x == doctest::Approx(10.0));
}

TEST_CASE("繰り返しは同じものを出し続ける") {
    Base::Presenter presenter;
    presenter.SetMode(Base::DisplayMode::Repeat);

    presenter.Submit(Single(1, 0.0, 0.0));
    presenter.Submit(Single(1, 10.0, 0.0));

    CHECK(presenter.Frame(0.0).Commands()[0].x == doctest::Approx(10.0));
    CHECK(presenter.Frame(0.5).Commands()[0].x == doctest::Approx(10.0));
    CHECK(presenter.Frame(1.0).Commands()[0].x == doctest::Approx(10.0));
}

TEST_CASE("補間は前の積み荷との間を出す") {
    Base::Presenter presenter;
    presenter.SetMode(Base::DisplayMode::Interpolate);

    presenter.Submit(Single(1, 0.0, 0.0));
    presenter.Submit(Single(1, 10.0, 20.0));

    CHECK(presenter.Frame(0.0).Commands()[0].x == doctest::Approx(0.0));
    CHECK(presenter.Frame(0.5).Commands()[0].x == doctest::Approx(5.0));
    CHECK(presenter.Frame(0.5).Commands()[0].y == doctest::Approx(10.0));
    CHECK(presenter.Frame(1.0).Commands()[0].x == doctest::Approx(10.0));
}

TEST_CASE("範囲の外を渡しても端で止まる") {
    Base::Presenter presenter;
    presenter.SetMode(Base::DisplayMode::Interpolate);
    presenter.Submit(Single(1, 0.0, 0.0));
    presenter.Submit(Single(1, 10.0, 0.0));

    CHECK(presenter.Frame(-5.0).Commands()[0].x == doctest::Approx(0.0));
    CHECK(presenter.Frame(5.0).Commands()[0].x == doctest::Approx(10.0));
}

TEST_CASE("最初の1つは補間しない") {
    Base::Presenter presenter;
    presenter.SetMode(Base::DisplayMode::Interpolate);
    presenter.Submit(Single(1, 30.0, 0.0));

    CHECK(presenter.Frame(0.0).Commands()[0].x == doctest::Approx(30.0));
    CHECK(presenter.Submitted() == 1);
}

TEST_CASE("新しく生まれたものはその場に出る") {
    Base::Presenter presenter;
    presenter.SetMode(Base::DisplayMode::Interpolate);

    presenter.Submit(Single(1, 0.0, 0.0));

    Base::DrawList second;
    {
        Base::Canvas canvas{second};
        canvas.SetSource(Id(1));
        canvas.Rectangle(10.0, 0.0, 10.0, 10.0);
        canvas.SetSource(Id(2));
        canvas.Rectangle(50.0, 50.0, 10.0, 10.0);
    }
    presenter.Submit(second);

    const Base::DrawList &frame = presenter.Frame(0.5);
    REQUIRE(frame.Size() == 2);
    CHECK(frame.Commands()[0].x == doctest::Approx(5.0));
    CHECK(frame.Commands()[1].x == doctest::Approx(50.0));
}

TEST_CASE("消えたものは出ない") {
    Base::Presenter presenter;
    presenter.SetMode(Base::DisplayMode::Interpolate);

    Base::DrawList first;
    {
        Base::Canvas canvas{first};
        canvas.SetSource(Id(1));
        canvas.Rectangle(0.0, 0.0, 10.0, 10.0);
        canvas.SetSource(Id(2));
        canvas.Rectangle(0.0, 0.0, 10.0, 10.0);
    }
    presenter.Submit(first);
    presenter.Submit(Single(1, 10.0, 0.0));

    CHECK(presenter.Frame(0.5).Size() == 1);
}

TEST_CASE("同じインスタンスが積んだ複数の絵を取り違えない") {
    Base::Presenter presenter;
    presenter.SetMode(Base::DisplayMode::Interpolate);

    for (double offset : {0.0, 10.0}) {
        Base::DrawList list;
        Base::Canvas canvas{list};
        canvas.SetSource(Id(1));
        canvas.Rectangle(offset, 0.0, 10.0, 10.0);
        canvas.Rectangle(offset + 100.0, 0.0, 10.0, 10.0);
        presenter.Submit(list);
    }

    const Base::DrawList &frame = presenter.Frame(0.5);
    REQUIRE(frame.Size() == 2);
    CHECK(frame.Commands()[0].x == doctest::Approx(5.0));
    CHECK(frame.Commands()[1].x == doctest::Approx(105.0));
}

TEST_CASE("種類が変わったら補間しない") {
    Base::Presenter presenter;
    presenter.SetMode(Base::DisplayMode::Interpolate);

    presenter.Submit(Single(1, 0.0, 0.0));

    Base::DrawList second;
    {
        Base::Canvas canvas{second};
        canvas.SetSource(Id(1));
        canvas.Circle(10.0, 0.0, 5.0);
    }
    presenter.Submit(second);

    CHECK(presenter.Frame(0.5).Commands()[0].x == doctest::Approx(10.0));
}

TEST_CASE("向きは短い側を回る") {
    Base::Presenter presenter;
    presenter.SetMode(Base::DisplayMode::Interpolate);

    for (double angle : {350.0, 10.0}) {
        Base::DrawList list;
        Base::Canvas canvas{list};
        canvas.SetSource(Id(1));
        canvas.SpriteExt(Base::ImageId{1}, 0, 0.0, 0.0, 1.0, 1.0, angle);
        presenter.Submit(list);
    }

    CHECK(presenter.Frame(0.5).Commands()[0].rotation == doctest::Approx(360.0));
}

TEST_CASE("色と透明度も混ざる") {
    Base::Presenter presenter;
    presenter.SetMode(Base::DisplayMode::Interpolate);

    for (int step = 0; step < 2; ++step) {
        Base::DrawList list;
        Base::Canvas canvas{list};
        canvas.SetSource(Id(1));
        canvas.SetColor(step == 0 ? Base::Color{0, 0, 0, 255} : Base::Color{100, 200, 0, 255});
        canvas.SetAlpha(step == 0 ? 0.0 : 1.0);
        canvas.Rectangle(0.0, 0.0, 10.0, 10.0);
        presenter.Submit(list);
    }

    const Base::DrawCommand &command = presenter.Frame(0.5).Commands()[0];
    CHECK(command.color.red == 50);
    CHECK(command.color.green == 100);
    CHECK(command.alpha == doctest::Approx(0.5));
}

TEST_CASE("文字も持ち越される") {
    Base::Presenter presenter;
    presenter.SetMode(Base::DisplayMode::Interpolate);

    for (double x : {0.0, 10.0}) {
        Base::DrawList list;
        Base::Canvas canvas{list};
        canvas.SetSource(Id(1));
        canvas.Text(x, 0.0, "ここ");
        presenter.Submit(list);
    }

    const Base::DrawList &frame = presenter.Frame(0.5);
    CHECK(frame.TextOf(frame.Commands()[0]) == "ここ");
    CHECK(frame.Commands()[0].x == doctest::Approx(5.0));
}

TEST_CASE("論理ステップまでの進み具合が取れる") {
    Base::LoopCycle cycle;
    CHECK(cycle.Progress() == doctest::Approx(0.0));

    cycle.AddElapsed(1000.0 / 60.0);
    CHECK(cycle.Progress() > 0.4);
    CHECK(cycle.Progress() < 0.6);

    cycle.AddElapsed(1000.0);
    CHECK(cycle.Progress() == doctest::Approx(1.0));
}

TEST_CASE("論理レートが0なら進み具合は0のまま") {
    Base::LoopCycle cycle{Base::LoopRates{.logic = 0, .display = 30}};
    cycle.AddElapsed(1000.0);
    CHECK(cycle.Progress() == doctest::Approx(0.0));
}
