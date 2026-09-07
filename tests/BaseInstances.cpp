#include <Base/GameObject.hpp>
#include <Base/Instances.hpp>
#include <Base/VariableCheck.hpp>
#include <Base/Variables.hpp>

#include "World.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Base = TellerEngine::Base;

namespace {

struct Bullet : Base::GameObject {
    int power = 1;

    static constexpr auto Variables() {
        return Base::Extend(Base::GameObject::Variables(),
                            Base::MakeVariables(Base::Var(&Bullet::power, "power")));
    }
};

struct Heart : Base::GameObject {
    int hp = 20;

    static constexpr auto Variables() {
        return Base::Extend(Base::GameObject::Variables(),
                            Base::MakeVariables(Base::Var(&Heart::hp, "hp")));
    }
};

int createdCount = 0;
int destroyedCount = 0;

struct Counted : Base::GameObject {
    void Create(Base::Context &) override { createdCount += 1; }
    void Destroy(Base::Context &) override { destroyedCount += 1; }

    static constexpr auto Variables() { return Base::GameObject::Variables(); }
};

// powerを登録し忘れた形
struct Leaky : Base::GameObject {
    std::int64_t power = 0;

    static constexpr auto Variables() { return Base::GameObject::Variables(); }
};

Base::TypeId reportedType = nullptr;
std::size_t reportedGaps = 0;

void RecordCoverage(Base::TypeId type, const Base::VariableCoverage &coverage) {
    reportedType = type;
    reportedGaps = coverage.gapCount;
}

std::vector<int> PowersOf(Base::Instances &instances) {
    std::vector<int> powers;
    instances.With<Bullet>([&](Bullet &bullet) { powers.push_back(bullet.power); });
    return powers;
}

} // namespace

TEST_CASE("生成するとIDが振られる") {
    Base::Instances instances;
    const Base::InstanceId first = instances.Create<Heart>();
    const Base::InstanceId second = instances.Create<Bullet>();

    CHECK(first != Base::InstanceId::None);
    CHECK(first != second);
    CHECK(instances.Find(first)->id == first);
    CHECK(instances.Count() == 2);
}

TEST_CASE("IDから実体を引ける") {
    Base::Instances instances;
    const Base::InstanceId id = instances.Create<Heart>();
    instances.Find<Heart>(id)->hp = 3;

    CHECK(instances.Find<Heart>(id)->hp == 3);
    CHECK(instances.Exists(id));
}

TEST_CASE("生成したときの型と違えば引けない") {
    Base::Instances instances;
    const Base::InstanceId id = instances.Create<Heart>();

    CHECK(instances.Find<Heart>(id) != nullptr);
    CHECK(instances.Find<Bullet>(id) == nullptr);
    CHECK(instances.Find(id) != nullptr);
}

TEST_CASE("宣言していないIDを引いても落ちない") {
    Base::Instances instances;
    const auto missing = static_cast<Base::InstanceId>(9999);

    CHECK_FALSE(instances.Exists(missing));
    CHECK(instances.Find(missing) == nullptr);
    CHECK(instances.Find<Heart>(missing) == nullptr);
    instances.Destroy(missing);
    CHECK(instances.Count() == 0);
}

TEST_CASE("破棄すると直ちに存在しなくなる") {
    createdCount = 0;
    destroyedCount = 0;

    TellerTest::World world;
    Base::Instances &instances = world.instances;
    const Base::InstanceId id = world.context.Create<Counted>();
    CHECK(createdCount == 1);

    world.context.Destroy(id);
    CHECK(destroyedCount == 1);
    CHECK_FALSE(instances.Exists(id));
    CHECK(instances.Find(id) == nullptr);
    CHECK(instances.Count() == 0);
}

TEST_CASE("二重に破棄しても2度目は何も起きない") {
    destroyedCount = 0;
    TellerTest::World world;
    const Base::InstanceId id = world.context.Create<Counted>();
    world.context.Destroy(id);
    world.context.Destroy(id);
    CHECK(destroyedCount == 1);
}

TEST_CASE("Collectしても生成順が変わらない") {
    Base::Instances instances;
    for (int power : {1, 2, 3, 4}) {
        const Base::InstanceId id = instances.Create<Bullet>();
        instances.Find<Bullet>(id)->power = power;
    }

    std::vector<Base::InstanceId> ids;
    instances.With<Bullet>([&](Bullet &bullet) { ids.push_back(bullet.id); });
    instances.Destroy(ids[1]);
    instances.Collect();

    CHECK(PowersOf(instances) == std::vector<int>{1, 3, 4});
    CHECK(instances.Find<Bullet>(ids[0]) != nullptr);
    CHECK(instances.Find<Bullet>(ids[2]) != nullptr);
    CHECK(instances.Find<Bullet>(ids[1]) == nullptr);
}

TEST_CASE("型ごとに数えられる") {
    Base::Instances instances;
    instances.Create<Heart>();
    instances.Create<Bullet>();
    instances.Create<Bullet>();

    CHECK(instances.Count() == 3);
    CHECK(instances.Count<Heart>() == 1);
    CHECK(instances.Count<Bullet>() == 2);
    CHECK(instances.Count<Counted>() == 0);
}

TEST_CASE("Withは同じ型だけを生成順に回る") {
    Base::Instances instances;
    instances.Create<Heart>();
    const Base::InstanceId a = instances.Create<Bullet>();
    instances.Create<Heart>();
    const Base::InstanceId b = instances.Create<Bullet>();
    instances.Find<Bullet>(a)->power = 5;
    instances.Find<Bullet>(b)->power = 7;

    CHECK(PowersOf(instances) == std::vector<int>{5, 7});
}

TEST_CASE("ForEachは型をまたいで生成順に回る") {
    Base::Instances instances;
    const Base::InstanceId first = instances.Create<Heart>();
    const Base::InstanceId second = instances.Create<Bullet>();
    const Base::InstanceId third = instances.Create<Heart>();

    std::vector<Base::InstanceId> visited;
    instances.ForEach([&](Base::GameObject &object) { visited.push_back(object.id); });
    CHECK(visited == std::vector<Base::InstanceId>{first, second, third});
}

TEST_CASE("activeでないインスタンスは回らない") {
    Base::Instances instances;
    const Base::InstanceId a = instances.Create<Bullet>();
    const Base::InstanceId b = instances.Create<Bullet>();
    instances.Find<Bullet>(a)->power = 5;
    instances.Find<Bullet>(b)->power = 7;
    instances.Find<Bullet>(a)->active = false;

    CHECK(PowersOf(instances) == std::vector<int>{7});
    CHECK_FALSE(instances.Exists(a));
    CHECK(instances.Find(a) != nullptr);
    CHECK(instances.Count<Bullet>() == 1);
}

TEST_CASE("Firstは最初の有効なインスタンスを返す") {
    Base::Instances instances;
    const Base::InstanceId a = instances.Create<Bullet>();
    const Base::InstanceId b = instances.Create<Bullet>();
    instances.Find<Bullet>(a)->power = 5;
    instances.Find<Bullet>(b)->power = 7;

    CHECK(instances.First<Bullet>()->power == 5);

    instances.Find<Bullet>(a)->active = false;
    CHECK(instances.First<Bullet>()->power == 7);

    instances.Destroy(b);
    CHECK(instances.First<Bullet>() == nullptr);
    CHECK(instances.First<Heart>() == nullptr);
}

TEST_CASE("回っている最中に生成したものは同じ周では回らない") {
    Base::Instances instances;
    instances.Create<Bullet>();
    instances.Create<Bullet>();

    int visited = 0;
    instances.With<Bullet>([&](Bullet &) {
        visited += 1;
        if (visited == 1) {
            instances.Create<Bullet>();
        }
    });

    CHECK(visited == 2);
    CHECK(instances.Count<Bullet>() == 3);
}

TEST_CASE("回っている最中に破棄したものはその周でも回らない") {
    Base::Instances instances;
    const Base::InstanceId a = instances.Create<Bullet>();
    const Base::InstanceId b = instances.Create<Bullet>();
    instances.Find<Bullet>(a)->power = 5;
    instances.Find<Bullet>(b)->power = 7;

    std::vector<int> seen;
    instances.With<Bullet>([&](Bullet &bullet) {
        seen.push_back(bullet.power);
        instances.Destroy(b);
    });

    CHECK(seen == std::vector<int>{5});
}

TEST_CASE("登録漏れのある型は生成したときに報告される") {
    reportedType = nullptr;
    reportedGaps = 0;

    Base::Instances instances;
    instances.SetCoverageHandler(&RecordCoverage);
    instances.Create<Leaky>();

    CHECK(reportedType == Base::TypeIdOf<Leaky>());
    CHECK(reportedGaps == 1);
}

TEST_CASE("登録漏れがなければ報告されない") {
    reportedType = nullptr;

    Base::Instances instances;
    instances.SetCoverageHandler(&RecordCoverage);
    instances.Create<Heart>();

    CHECK(reportedType == nullptr);
}
