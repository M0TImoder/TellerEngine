#include <Base/Context.hpp>
#include <Base/GameObject.hpp>
#include <Base/Globals.hpp>
#include <Base/Instances.hpp>
#include <Base/Input.hpp>
#include <Base/Loop.hpp>
#include <Base/Random.hpp>
#include <Base/Scheduler.hpp>
#include <Base/State.hpp>
#include <Base/Variables.hpp>

#include <doctest/doctest.h>

#include <cstdint>
#include <vector>

namespace Base = TellerEngine::Base;

namespace {

struct StoryGlobals : Base::Globals {
    std::int64_t plot = 0;
    std::int64_t spawned = 0;

    static constexpr auto Variables() {
        return Base::Extend(Base::Globals::Variables(),
                            Base::MakeVariables(Base::Var(&StoryGlobals::plot, "plot"),
                                                Base::Var(&StoryGlobals::spawned, "spawned")));
    }
};

struct Mote : Base::GameObject {
    std::int64_t life = 3;

    void Step(Base::Context &context) override {
        life -= 1;
        if (life <= 0) {
            context.Destroy(id);
        }
    }

    static constexpr auto Variables() {
        return Base::Extend(Base::GameObject::Variables(),
                            Base::MakeVariables(Base::Var(&Mote::life, "life")));
    }
};

struct Spawner : Base::GameObject {
    void Step(Base::Context &context) override {
        StoryGlobals *globals = context.Global<StoryGlobals>();
        globals->plot += 1;
        if (context.random.Integer(2) == 0) {
            const Base::InstanceId child = context.Create<Mote>();
            context.instances.Find<Mote>(child)->life = context.random.Integer(4) + 1;
            globals->spawned += 1;
        }
    }

    static constexpr auto Variables() { return Base::GameObject::Variables(); }
};

struct World {
    Base::Instances instances;
    StoryGlobals globals;
    Base::Random random{12345};
    Base::LoopCycle cycle;
    Base::Input input;
    Base::Context context{instances, globals, random, cycle.Clock(), input};
    Base::Scheduler scheduler;

    void Run(int frames) {
        for (int i = 0; i < frames; ++i) {
            cycle.AddElapsed(1000.0 / 30.0);
            scheduler.Advance(context);
        }
    }

    Base::StateSnapshot<StoryGlobals> Save() {
        return Base::SaveState(instances, globals, random, cycle, input, scheduler);
    }

    void Restore(const Base::StateSnapshot<StoryGlobals> &snapshot) {
        Base::RestoreState(snapshot, instances, globals, random, cycle, input, scheduler);
    }

    std::vector<std::int64_t> Lives() {
        std::vector<std::int64_t> lives;
        instances.With<Mote>([&](Mote &mote) { lives.push_back(mote.life); });
        return lives;
    }
};

} // namespace

TEST_CASE("控えを取って戻すと状態が揃う") {
    World world;
    world.context.Create<Spawner>();
    world.Run(10);

    const auto saved = world.Save();
    const std::int64_t plot = world.globals.plot;
    const std::vector<std::int64_t> lives = world.Lives();
    const std::uint64_t frame = world.scheduler.Frame();
    const std::uint64_t draws = world.random.Draws();

    world.Run(20);
    CHECK(world.globals.plot != plot);

    world.Restore(saved);
    CHECK(world.globals.plot == plot);
    CHECK(world.Lives() == lives);
    CHECK(world.scheduler.Frame() == frame);
    CHECK(world.random.Draws() == draws);
}

TEST_CASE("戻したあとの続きが必ず同じになる") {
    World world;
    world.context.Create<Spawner>();
    world.Run(5);

    const auto saved = world.Save();

    world.Run(30);
    const std::int64_t firstPlot = world.globals.plot;
    const std::int64_t firstSpawned = world.globals.spawned;
    const std::vector<std::int64_t> firstLives = world.Lives();
    const std::uint64_t firstDraws = world.random.Draws();

    world.Restore(saved);
    world.Run(30);

    CHECK(world.globals.plot == firstPlot);
    CHECK(world.globals.spawned == firstSpawned);
    CHECK(world.Lives() == firstLives);
    CHECK(world.random.Draws() == firstDraws);
}

TEST_CASE("同じ控えから何度でも戻せる") {
    World world;
    world.context.Create<Spawner>();
    world.Run(5);
    const auto saved = world.Save();

    std::vector<std::int64_t> results;
    for (int attempt = 0; attempt < 3; ++attempt) {
        world.Restore(saved);
        world.Run(12);
        results.push_back(world.globals.plot);
    }

    CHECK(results[0] == results[1]);
    CHECK(results[1] == results[2]);
}

TEST_CASE("控えのあとに生まれたインスタンスは戻すと消える") {
    World world;
    const auto saved = world.Save();

    world.context.Create<Mote>();
    world.context.Create<Mote>();
    CHECK(world.instances.Count() == 2);

    world.Restore(saved);
    CHECK(world.instances.Count() == 0);
}

TEST_CASE("控えのあとに破棄したインスタンスは戻すと生き返る") {
    World world;
    const Base::InstanceId id = world.context.Create<Mote>();
    world.instances.Find<Mote>(id)->life = 42;

    const auto saved = world.Save();

    world.context.Destroy(id);
    world.instances.Collect();
    CHECK(world.instances.Count() == 0);

    world.Restore(saved);
    REQUIRE(world.instances.Find<Mote>(id) != nullptr);
    CHECK(world.instances.Find<Mote>(id)->life == 42);
}

TEST_CASE("戻したあとに振られるIDが衝突しない") {
    World world;
    world.context.Create<Mote>();
    const auto saved = world.Save();

    const Base::InstanceId afterSave = world.context.Create<Mote>();
    world.Restore(saved);
    const Base::InstanceId afterRestore = world.context.Create<Mote>();

    CHECK(afterSave == afterRestore);
    CHECK(world.instances.Count() == 2);
}

TEST_CASE("控えは元の状態から切り離されている") {
    World world;
    const Base::InstanceId id = world.context.Create<Mote>();
    world.instances.Find<Mote>(id)->life = 7;

    const auto saved = world.Save();
    world.instances.Find<Mote>(id)->life = 99;
    world.globals.plot = 99;

    world.Restore(saved);
    CHECK(world.instances.Find<Mote>(id)->life == 7);
    CHECK(world.globals.plot == 0);
}

TEST_CASE("止めたインスタンスの状態も控えに入る") {
    World world;
    const Base::InstanceId id = world.context.Create<Mote>();
    world.instances.Deactivate(id);

    const auto saved = world.Save();
    world.instances.Activate(id);
    CHECK(world.instances.Exists(id));

    world.Restore(saved);
    CHECK_FALSE(world.instances.Exists(id));
    CHECK(world.instances.Find(id) != nullptr);
}

TEST_CASE("見た目の状態も控えに入る") {
    World world;
    const Base::InstanceId id = world.context.Create<Mote>();
    Base::GameObject *object = world.instances.Find(id);
    object->x = 1.5;
    object->y = -2.5;
    object->depth = 30.0;
    object->visible = false;

    const auto saved = world.Save();
    object->x = 0.0;
    object->depth = 0.0;
    object->visible = true;

    world.Restore(saved);
    const Base::GameObject *restored = world.instances.Find(id);
    CHECK(restored->x == doctest::Approx(1.5));
    CHECK(restored->y == doctest::Approx(-2.5));
    CHECK(restored->depth == doctest::Approx(30.0));
    CHECK_FALSE(restored->visible);
}

TEST_CASE("レートと仮想クロックも控えに入る") {
    World world;
    world.Run(3);
    world.cycle.SetLogicRate(20);
    const auto saved = world.Save();

    world.cycle.SetLogicRate(10);
    world.Run(5);
    CHECK(world.cycle.LogicRate() == 10);

    world.Restore(saved);
    CHECK(world.cycle.LogicRate() == 20);
    CHECK(world.cycle.Clock().Milliseconds() == 99);
}
