#include <Base/Rate.hpp>
#include <Base/Variables.hpp>

#include <doctest/doctest.h>

#include <string>
#include <vector>

namespace Base = TellerEngine::Base;

namespace {

struct Actor {
    double x = 0.0;
    double y = 0.0;
    bool visible = true;
    Base::Duration invulnerable{30};

    static constexpr auto Variables() {
        return Base::MakeVariables(Base::Var(&Actor::x, "x"), Base::Var(&Actor::y, "y"),
                                   Base::Var(&Actor::visible, "visible"),
                                   Base::Var(&Actor::invulnerable, "invulnerable",
                                             "invultimer"));
    }
};

struct Heart : Actor {
    Base::Velocity moveSpeed{2};
    std::string label = "soul";
    int charge = 0;

    static constexpr auto Variables() {
        return Base::Extend(Actor::Variables(),
                            Base::MakeVariables(Base::Var(&Heart::moveSpeed, "moveSpeed", "spd"),
                                                Base::Var(&Heart::label, "label"),
                                                Base::Var(&Heart::charge, "charge")));
    }
};

// Actorの部分が先頭に来ない配置を作る
struct Marker {
    Base::Acceleration gravity{0.5};

    static constexpr auto Variables() {
        return Base::MakeVariables(Base::Var(&Marker::gravity, "gravity"));
    }
};

struct Combined : Marker, Actor {
    int layer = 3;

    static constexpr auto Variables() {
        return Base::Extend(Base::Extend(Marker::Variables(), Actor::Variables()),
                            Base::MakeVariables(Base::Var(&Combined::layer, "layer")));
    }
};

std::vector<std::string> NamesOf(const Heart &heart) {
    std::vector<std::string> names;
    Base::ForEachVariable(heart, [&](const Base::VariableDescriptor &descriptor,
                                     Base::ConstVariableRef) {
        names.emplace_back(descriptor.name);
    });
    return names;
}

} // namespace

TEST_CASE("継承で表が連結される") {
    static_assert(Base::VariableCount<Actor>() == 4);
    static_assert(Base::VariableCount<Heart>() == 7);

    CHECK(NamesOf(Heart{}) == std::vector<std::string>{"x", "y", "visible", "invulnerable",
                                                       "moveSpeed", "label", "charge"});
}

TEST_CASE("原名は省くと名前と同じものが入る") {
    constexpr auto descriptors = Base::DescribeVariables<Heart>();
    CHECK(descriptors[0].name == "x");
    CHECK(descriptors[0].original == "x");
    CHECK(descriptors[3].name == "invulnerable");
    CHECK(descriptors[3].original == "invultimer");
    CHECK(descriptors[4].name == "moveSpeed");
    CHECK(descriptors[4].original == "spd");
}

TEST_CASE("区分が型から決まる") {
    constexpr auto descriptors = Base::DescribeVariables<Heart>();
    CHECK(descriptors[0].kind == Base::VariableKind::Number);
    CHECK(descriptors[2].kind == Base::VariableKind::Boolean);
    CHECK(descriptors[3].kind == Base::VariableKind::Duration);
    CHECK(descriptors[4].kind == Base::VariableKind::Velocity);
    CHECK(descriptors[5].kind == Base::VariableKind::Text);
    CHECK(descriptors[6].kind == Base::VariableKind::Integer);

    constexpr auto marker = Base::DescribeVariables<Marker>();
    CHECK(marker[0].kind == Base::VariableKind::Acceleration);
}

TEST_CASE("名前でも原名でも引ける") {
    Heart heart;
    CHECK(Base::FindVariable(heart, "moveSpeed").As<Base::Velocity>() != nullptr);
    CHECK(Base::FindVariable(heart, "spd").As<Base::Velocity>() != nullptr);
    CHECK(Base::FindVariable(heart, "invultimer").As<Base::Duration>() != nullptr);
}

TEST_CASE("引いた参照から書き換えられる") {
    Heart heart;
    *Base::FindVariable(heart, "x").As<double>() = 12.5;
    *Base::FindVariable(heart, "charge").As<int>() = 7;
    *Base::FindVariable(heart, "label").As<std::string>() = "changed";

    CHECK(heart.x == doctest::Approx(12.5));
    CHECK(heart.charge == 7);
    CHECK(heart.label == "changed");
}

TEST_CASE("型が違えば取り出せない") {
    Heart heart;
    const Base::VariableRef ref = Base::FindVariable(heart, "charge");
    REQUIRE(ref);
    CHECK(ref.As<int>() != nullptr);
    CHECK(ref.As<double>() == nullptr);
    CHECK(ref.As<long>() == nullptr);
}

TEST_CASE("宣言していない名前は存在しない") {
    Heart heart;
    const Base::VariableRef ref = Base::FindVariable(heart, "存在しない");
    CHECK_FALSE(ref);
    CHECK(ref.As<double>() == nullptr);
}

TEST_CASE("先頭にない基底のメンバでも番地が合う") {
    Combined combined;
    static_assert(Base::VariableCount<Combined>() == 6);

    *Base::FindVariable(combined, "y").As<double>() = 99.0;
    *Base::FindVariable(combined, "gravity").As<Base::Acceleration>() = 1.25;
    *Base::FindVariable(combined, "layer").As<int>() = 8;

    CHECK(combined.Actor::y == doctest::Approx(99.0));
    CHECK(combined.gravity.Value() == doctest::Approx(1.25));
    CHECK(combined.layer == 8);
    CHECK(Base::FindVariable(combined, "y").data == static_cast<void *>(&combined.Actor::y));
}

TEST_CASE("constのインスタンスからは読むだけになる") {
    const Heart heart;
    const Base::ConstVariableRef ref = Base::FindVariable(heart, "visible");
    REQUIRE(ref);
    const bool *value = ref.As<bool>();
    REQUIRE(value != nullptr);
    CHECK(*value);
}

TEST_CASE("レート次元は倍率が次数で決まる") {
    constexpr Base::Duration duration{30};
    constexpr Base::Velocity velocity{4};
    constexpr Base::Acceleration acceleration{0.5};

    static_assert(duration.Rescaled(2.0).Value() == 60.0);
    static_assert(velocity.Rescaled(2.0).Value() == 2.0);
    static_assert(acceleration.Rescaled(2.0).Value() == 0.125);

    CHECK(duration.Rescaled(1.0).Value() == doctest::Approx(30.0));
}

TEST_CASE("レート次元はdoubleとして計算できる") {
    Base::Velocity speed{2};
    double x = 10.0;
    x -= speed;
    CHECK(x == doctest::Approx(8.0));

    speed = 3.0;
    CHECK(speed.Value() == doctest::Approx(3.0));
}

TEST_CASE("登録済みメンバの大きさを合計する") {
    static_assert(Base::RegisteredBytes<Actor>() ==
                  sizeof(double) * 2 + sizeof(bool) + sizeof(Base::Duration));
    CHECK(Base::RegisteredBytes<Actor>() <= sizeof(Actor));
}
