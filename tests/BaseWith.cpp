#include <Base/GameObject.hpp>
#include <Base/Instances.hpp>
#include <Base/Variables.hpp>

#include <doctest/doctest.h>

#include <string>
#include <vector>

namespace Base = TellerEngine::Base;

namespace {

struct Bullet : Base::GameObject {
    std::string tag = "bullet";

    static constexpr auto Variables() {
        return Base::Extend(Base::GameObject::Variables(),
                            Base::MakeVariables(Base::Var(&Bullet::tag, "tag")));
    }
};

struct HomingBullet : Bullet {
    static constexpr auto Variables() { return Bullet::Variables(); }
};

struct SplitBullet : HomingBullet {
    static constexpr auto Variables() { return HomingBullet::Variables(); }
};

struct Heart : Base::GameObject {
    static constexpr auto Variables() { return Base::GameObject::Variables(); }
};

template <typename T> std::vector<std::string> TagsOf(Base::Instances &instances) {
    std::vector<std::string> tags;
    instances.With<T>([&](Bullet &bullet) { tags.push_back(bullet.tag); });
    return tags;
}

Base::InstanceId AddBullet(Base::Instances &instances, std::string tag) {
    const Base::InstanceId id = instances.Create<Bullet>();
    instances.Find<Bullet>(id)->tag = std::move(tag);
    return id;
}

} // namespace

TEST_CASE("Withは派生した型も回る") {
    Base::Instances instances;
    AddBullet(instances, "plain");
    instances.Find<HomingBullet>(instances.Create<HomingBullet>())->tag = "homing";
    instances.Find<SplitBullet>(instances.Create<SplitBullet>())->tag = "split";
    instances.Create<Heart>();

    CHECK(TagsOf<Bullet>(instances) == std::vector<std::string>{"plain", "homing", "split"});
    CHECK(TagsOf<HomingBullet>(instances) == std::vector<std::string>{"homing", "split"});
    CHECK(TagsOf<SplitBullet>(instances) == std::vector<std::string>{"split"});
}

TEST_CASE("WithExactは丁度その型だけ回る") {
    Base::Instances instances;
    AddBullet(instances, "plain");
    instances.Create<HomingBullet>();

    std::vector<std::string> tags;
    instances.WithExact<Bullet>([&](Bullet &bullet) { tags.push_back(bullet.tag); });
    CHECK(tags == std::vector<std::string>{"plain"});
}

TEST_CASE("Withは基底越しにも回る") {
    Base::Instances instances;
    AddBullet(instances, "a");
    instances.Create<Heart>();

    std::size_t visited = 0;
    instances.With<Base::GameObject>([&](Base::GameObject &) { visited += 1; });
    CHECK(visited == 2);
}

TEST_CASE("1つも無い型を回しても何も起きない") {
    Base::Instances instances;
    instances.Create<Heart>();

    std::size_t visited = 0;
    instances.With<Bullet>([&](Bullet &) { visited += 1; });
    CHECK(visited == 0);
    CHECK_FALSE(instances.Exists<Bullet>());
    CHECK(instances.First<Bullet>() == nullptr);
}

TEST_CASE("IDを指した走査は存在すれば1回") {
    Base::Instances instances;
    const Base::InstanceId id = AddBullet(instances, "a");

    std::vector<std::string> tags;
    instances.WithId<Bullet>(id, [&](Bullet &bullet) { tags.push_back(bullet.tag); });
    CHECK(tags == std::vector<std::string>{"a"});
}

TEST_CASE("存在しないIDを指した走査は0回") {
    Base::Instances instances;
    const Base::InstanceId id = AddBullet(instances, "a");
    instances.Destroy(id);

    std::size_t visited = 0;
    instances.WithId(id, [&](Base::GameObject &) { visited += 1; });
    instances.WithId<Bullet>(id, [&](Bullet &) { visited += 1; });
    instances.WithId(Base::InstanceId::None, [&](Base::GameObject &) { visited += 1; });
    CHECK(visited == 0);
}

TEST_CASE("IDを指した走査は型が合わなければ0回") {
    Base::Instances instances;
    const Base::InstanceId id = instances.Create<Heart>();

    std::size_t visited = 0;
    instances.WithId<Bullet>(id, [&](Bullet &) { visited += 1; });
    CHECK(visited == 0);
}

TEST_CASE("存在の確認は派生も含む") {
    Base::Instances instances;
    const Base::InstanceId id = instances.Create<SplitBullet>();

    CHECK(instances.Exists<Bullet>());
    CHECK(instances.Exists<SplitBullet>());
    CHECK(instances.Exists(id));
    CHECK_FALSE(instances.Exists<Heart>());

    instances.Destroy(id);
    CHECK_FALSE(instances.Exists<Bullet>());
    CHECK_FALSE(instances.Exists(id));
}

TEST_CASE("数えるときも派生を含む") {
    Base::Instances instances;
    instances.Create<Bullet>();
    instances.Create<HomingBullet>();
    instances.Create<SplitBullet>();
    instances.Create<Heart>();

    CHECK(instances.Count<Bullet>() == 3);
    CHECK(instances.CountExact<Bullet>() == 1);
    CHECK(instances.Count<HomingBullet>() == 2);
    CHECK(instances.Count<Base::GameObject>() == 4);
    CHECK(instances.Count() == 4);
}

TEST_CASE("最初の1つも派生を含む") {
    Base::Instances instances;
    instances.Find<HomingBullet>(instances.Create<HomingBullet>())->tag = "homing";
    AddBullet(instances, "plain");

    CHECK(instances.First<Bullet>()->tag == "homing");
    instances.First<Bullet>()->active = false;
    CHECK(instances.First<Bullet>()->tag == "plain");
}

TEST_CASE("activeでないインスタンスは走査にも数にも入らない") {
    Base::Instances instances;
    const Base::InstanceId id = AddBullet(instances, "a");
    AddBullet(instances, "b");
    instances.Find<Bullet>(id)->active = false;

    CHECK(TagsOf<Bullet>(instances) == std::vector<std::string>{"b"});
    CHECK_FALSE(instances.Exists(id));
    CHECK(instances.Find(id) != nullptr);
    CHECK(instances.Count<Bullet>() == 1);
}

TEST_CASE("走査の途中で破棄しても残りが崩れない") {
    Base::Instances instances;
    AddBullet(instances, "a");
    const Base::InstanceId b = AddBullet(instances, "b");
    AddBullet(instances, "c");

    std::vector<std::string> tags;
    instances.With<Bullet>([&](Bullet &bullet) {
        tags.push_back(bullet.tag);
        instances.Destroy(b);
    });
    CHECK(tags == std::vector<std::string>{"a", "c"});
}

TEST_CASE("走査の途中で自分を破棄しても続きが回る") {
    Base::Instances instances;
    AddBullet(instances, "a");
    AddBullet(instances, "b");

    std::vector<std::string> tags;
    instances.With<Bullet>([&](Bullet &bullet) {
        tags.push_back(bullet.tag);
        instances.Destroy(bullet.id);
    });
    CHECK(tags == std::vector<std::string>{"a", "b"});
    CHECK(instances.Count() == 0);
}
