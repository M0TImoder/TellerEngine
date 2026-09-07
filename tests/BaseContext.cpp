#include <Base/Context.hpp>
#include <Base/GameObject.hpp>
#include <Base/Globals.hpp>
#include <Base/Instances.hpp>
#include <Base/Loop.hpp>
#include <Base/Random.hpp>
#include <Base/Scheduler.hpp>
#include <Base/Variables.hpp>

#include <doctest/doctest.h>

#include <cstdint>
#include <vector>

namespace Base = TellerEngine::Base;

namespace {

struct StoryGlobals : Base::Globals {
    std::int64_t plot = 0;
    std::int64_t deaths = 0;

    static constexpr auto Variables() {
        return Base::Extend(Base::Globals::Variables(),
                            Base::MakeVariables(Base::Var(&StoryGlobals::plot, "plot"),
                                                Base::Var(&StoryGlobals::deaths, "deaths")));
    }
};

struct OtherGlobals : Base::Globals {
    static constexpr auto Variables() { return Base::Globals::Variables(); }
};

struct Advancer : Base::GameObject {
    void Step(Base::Context &context) override {
        if (StoryGlobals *globals = context.Global<StoryGlobals>()) {
            globals->plot += 1;
        }
    }

    static constexpr auto Variables() { return Base::GameObject::Variables(); }
};

struct Roller : Base::GameObject {
    std::int64_t drawn = 0;

    void Step(Base::Context &context) override {
        drawn = context.random.Integer(99);
    }

    static constexpr auto Variables() {
        return Base::Extend(Base::GameObject::Variables(),
                            Base::MakeVariables(Base::Var(&Roller::drawn, "drawn")));
    }
};

std::vector<int> lifecycle;

struct Child : Base::GameObject {
    void Create(Base::Context &) override { lifecycle.push_back(1); }
    void Destroy(Base::Context &) override { lifecycle.push_back(2); }

    static constexpr auto Variables() { return Base::GameObject::Variables(); }
};

struct Parent : Base::GameObject {
    Base::InstanceId child = Base::InstanceId::None;

    void Create(Base::Context &context) override { child = context.Create<Child>(); }
    void Destroy(Base::Context &context) override { context.Destroy(child); }

    static constexpr auto Variables() {
        return Base::Extend(Base::GameObject::Variables(),
                            Base::MakeVariables(Base::Var(&Parent::child, "child")));
    }
};

struct World {
    Base::Instances instances;
    StoryGlobals globals;
    Base::Random random;
    Base::LoopCycle cycle;
    Base::Context context{instances, globals, random, cycle.Clock()};
    Base::Scheduler scheduler;
};

} // namespace

TEST_CASE("インスタンスからグローバルへ届く") {
    World world;
    world.context.Create<Advancer>();

    world.scheduler.Advance(world.context);
    CHECK(world.globals.plot == 1);

    world.scheduler.Advance(world.context);
    CHECK(world.globals.plot == 2);
}

TEST_CASE("宣言した型と違うグローバルは取り出せない") {
    World world;
    CHECK(world.context.Global<StoryGlobals>() != nullptr);
    CHECK(world.context.Global<OtherGlobals>() == nullptr);
    CHECK(world.context.Global<Base::Globals>() != nullptr);
}

TEST_CASE("インスタンスから乱数へ届く") {
    World world;
    world.context.Create<Roller>();
    world.scheduler.Advance(world.context);
    world.scheduler.Advance(world.context);

    CHECK(world.random.Draws() == 2);
    CHECK(world.context.instances.First<Roller>()->drawn >= 0);
    CHECK(world.context.instances.First<Roller>()->drawn <= 99);
}

TEST_CASE("生成と破棄がフックを呼ぶ") {
    lifecycle.clear();
    World world;
    const Base::InstanceId id = world.context.Create<Parent>();

    CHECK(lifecycle == std::vector<int>{1});
    CHECK(world.instances.Count() == 2);

    world.context.Destroy(id);
    CHECK(lifecycle == std::vector<int>{1, 2});
    CHECK(world.instances.Count() == 0);
}

TEST_CASE("存在しないインスタンスを破棄しても何も起きない") {
    lifecycle.clear();
    World world;
    world.context.Destroy(Base::InstanceId::None);
    world.context.Destroy(static_cast<Base::InstanceId>(1234));
    CHECK(lifecycle.empty());
}

TEST_CASE("二重に破棄してもフックは1度だけ") {
    lifecycle.clear();
    World world;
    const Base::InstanceId id = world.context.Create<Child>();
    world.context.Destroy(id);
    world.context.Destroy(id);
    CHECK(lifecycle == std::vector<int>{1, 2});
}

TEST_CASE("グローバルは文脈越しに名前でも引ける") {
    World world;
    world.globals.deaths = 7;
    const Base::VariableRef ref = Base::FindVariable(world.globals, "deaths");
    REQUIRE(ref);
    CHECK(*ref.As<std::int64_t>() == 7);
}

TEST_CASE("同じ一覧の上で別のグローバルを並べられる") {
    Base::Instances instances;
    StoryGlobals first;
    StoryGlobals second;
    Base::Random random;
    Base::LoopCycle cycle;
    Base::Context left{instances, first, random, cycle.Clock()};
    Base::Context right{instances, second, random, cycle.Clock()};

    left.Global<StoryGlobals>()->plot = 1;
    right.Global<StoryGlobals>()->plot = 2;
    CHECK(first.plot == 1);
    CHECK(second.plot == 2);
}
