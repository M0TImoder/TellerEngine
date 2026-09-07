#include <Base/VariableCheck.hpp>
#include <Base/Variables.hpp>

#include <doctest/doctest.h>

#include <cstdint>
#include <string>

namespace Base = TellerEngine::Base;

namespace {

struct Complete {
    double x = 0.0;
    double y = 0.0;
    std::int64_t hp = 20;

    static constexpr auto Variables() {
        return Base::MakeVariables(Base::Var(&Complete::x, "x"), Base::Var(&Complete::y, "y"),
                                   Base::Var(&Complete::hp, "hp"));
    }
};

// hpを登録し忘れた形
struct Forgotten {
    double x = 0.0;
    double y = 0.0;
    std::int64_t hp = 20;

    static constexpr auto Variables() {
        return Base::MakeVariables(Base::Var(&Forgotten::x, "x"),
                                   Base::Var(&Forgotten::y, "y"));
    }
};

// 先頭と末尾の両方を登録し忘れた形
struct ForgottenEnds {
    std::int64_t head = 0;
    double middle = 0.0;
    std::int64_t tail = 0;

    static constexpr auto Variables() {
        return Base::MakeVariables(Base::Var(&ForgottenEnds::middle, "middle"));
    }
};

struct Polymorphic {
    virtual ~Polymorphic() = default;

    double x = 0.0;
    double y = 0.0;

    static constexpr auto Variables() {
        return Base::MakeVariables(Base::Var(&Polymorphic::x, "x"),
                                   Base::Var(&Polymorphic::y, "y"));
    }
};

struct DuplicateName {
    double first = 0.0;
    double second = 0.0;

    static constexpr auto Variables() {
        return Base::MakeVariables(Base::Var(&DuplicateName::first, "x"),
                                   Base::Var(&DuplicateName::second, "x"));
    }
};

// 別名が他の変数の名前とぶつかる形
struct CollidingOriginal {
    double first = 0.0;
    double second = 0.0;

    static constexpr auto Variables() {
        return Base::MakeVariables(Base::Var(&CollidingOriginal::first, "moveSpeed"),
                                   Base::Var(&CollidingOriginal::second, "charge",
                                             "moveSpeed"));
    }
};

// 同じメンバを2回登録した形
struct Overlapping {
    double x = 0.0;
    double y = 0.0;

    static constexpr auto Variables() {
        return Base::MakeVariables(Base::Var(&Overlapping::x, "x"),
                                   Base::Var(&Overlapping::x, "xAlias"),
                                   Base::Var(&Overlapping::y, "y"));
    }
};

} // namespace

TEST_CASE("全部登録してあれば隙間が出ない") {
    const Base::VariableCoverage coverage = Base::CheckVariableCoverage(Complete{});
    CHECK(coverage.Ok());
    CHECK(coverage.gapCount == 0);
    CHECK(coverage.overlapCount == 0);
    CHECK(coverage.objectSize == sizeof(Complete));
    CHECK(coverage.registeredBytes == sizeof(double) * 2 + sizeof(std::int64_t));
}

TEST_CASE("末尾の登録漏れを見つける") {
    const Base::VariableCoverage coverage = Base::CheckVariableCoverage(Forgotten{});
    CHECK_FALSE(coverage.Ok());
    REQUIRE(coverage.gapCount == 1);
    CHECK(coverage.gaps[0].offset == sizeof(double) * 2);
    CHECK(coverage.gaps[0].size == sizeof(std::int64_t));
}

TEST_CASE("先頭と末尾の登録漏れを両方見つける") {
    const Base::VariableCoverage coverage = Base::CheckVariableCoverage(ForgottenEnds{});
    CHECK_FALSE(coverage.Ok());
    REQUIRE(coverage.gapCount == 2);
    CHECK(coverage.gaps[0].offset == 0);
    CHECK(coverage.gaps[0].size == sizeof(std::int64_t));
    CHECK(coverage.gaps[1].size == sizeof(std::int64_t));
}

TEST_CASE("多相なクラスの先頭は隙間として挙げない") {
    const Polymorphic object;
    const Base::VariableCoverage coverage = Base::CheckVariableCoverage(object);
    CHECK(coverage.Ok());
}

TEST_CASE("同じメンバを2回登録すると重なりが出る") {
    const Base::VariableCoverage coverage = Base::CheckVariableCoverage(Overlapping{});
    CHECK_FALSE(coverage.Ok());
    CHECK(coverage.overlapCount == 1);
}

TEST_CASE("名前の重複をコンパイル時に見つける") {
    static_assert(Base::HasUniqueVariableNames<Complete>());
    static_assert(!Base::HasUniqueVariableNames<DuplicateName>());
    static_assert(!Base::HasUniqueVariableNames<CollidingOriginal>());
}

TEST_CASE("登録した合計はクラスの大きさに収まる") {
    static_assert(Base::VariablesFitInObject<Complete>());
    static_assert(Base::VariablesFitInObject<Forgotten>());
    static_assert(Base::VariablesFitInObject<Polymorphic>());
}
