#include <Base/GameObject.hpp>
#include <Base/Instances.hpp>
#include <Base/Rate.hpp>
#include <Base/Scheduler.hpp>
#include <Base/Timers.hpp>
#include <Base/Variables.hpp>

#include "World.hpp"

#include <doctest/doctest.h>

#include <string>
#include <vector>

namespace Base = TellerEngine::Base;

namespace {

std::vector<std::string> phaseLog;

struct Tracer : Base::GameObject {
    std::string tag = "?";

    void BeginStep(Base::Context &) override { phaseLog.push_back(tag + ":BeginStep"); }
    void Alarm(Base::Context &) override { phaseLog.push_back(tag + ":Alarm"); }
    void Step(Base::Context &) override { phaseLog.push_back(tag + ":Step"); }
    void EndStep(Base::Context &) override { phaseLog.push_back(tag + ":EndStep"); }
    void Draw(Base::Context &, Base::Canvas &) override { phaseLog.push_back(tag + ":Draw"); }

    static constexpr auto Variables() {
        return Base::Extend(Base::GameObject::Variables(),
                            Base::MakeVariables(Base::Var(&Tracer::tag, "tag")));
    }
};

struct Spawner : Base::GameObject {
    bool spawned = false;

    void BeginStep(Base::Context &context) override {
        if (!spawned) {
            spawned = true;
            const Base::InstanceId id = context.Create<Tracer>();
            context.instances.Find<Tracer>(id)->tag = "born";
        }
    }

    static constexpr auto Variables() { return Base::GameObject::Variables(); }
};

struct Ticker : Base::GameObject {
    Base::Duration shortFuse{2};
    Base::Duration longFuse{5};
    int shortFired = 0;
    int longFired = 0;

    void Alarm(Base::Context &) override { Base::TickTimers(*this); }

    void OnShort() { shortFired += 1; }
    void OnLong() { longFired += 1; }

    static constexpr auto Timers() {
        return Base::ExtendTimers(Base::GameObject::Timers(),
                                  Base::MakeTimers(Base::Timer(&Ticker::shortFuse,
                                                               &Ticker::OnShort),
                                                   Base::Timer(&Ticker::longFuse,
                                                               &Ticker::OnLong)));
    }

    static constexpr auto Variables() {
        return Base::Extend(
            Base::GameObject::Variables(),
            Base::MakeVariables(Base::Var(&Ticker::shortFuse, "shortFuse"),
                                Base::Var(&Ticker::longFuse, "longFuse"),
                                Base::Var(&Ticker::shortFired, "shortFired"),
                                Base::Var(&Ticker::longFired, "longFired")));
    }
};

int resolved = 0;

void CountCollisions(Base::Context &context) {
    (void)context;
    resolved += 1;
}

} // namespace

TEST_CASE("フェーズの並びが決まっている") {
    phaseLog.clear();
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    Base::Scheduler scheduler;
    instances.Find<Tracer>(instances.Create<Tracer>())->tag = "a";

    scheduler.Advance(world.context, world.canvas);
    CHECK(phaseLog == std::vector<std::string>{"a:BeginStep", "a:Alarm", "a:Step", "a:EndStep",
                                          "a:Draw"});
    CHECK(scheduler.Frame() == 1);
}

TEST_CASE("フェーズごとに全インスタンスを回してから次へ進む") {
    phaseLog.clear();
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    Base::Scheduler scheduler;
    instances.Find<Tracer>(instances.Create<Tracer>())->tag = "a";
    instances.Find<Tracer>(instances.Create<Tracer>())->tag = "b";

    scheduler.Advance(world.context, world.canvas);
    CHECK(phaseLog == std::vector<std::string>{"a:BeginStep", "b:BeginStep", "a:Alarm", "b:Alarm",
                                          "a:Step", "b:Step", "a:EndStep", "b:EndStep",
                                          "a:Draw", "b:Draw"});
}

TEST_CASE("生まれたフレームはStep系のフェーズに一切入らない") {
    phaseLog.clear();
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    Base::Scheduler scheduler;
    instances.Create<Spawner>();

    scheduler.Advance(world.context, world.canvas);
    CHECK(phaseLog == std::vector<std::string>{"born:Draw"});

    phaseLog.clear();
    scheduler.Advance(world.context, world.canvas);
    CHECK(phaseLog == std::vector<std::string>{"born:BeginStep", "born:Alarm", "born:Step",
                                          "born:EndStep", "born:Draw"});
}

TEST_CASE("visibleでないインスタンスは描かれない") {
    phaseLog.clear();
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    Base::Scheduler scheduler;
    Tracer *tracer = instances.Find<Tracer>(instances.Create<Tracer>());
    tracer->tag = "a";
    tracer->visible = false;

    scheduler.Advance(world.context, world.canvas);
    CHECK(phaseLog == std::vector<std::string>{"a:BeginStep", "a:Alarm", "a:Step", "a:EndStep"});
}

TEST_CASE("衝突の解決はStepとEndStepの間に入る") {
    resolved = 0;
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    Base::Scheduler scheduler;
    scheduler.SetCollisionResolver(&CountCollisions);
    instances.Create<Tracer>();

    scheduler.Advance(world.context, world.canvas);
    scheduler.Advance(world.context, world.canvas);
    CHECK(resolved == 2);
}

TEST_CASE("破棄したインスタンスは次のフレームで領域ごと消える") {
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    Base::Scheduler scheduler;
    const Base::InstanceId id = instances.Create<Tracer>();
    instances.Destroy(id);

    CHECK(instances.Count() == 0);
    scheduler.Advance(world.context, world.canvas);
    CHECK(instances.Find(id) == nullptr);
}

TEST_CASE("タイマーは数え終わったときに1度だけ呼ぶ") {
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    Base::Scheduler scheduler;
    Ticker *ticker = instances.Find<Ticker>(instances.Create<Ticker>());

    scheduler.Advance(world.context, world.canvas);
    CHECK(ticker->shortFired == 0);

    scheduler.Advance(world.context, world.canvas);
    CHECK(ticker->shortFired == 1);
    CHECK(ticker->longFired == 0);

    scheduler.Advance(world.context, world.canvas);
    scheduler.Advance(world.context, world.canvas);
    CHECK(ticker->longFired == 0);

    scheduler.Advance(world.context, world.canvas);
    CHECK(ticker->longFired == 1);

    for (int i = 0; i < 10; ++i) {
        scheduler.Advance(world.context, world.canvas);
    }
    CHECK(ticker->shortFired == 1);
    CHECK(ticker->longFired == 1);
}

TEST_CASE("止まったタイマーは入れ直せば再び動く") {
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    Base::Scheduler scheduler;
    Ticker *ticker = instances.Find<Ticker>(instances.Create<Ticker>());

    for (int i = 0; i < 3; ++i) {
        scheduler.Advance(world.context, world.canvas);
    }
    REQUIRE(ticker->shortFired == 1);
    CHECK(ticker->shortFuse.Value() == doctest::Approx(-1.0));

    ticker->shortFuse = 2.0;
    scheduler.Advance(world.context, world.canvas);
    scheduler.Advance(world.context, world.canvas);
    CHECK(ticker->shortFired == 2);
}

TEST_CASE("タイマーの残りは表に載る") {
    static_assert(Base::TimerCount<Ticker>() == 2);
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    Ticker *ticker = instances.Find<Ticker>(instances.Create<Ticker>());
    CHECK(Base::FindVariable(*ticker, "shortFuse").As<Base::Duration>() != nullptr);
}
