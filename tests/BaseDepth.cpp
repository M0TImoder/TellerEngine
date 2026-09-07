#include <Base/GameObject.hpp>
#include <Base/Instances.hpp>
#include <Base/Scheduler.hpp>
#include <Base/Variables.hpp>

#include "World.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <string>
#include <vector>

namespace Base = TellerEngine::Base;

namespace {

std::vector<std::string> drawn;

struct Layer : Base::GameObject {
    std::string tag = "?";

    void Draw(Base::Context &) override { drawn.push_back(tag); }

    static constexpr auto Variables() {
        return Base::Extend(Base::GameObject::Variables(),
                            Base::MakeVariables(Base::Var(&Layer::tag, "tag")));
    }
};

Layer &Add(Base::Instances &instances, std::string tag, double depth) {
    Layer *layer = instances.Find<Layer>(instances.Create<Layer>());
    layer->tag = std::move(tag);
    layer->depth = depth;
    return *layer;
}

std::vector<std::string> DrawOrder(TellerTest::World &world) {
    drawn.clear();
    world.instances.ForEachByDepth(
        [&world](Base::GameObject &object) { object.Draw(world.context); });
    return drawn;
}

} // namespace

TEST_CASE("depthの大きいものから描かれる") {
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    Add(instances, "front", -10.0);
    Add(instances, "back", 100.0);
    Add(instances, "middle", 0.0);

    CHECK(DrawOrder(world) == std::vector<std::string>{"back", "middle", "front"});
}

TEST_CASE("同じdepthは生成順を保つ") {
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    Add(instances, "a", 0.0);
    Add(instances, "b", 0.0);
    Add(instances, "c", 0.0);
    Add(instances, "d", 0.0);

    CHECK(DrawOrder(world) == std::vector<std::string>{"a", "b", "c", "d"});
}

TEST_CASE("同じdepthの中でだけ生成順が残る") {
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    Add(instances, "a", 0.0);
    Add(instances, "z", 50.0);
    Add(instances, "b", 0.0);
    Add(instances, "y", 50.0);
    Add(instances, "c", 0.0);

    CHECK(DrawOrder(world) == std::vector<std::string>{"z", "y", "a", "b", "c"});
}

TEST_CASE("depthを変えると次の周から並びが変わる") {
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    Layer &a = Add(instances, "a", 0.0);
    Add(instances, "b", 10.0);

    CHECK(DrawOrder(world) == std::vector<std::string>{"b", "a"});
    a.depth = 20.0;
    CHECK(DrawOrder(world) == std::vector<std::string>{"a", "b"});
}

TEST_CASE("activeでないインスタンスは並びに入らない") {
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    Add(instances, "a", 0.0);
    Layer &b = Add(instances, "b", 10.0);
    b.active = false;

    CHECK(DrawOrder(world) == std::vector<std::string>{"a"});
}

TEST_CASE("破棄したインスタンスは並びに入らない") {
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    Add(instances, "a", 0.0);
    Layer &b = Add(instances, "b", 10.0);
    instances.Destroy(b.id);

    CHECK(DrawOrder(world) == std::vector<std::string>{"a"});
}

TEST_CASE("depthが数でない場合は最後に回る") {
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    Add(instances, "broken", std::nan(""));
    Add(instances, "a", 0.0);
    Add(instances, "b", 10.0);

    CHECK(DrawOrder(world) == std::vector<std::string>{"b", "a", "broken"});
}

TEST_CASE("スケジューラのDrawがdepth順になる") {
    drawn.clear();
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    Base::Scheduler scheduler;
    Add(instances, "front", -5.0);
    Add(instances, "back", 5.0);

    scheduler.Advance(world.context);
    CHECK(drawn == std::vector<std::string>{"back", "front"});
}

TEST_CASE("描かれないインスタンスは並びを崩さない") {
    drawn.clear();
    TellerTest::World world;
    Base::Instances &instances = world.instances;
    Base::Scheduler scheduler;
    Add(instances, "back", 5.0);
    Add(instances, "hidden", 0.0).visible = false;
    Add(instances, "front", -5.0);

    scheduler.Advance(world.context);
    CHECK(drawn == std::vector<std::string>{"back", "front"});
}
