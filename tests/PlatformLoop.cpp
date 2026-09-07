#include <Base/Loop.hpp>
#include <Base/Platform/LoopCycleCtrl.hpp>
#include <Base/Platform/System.hpp>

#include <doctest/doctest.h>

#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

namespace Base = TellerEngine::Base;
namespace Platform = TellerEngine::Base::Platform;

namespace {

// 実時間を待たずに任意の経過を与える
struct Driver : Platform::LoopCycleCtrl {
    using LoopCycleCtrl::LoopCycleCtrl;

    int steps = 0;
    int presents = 0;

    void Step() override { steps += 1; }
    void Present() override { presents += 1; }
    void Wait() override {}

    int Feed(std::uint64_t nanoseconds) { return Advance(nanoseconds); }
};

constexpr std::uint64_t kOneSecond = 1000000000;

} // namespace

TEST_CASE("論理と表示が同じレートなら1反復に1ステップ") {
    Base::LoopCycle cycle;
    Driver driver{cycle};

    CHECK(driver.Feed(kOneSecond / 30) == 1);
    CHECK(driver.Feed(kOneSecond / 30) == 1);
    CHECK(driver.steps == 2);
}

TEST_CASE("遅れても速くても1反復1ステップのまま") {
    Base::LoopCycle cycle;
    Driver driver{cycle};

    CHECK(driver.Feed(kOneSecond) == 1);
    CHECK(driver.Feed(1) == 1);
    CHECK(driver.steps == 2);
}

TEST_CASE("レートが食い違えば実時間から回数を出す") {
    Base::LoopCycle cycle{Base::LoopRates{.logic = 60, .display = 30}};
    Driver driver{cycle};

    CHECK(driver.Feed(kOneSecond / 30) == 2);
    CHECK(driver.steps == 2);
}

TEST_CASE("溜め込む方を選べば同じレートでも実時間に従う") {
    Base::LoopCycle cycle;
    Driver driver{cycle};
    driver.SetPolicy(Platform::StepPolicy::Accumulated);

    CHECK(driver.Feed(kOneSecond / 60) == 0);
    CHECK(driver.Feed(kOneSecond / 60) == 1);
}

TEST_CASE("止まっていた分を無限に取り返そうとしない") {
    Base::LoopCycle cycle{Base::LoopRates{.logic = 60, .display = 30}};
    Driver driver{cycle};
    CHECK(driver.MaxSteps() == 8);

    CHECK(driver.Feed(kOneSecond * 10) == 8);
    CHECK(driver.Feed(kOneSecond / 30) == 2);
}

TEST_CASE("上限を変えられる") {
    Base::LoopCycle cycle{Base::LoopRates{.logic = 60, .display = 30}};
    Driver driver{cycle};
    driver.SetMaxSteps(3);

    CHECK(driver.Feed(kOneSecond) == 3);
}

TEST_CASE("論理レートが0なら進まない") {
    Base::LoopCycle cycle{Base::LoopRates{.logic = 0, .display = 30}};
    Driver driver{cycle};

    CHECK(driver.Feed(kOneSecond) == 0);
    CHECK(driver.steps == 0);
}

TEST_CASE("反復のたびに1度だけ画面へ出す") {
    auto system = Platform::System::Create(SDL_INIT_EVENTS);
    REQUIRE(system.has_value());

    Base::LoopCycle cycle;
    Driver driver{cycle};

    driver.Update();
    driver.Update();
    driver.Update();

    CHECK(driver.presents == 3);
    CHECK(driver.Iterations() == 3);
    CHECK(driver.LastSteps() == 1);
}

TEST_CASE("実行中にレートが変わると刻み方も変わる") {
    Base::LoopCycle cycle;
    Driver driver{cycle};

    CHECK(driver.Feed(kOneSecond / 30) == 1);

    cycle.SetLogicRate(10);
    CHECK(driver.Feed(kOneSecond / 30) == 0);
    CHECK(driver.Feed(kOneSecond / 10) == 1);
}

TEST_CASE("待ち方を差し替えられる") {
    auto system = Platform::System::Create(SDL_INIT_EVENTS);
    REQUIRE(system.has_value());

    Base::LoopCycle cycle{Base::LoopRates{.logic = 30, .display = 1000}};
    Driver driver{cycle};

    const std::uint64_t before = SDL_GetTicksNS();
    for (int i = 0; i < 3; ++i) {
        driver.Update();
    }
    const std::uint64_t spent = SDL_GetTicksNS() - before;
    CHECK(spent < kOneSecond);
}
