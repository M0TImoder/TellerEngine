#include <Base/Context.hpp>
#include <Base/GameObject.hpp>
#include <Base/Globals.hpp>
#include <Base/Instances.hpp>
#include <Base/Input.hpp>
#include <Base/Loop.hpp>
#include <Base/Random.hpp>
#include <Base/Scheduler.hpp>
#include <Base/Variables.hpp>

#include <doctest/doctest.h>

#include <cstdint>
#include <vector>

namespace Base = TellerEngine::Base;

namespace {

int StepsFor(Base::LoopCycle &cycle, double milliseconds) {
    cycle.AddElapsed(milliseconds);
    int steps = 0;
    while (cycle.ConsumeStep()) {
        steps += 1;
    }
    return steps;
}

// トリエル撃破で20に落ちるような書き換え
struct RateChanger : Base::GameObject {
    Base::LoopCycle *cycle = nullptr;
    int changeAt = 0;
    int seen = 0;

    void Step(Base::Context &) override {
        seen += 1;
        if (seen == changeAt) {
            cycle->SetLogicRate(10);
        }
    }

    static constexpr auto Variables() { return Base::GameObject::Variables(); }
};

struct ClockReader : Base::GameObject {
    std::vector<std::uint64_t> seen;

    void Step(Base::Context &context) override {
        seen.push_back(context.clock.Milliseconds());
    }

    static constexpr auto Variables() {
        return Base::Extend(Base::GameObject::Variables(),
                            Base::MakeVariables(Base::Var(&ClockReader::seen, "seen")));
    }
};

} // namespace

TEST_CASE("既定のレートは論理も表示も30") {
    const Base::LoopRates rates;
    CHECK(rates.logic == 30);
    CHECK(rates.display == 30);

    Base::LoopCycle cycle;
    CHECK(cycle.LogicRate() == 30);
    CHECK(cycle.DisplayRate() == 30);
}

TEST_CASE("指示付き初期化で両方まとめて指定できる") {
    Base::LoopCycle cycle{Base::LoopRates{.logic = 30, .display = 60}};
    CHECK(cycle.LogicRate() == 30);
    CHECK(cycle.DisplayRate() == 60);
    CHECK(cycle.DisplayInterval() == doctest::Approx(1000.0 / 60.0));
}

TEST_CASE("溜まった分だけステップを取り出せる") {
    Base::LoopCycle cycle;
    CHECK(StepsFor(cycle, 0.0) == 0);
    CHECK(StepsFor(cycle, 33.4) == 1);
    CHECK(StepsFor(cycle, 100.0) == 3);
}

TEST_CASE("割り切れないレートでも取りこぼしが積み上がらない") {
    Base::LoopCycle cycle;
    CHECK(StepsFor(cycle, 1000.0) == 30);
    CHECK(cycle.PendingMicroseconds() == 0);

    int total = 0;
    for (int second = 0; second < 60; ++second) {
        total += StepsFor(cycle, 1000.0);
    }
    CHECK(total == 1800);
}

TEST_CASE("端数は次の反復へ持ち越される") {
    Base::LoopCycle cycle;
    CHECK(StepsFor(cycle, 20.0) == 0);
    CHECK(cycle.PendingMicroseconds() == 20000);
    CHECK(StepsFor(cycle, 20.0) == 1);
    CHECK(cycle.PendingMicroseconds() == 40000 - 33333);
}

TEST_CASE("論理レートを上げると同じ時間で回数が増える") {
    Base::LoopCycle slow;
    Base::LoopCycle fast{Base::LoopRates{.logic = 60, .display = 60}};
    CHECK(StepsFor(slow, 1000.0) == 30);
    CHECK(StepsFor(fast, 1000.0) == 60);
}

TEST_CASE("論理レートが0以下ならステップは出ない") {
    Base::LoopCycle cycle{Base::LoopRates{.logic = 0, .display = 30}};
    CHECK(StepsFor(cycle, 1000.0) == 0);
    CHECK(cycle.DisplayInterval() == doctest::Approx(1000.0 / 30.0));
}

TEST_CASE("溜まった分を捨てられる") {
    Base::LoopCycle cycle;
    cycle.AddElapsed(500.0);
    cycle.DropPending();
    CHECK(cycle.PendingMicroseconds() == 0);
    CHECK_FALSE(cycle.ConsumeStep());
}

TEST_CASE("仮想クロックは経過した分だけ進む") {
    Base::LoopCycle cycle;
    CHECK(cycle.Clock().Milliseconds() == 0);
    cycle.AddElapsed(16.0);
    cycle.AddElapsed(16.0);
    CHECK(cycle.Clock().Milliseconds() == 32);

    cycle.Clock().SetMicroseconds(1000000);
    CHECK(cycle.Clock().Milliseconds() == 1000);
}

TEST_CASE("控えを取って戻せる") {
    Base::LoopCycle cycle;
    cycle.AddElapsed(50.0);
    const Base::LoopCycle::Snapshot saved = cycle.Save();

    cycle.SetLogicRate(10);
    cycle.AddElapsed(500.0);
    CHECK(cycle.LogicRate() == 10);

    cycle.Restore(saved);
    CHECK(cycle.LogicRate() == 30);
    CHECK(cycle.PendingMicroseconds() == 50000);
    CHECK(cycle.Clock().Milliseconds() == 50);
}

TEST_CASE("回している最中に論理レートが変わっても刻み幅が追従する") {
    Base::Instances instances;
    Base::Globals globals;
    Base::Random random;
    Base::LoopCycle cycle;
    Base::Input input;
    Base::Context context{instances, globals, random, cycle.Clock(), input};
    Base::Scheduler scheduler;

    RateChanger *changer =
        instances.Find<RateChanger>(context.Create<RateChanger>());
    changer->cycle = &cycle;
    changer->changeAt = 2;

    // 30で2回進んだあと10に落ちるので、残りは100msごとになる
    cycle.AddElapsed(1000.0);
    int steps = 0;
    while (cycle.ConsumeStep()) {
        scheduler.Advance(context);
        steps += 1;
    }

    CHECK(cycle.LogicRate() == 10);
    CHECK(steps == 11);
}

TEST_CASE("ゲームコードは仮想クロックだけを読む") {
    Base::Instances instances;
    Base::Globals globals;
    Base::Random random;
    Base::LoopCycle cycle;
    Base::Input input;
    Base::Context context{instances, globals, random, cycle.Clock(), input};
    Base::Scheduler scheduler;

    context.Create<ClockReader>();

    cycle.AddElapsed(100.0);
    scheduler.Advance(context);
    cycle.AddElapsed(50.0);
    scheduler.Advance(context);

    const ClockReader *reader = instances.First<ClockReader>();
    REQUIRE(reader != nullptr);
    CHECK(reader->seen == std::vector<std::uint64_t>{100, 150});
}
