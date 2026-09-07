#include <Base/GameObject.hpp>
#include <Base/Instances.hpp>
#include <Base/Scheduler.hpp>
#include <Base/Variables.hpp>

#include "World.hpp"

#include <doctest/doctest.h>

#include <string>
#include <vector>

namespace Base = TellerEngine::Base;

namespace {

struct Bullet : Base::GameObject {
    int power = 0;
    int steps = 0;

    void Step(Base::Context &) override { steps += 1; }

    static constexpr auto Variables() {
        return Base::Extend(Base::GameObject::Variables(),
                            Base::MakeVariables(Base::Var(&Bullet::power, "power"),
                                                Base::Var(&Bullet::steps, "steps")));
    }
};

struct HomingBullet : Bullet {
    static constexpr auto Variables() { return Bullet::Variables(); }
};

struct Heart : Base::GameObject {
    static constexpr auto Variables() { return Base::GameObject::Variables(); }
};

Base::InstanceId AddBullet(Base::Instances &instances, int power) {
    const Base::InstanceId id = instances.Create<Bullet>();
    instances.Find<Bullet>(id)->power = power;
    return id;
}

} // namespace

TEST_CASE("止めたインスタンスは存在しないものとして扱われる") {
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    const Base::InstanceId id = AddBullet(instances, 1);

    instances.Deactivate(id);
    CHECK_FALSE(instances.Exists(id));
    CHECK_FALSE(instances.Exists<Bullet>());
    CHECK(instances.Count() == 0);
    CHECK(instances.Count<Bullet>() == 0);
    CHECK(instances.CountExact<Bullet>() == 0);
    CHECK(instances.First<Bullet>() == nullptr);
}

TEST_CASE("止めてもIDを指した参照だけは残る") {
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    const Base::InstanceId id = AddBullet(instances, 42);

    instances.Deactivate(id);
    REQUIRE(instances.Find<Bullet>(id) != nullptr);
    CHECK(instances.Find<Bullet>(id)->power == 42);
    instances.Find<Bullet>(id)->power = 43;
    CHECK(instances.Find<Bullet>(id)->power == 43);
}

TEST_CASE("止めたインスタンスは走査に入らない") {
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    const Base::InstanceId id = AddBullet(instances, 1);

    instances.Deactivate(id);

    std::size_t visited = 0;
    instances.With<Bullet>([&](Bullet &) { visited += 1; });
    instances.ForEach([&](Base::GameObject &) { visited += 1; });
    instances.WithId(id, [&](Base::GameObject &) { visited += 1; });
    instances.WithId<Bullet>(id, [&](Bullet &) { visited += 1; });
    CHECK(visited == 0);
}

TEST_CASE("止めたインスタンスはどのフェーズにも入らない") {
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    Base::Scheduler scheduler;
    const Base::InstanceId id = AddBullet(instances, 1);

    scheduler.Advance(world.context);
    CHECK(instances.Find<Bullet>(id)->steps == 1);

    instances.Deactivate(id);
    scheduler.Advance(world.context);
    scheduler.Advance(world.context);
    CHECK(instances.Find<Bullet>(id)->steps == 1);

    instances.Activate(id);
    scheduler.Advance(world.context);
    CHECK(instances.Find<Bullet>(id)->steps == 2);
}

TEST_CASE("全て止めてから全て戻せる") {
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    AddBullet(instances, 1);
    AddBullet(instances, 2);
    instances.Create<Heart>();

    instances.DeactivateAll();
    CHECK(instances.Count() == 0);

    instances.ActivateAll();
    CHECK(instances.Count() == 3);
}

TEST_CASE("1つだけ残して全て止められる") {
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    const Base::InstanceId keep = AddBullet(instances, 1);
    AddBullet(instances, 2);
    instances.Create<Heart>();

    instances.DeactivateAllExcept(keep);
    CHECK(instances.Count() == 1);
    CHECK(instances.Exists(keep));
    CHECK(instances.First<Bullet>()->power == 1);
}

TEST_CASE("型を指して止めたり戻したりできる") {
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    AddBullet(instances, 1);
    instances.Create<HomingBullet>();
    instances.Create<Heart>();

    instances.DeactivateAll<Bullet>();
    CHECK(instances.Count<Bullet>() == 0);
    CHECK(instances.Count<Heart>() == 1);

    instances.ActivateAll<Bullet>();
    CHECK(instances.Count<Bullet>() == 2);
}

TEST_CASE("止めたものを型で戻すときも派生を含む") {
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    instances.Create<HomingBullet>();

    instances.DeactivateAll();
    CHECK(instances.Count<HomingBullet>() == 0);

    instances.ActivateAll<Bullet>();
    CHECK(instances.Count<HomingBullet>() == 1);
}

TEST_CASE("止めたインスタンスは描かれない") {
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    const Base::InstanceId id = AddBullet(instances, 1);
    instances.Deactivate(id);

    std::size_t drawn = 0;
    instances.ForEachByDepth([&](Base::GameObject &) { drawn += 1; });
    CHECK(drawn == 0);
}

TEST_CASE("何番目かを指して取り出せる") {
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    AddBullet(instances, 10);
    const Base::InstanceId second = AddBullet(instances, 20);
    AddBullet(instances, 30);

    CHECK(instances.Nth<Bullet>(0)->power == 10);
    CHECK(instances.Nth<Bullet>(1)->power == 20);
    CHECK(instances.Nth<Bullet>(2)->power == 30);
    CHECK(instances.Nth<Bullet>(3) == nullptr);

    instances.Deactivate(second);
    CHECK(instances.Nth<Bullet>(1)->power == 30);
}

TEST_CASE("止めたインスタンスを破棄しても数は合う") {
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    const Base::InstanceId id = AddBullet(instances, 1);
    instances.Deactivate(id);
    instances.Destroy(id);

    CHECK(instances.Find(id) == nullptr);
    CHECK(instances.Count() == 0);
}
