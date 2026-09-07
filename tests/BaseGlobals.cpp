#include <Base/Globals.hpp>
#include <Base/Rate.hpp>
#include <Base/VariableCheck.hpp>
#include <Base/Variables.hpp>

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Base = TellerEngine::Base;

namespace {

enum class Route {
    Neutral,
    Pacifist,
    Genocide,
};

struct StoryGlobals : Base::Globals {
    std::int64_t plot = 0;
    Route route = Route::Neutral;
    std::string name = "FRISK";
    Base::Duration textSpeed{2};

    static constexpr auto Variables() {
        return Base::Extend(
            Base::Globals::Variables(),
            Base::MakeVariables(Base::Var(&StoryGlobals::plot, "plot"),
                                Base::Var(&StoryGlobals::route, "route"),
                                Base::Var(&StoryGlobals::name, "name", "charname"),
                                Base::Var(&StoryGlobals::textSpeed, "textSpeed")));
    }
};

// 使用者が自分の分を足した形
struct ModGlobals : StoryGlobals {
    std::int64_t coins = 0;

    static constexpr auto Variables() {
        return Base::Extend(StoryGlobals::Variables(),
                            Base::MakeVariables(Base::Var(&ModGlobals::coins, "coins")));
    }
};

std::vector<std::string> NamesOf(const ModGlobals &globals) {
    std::vector<std::string> names;
    Base::ForEachVariable(globals, [&](const Base::VariableDescriptor &descriptor,
                                       Base::ConstVariableRef) {
        names.emplace_back(descriptor.name);
    });
    return names;
}

} // namespace

TEST_CASE("グローバルは名前と型を持つ") {
    StoryGlobals globals;
    static_assert(Base::VariableCount<StoryGlobals>() == 4);

    *Base::FindVariable(globals, "plot").As<std::int64_t>() = 12;
    *Base::FindVariable(globals, "route").As<Route>() = Route::Genocide;
    CHECK(globals.plot == 12);
    CHECK(globals.route == Route::Genocide);
}

TEST_CASE("型が違えば取り出せない") {
    StoryGlobals globals;
    const Base::VariableRef ref = Base::FindVariable(globals, "plot");
    REQUIRE(ref);
    CHECK(ref.As<std::int64_t>() != nullptr);
    CHECK(ref.As<double>() == nullptr);
    CHECK(ref.As<Route>() == nullptr);
}

TEST_CASE("原名でも引ける") {
    StoryGlobals globals;
    REQUIRE(Base::FindVariable(globals, "charname").As<std::string>() != nullptr);
    *Base::FindVariable(globals, "charname").As<std::string>() = "CHARA";
    CHECK(globals.name == "CHARA");
}

TEST_CASE("使用者が自分の分を足せる") {
    static_assert(Base::VariableCount<ModGlobals>() == 5);
    CHECK(NamesOf(ModGlobals{}) ==
          std::vector<std::string>{"plot", "route", "name", "textSpeed", "coins"});

    ModGlobals globals;
    *Base::FindVariable(globals, "coins").As<std::int64_t>() = 99;
    CHECK(globals.coins == 99);
}

TEST_CASE("宣言していないグローバルは存在しない") {
    StoryGlobals globals;
    CHECK(Base::GlobalExists(globals, "plot"));
    CHECK(Base::GlobalExists(globals, "charname"));
    CHECK_FALSE(Base::GlobalExists(globals, "coins"));
    CHECK_FALSE(Base::GlobalExists(globals, "存在しない"));

    ModGlobals extended;
    CHECK(Base::GlobalExists(extended, "coins"));
}

TEST_CASE("登録漏れも名前の重複も検査できる") {
    static_assert(Base::HasUniqueVariableNames<StoryGlobals>());
    static_assert(Base::HasUniqueVariableNames<ModGlobals>());

    CHECK(Base::CheckVariableCoverage(StoryGlobals{}).Ok());
    CHECK(Base::CheckVariableCoverage(ModGlobals{}).Ok());
}

TEST_CASE("丸ごと写して戻せる") {
    ModGlobals globals;
    globals.plot = 5;
    globals.name = "before";
    globals.coins = 1;

    const ModGlobals saved = globals;

    globals.plot = 99;
    globals.name = "after";
    globals.coins = 2;

    globals = saved;
    CHECK(globals.plot == 5);
    CHECK(globals.name == "before");
    CHECK(globals.coins == 1);
}

TEST_CASE("複数のグローバルが並び立つ") {
    ModGlobals first;
    ModGlobals second;
    first.plot = 1;
    second.plot = 2;

    CHECK(first.plot == 1);
    CHECK(second.plot == 2);
}
