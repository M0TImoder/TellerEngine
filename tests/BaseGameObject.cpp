#include <Base/GameObject.hpp>
#include <Base/Rate.hpp>
#include <Base/VariableCheck.hpp>
#include <Base/Variables.hpp>

#include "World.hpp"

#include <doctest/doctest.h>

#include <string>
#include <vector>

namespace Base = TellerEngine::Base;

namespace {

struct Recorder : Base::GameObject {
    std::vector<std::string> calls;
    Base::Velocity moveSpeed{2};

    void Create(Base::Context &) override { calls.emplace_back("Create"); }
    void Destroy(Base::Context &) override { calls.emplace_back("Destroy"); }
    void BeginStep(Base::Context &) override { calls.emplace_back("BeginStep"); }
    void Alarm(Base::Context &) override { calls.emplace_back("Alarm"); }
    void Step(Base::Context &) override { calls.emplace_back("Step"); }
    void Collision(Base::Context &, Base::GameObject &other) override {
        (void)other;
        calls.emplace_back("Collision");
    }
    void EndStep(Base::Context &) override { calls.emplace_back("EndStep"); }
    void Draw(Base::Context &) override { calls.emplace_back("Draw"); }

    static constexpr auto Variables() {
        return Base::Extend(Base::GameObject::Variables(),
                            Base::MakeVariables(Base::Var(&Recorder::calls, "calls"),
                                                Base::Var(&Recorder::moveSpeed, "moveSpeed",
                                                          "spd")));
    }
};

struct Silent : Base::GameObject {
    static constexpr auto Variables() { return Base::GameObject::Variables(); }
};

void RunPhases(Base::Context &context, Base::GameObject &object, Base::GameObject &other) {
    object.BeginStep(context);
    object.Alarm(context);
    object.Step(context);
    object.Collision(context, other);
    object.EndStep(context);
    object.Draw(context);
}

} // namespace

TEST_CASE("既定のフックは何もしない") {
    TellerTest::World world;
    Silent silent;
    Silent other;
    RunPhases(world.context, silent, other);
    CHECK(silent.x == doctest::Approx(0.0));
    CHECK(silent.visible);
    CHECK(silent.active);
    CHECK(silent.id == Base::InstanceId::None);
}

TEST_CASE("派生クラスがフックを差し替えられる") {
    TellerTest::World world;
    Recorder recorder;
    Silent other;
    recorder.Create(world.context);
    RunPhases(world.context, recorder, other);
    recorder.Destroy(world.context);

    CHECK(recorder.calls == std::vector<std::string>{"Create", "BeginStep", "Alarm", "Step",
                                                     "Collision", "EndStep", "Draw",
                                                     "Destroy"});
}

TEST_CASE("基底越しに呼んでも派生の側が動く") {
    TellerTest::World world;
    Recorder recorder;
    Base::GameObject &object = recorder;
    object.Step(world.context);
    CHECK(recorder.calls == std::vector<std::string>{"Step"});
}

TEST_CASE("基底の変数が表に載っている") {
    static_assert(Base::VariableCount<Base::GameObject>() == 6);
    Base::GameObject object;
    *Base::FindVariable(object, "x").As<double>() = 4.5;
    *Base::FindVariable(object, "depth").As<double>() = -10.0;
    *Base::FindVariable(object, "visible").As<bool>() = false;

    CHECK(object.x == doctest::Approx(4.5));
    CHECK(object.depth == doctest::Approx(-10.0));
    CHECK_FALSE(object.visible);
}

TEST_CASE("派生クラスの表が基底の分を引き継ぐ") {
    static_assert(Base::VariableCount<Recorder>() == 8);
    Recorder recorder;
    CHECK(Base::FindVariable(recorder, "y").As<double>() != nullptr);
    CHECK(Base::FindVariable(recorder, "spd").As<Base::Velocity>() != nullptr);
}

TEST_CASE("基底も派生も名前が重複していない") {
    static_assert(Base::HasUniqueVariableNames<Base::GameObject>());
    static_assert(Base::HasUniqueVariableNames<Recorder>());
    static_assert(Base::VariablesFitInObject<Base::GameObject>());
}

TEST_CASE("基底に登録漏れがない") {
    const Base::VariableCoverage coverage = Base::CheckVariableCoverage(Base::GameObject{});
    CHECK(coverage.Ok());
}

TEST_CASE("派生クラスの登録漏れも見つかる") {
    const Base::VariableCoverage full = Base::CheckVariableCoverage(Recorder{});
    CHECK(full.Ok());

    const Base::VariableCoverage partial = Base::CheckVariableCoverage(Silent{});
    CHECK(partial.Ok());
}
