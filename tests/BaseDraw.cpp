#include <Base/Canvas.hpp>
#include <Base/Context.hpp>
#include <Base/Draw.hpp>
#include <Base/GameObject.hpp>
#include <Base/Scheduler.hpp>
#include <Base/Variables.hpp>

#include "World.hpp"

#include <doctest/doctest.h>

#include <string>
#include <vector>

namespace Base = TellerEngine::Base;

namespace {

struct Painter : Base::GameObject {
    Base::Color tint{255, 0, 0, 255};

    void Draw(Base::Context &, Base::Canvas &canvas) override {
        canvas.SetColor(tint);
        canvas.SetAlpha(0.5);
        canvas.Rectangle(x, y, 10.0, 20.0);
    }

    static constexpr auto Variables() { return Base::GameObject::Variables(); }
};

struct Leaker : Base::GameObject {
    void Draw(Base::Context &, Base::Canvas &canvas) override {
        canvas.SetColor(Base::Color{1, 2, 3, 4});
        canvas.SetAlpha(0.25);
        canvas.SetBlend(Base::BlendMode::Add);
        canvas.Rectangle(0.0, 0.0, 1.0, 1.0);
    }

    static constexpr auto Variables() { return Base::GameObject::Variables(); }
};

struct Follower : Base::GameObject {
    void Draw(Base::Context &, Base::Canvas &canvas) override {
        canvas.Rectangle(0.0, 0.0, 1.0, 1.0);
    }

    static constexpr auto Variables() { return Base::GameObject::Variables(); }
};

} // namespace

TEST_CASE("積んだ順に並ぶ") {
    Base::DrawList list;
    Base::Canvas canvas{list};

    canvas.Sprite(Base::ImageId{3}, 1, 10.0, 20.0);
    canvas.Rectangle(0.0, 0.0, 5.0, 5.0);
    canvas.Text(1.0, 2.0, "ここ");

    REQUIRE(list.Size() == 3);
    CHECK(list.Commands()[0].kind == Base::DrawKind::Sprite);
    CHECK(list.Commands()[1].kind == Base::DrawKind::Rectangle);
    CHECK(list.Commands()[2].kind == Base::DrawKind::Text);
    CHECK(list.Commands()[0].image == Base::ImageId{3});
    CHECK(list.Commands()[0].frame == 1);
    CHECK(list.Commands()[0].x == doctest::Approx(10.0));
}

TEST_CASE("文字列は別に置いて位置で指す") {
    Base::DrawList list;
    Base::Canvas canvas{list};

    canvas.Text(0.0, 0.0, "さいしょ");
    canvas.Text(0.0, 0.0, "つぎ");

    CHECK(list.TextOf(list.Commands()[0]) == "さいしょ");
    CHECK(list.TextOf(list.Commands()[1]) == "つぎ");
}

TEST_CASE("色と透明度と混ぜ方は持ち回る") {
    Base::DrawList list;
    Base::Canvas canvas{list};

    canvas.SetColor(Base::Color{10, 20, 30, 255});
    canvas.SetAlpha(0.25);
    canvas.SetBlend(Base::BlendMode::Add);
    canvas.Rectangle(0.0, 0.0, 1.0, 1.0);
    canvas.Circle(0.0, 0.0, 2.0);

    for (const Base::DrawCommand &command : list.Commands()) {
        CHECK(command.color == Base::Color{10, 20, 30, 255});
        CHECK(command.alpha == doctest::Approx(0.25));
        CHECK(command.blend == Base::BlendMode::Add);
    }
}

TEST_CASE("塗るかどうかを選べる") {
    Base::DrawList list;
    Base::Canvas canvas{list};

    canvas.Rectangle(0.0, 0.0, 1.0, 1.0, false);
    canvas.Circle(0.0, 0.0, 1.0, true);
    CHECK_FALSE(list.Commands()[0].filled);
    CHECK(list.Commands()[1].filled);
}

TEST_CASE("同じ積み荷は同じ値になる") {
    Base::DrawList first;
    Base::DrawList second;
    Base::Canvas left{first};
    Base::Canvas right{second};

    for (Base::Canvas *canvas : {&left, &right}) {
        canvas->SetColor(Base::Color{1, 2, 3, 4});
        canvas->Sprite(Base::ImageId{7}, 2, 1.5, -2.5);
        canvas->Text(0.0, 0.0, "同じ");
    }

    CHECK(first.Hash() == second.Hash());
}

TEST_CASE("少しでも違えば値が変わる") {
    Base::DrawList base;
    Base::Canvas canvas{base};
    canvas.Sprite(Base::ImageId{7}, 2, 1.5, -2.5);
    const std::uint64_t original = base.Hash();

    Base::DrawList moved;
    Base::Canvas other{moved};
    other.Sprite(Base::ImageId{7}, 2, 1.5000001, -2.5);
    CHECK(moved.Hash() != original);

    Base::DrawList recolored;
    Base::Canvas third{recolored};
    third.SetColor(Base::Color{0, 0, 0, 255});
    third.Sprite(Base::ImageId{7}, 2, 1.5, -2.5);
    CHECK(recolored.Hash() != original);

    Base::DrawList retexted;
    Base::Canvas fourth{retexted};
    fourth.Text(0.0, 0.0, "あ");
    Base::DrawList retexted2;
    Base::Canvas fifth{retexted2};
    fifth.Text(0.0, 0.0, "い");
    CHECK(retexted.Hash() != retexted2.Hash());
}

TEST_CASE("空の積み荷どうしは等しい") {
    Base::DrawList first;
    Base::DrawList second;
    CHECK(first.Empty());
    CHECK(first.Hash() == second.Hash());
}

TEST_CASE("空にすると文字列も消える") {
    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.Text(0.0, 0.0, "消える");
    list.Clear();

    CHECK(list.Empty());
    canvas.Text(0.0, 0.0, "次");
    CHECK(list.TextOf(list.Commands()[0]) == "次");
}

TEST_CASE("誰が積んだかが残る") {
    TellerTest::World world;
    const Base::InstanceId id = world.context.Create<Painter>();
    world.instances.Find<Painter>(id)->x = 4.0;

    world.scheduler.Advance(world.context, world.canvas);

    REQUIRE(world.drawList.Size() == 1);
    const Base::DrawCommand &command = world.drawList.Commands()[0];
    CHECK(command.source == id);
    CHECK(command.x == doctest::Approx(4.0));
    CHECK(command.color == Base::Color{255, 0, 0, 255});
    CHECK(command.alpha == doctest::Approx(0.5));
}

TEST_CASE("反復のたびに積み直す") {
    TellerTest::World world;
    world.context.Create<Painter>();

    world.scheduler.Advance(world.context, world.canvas);
    CHECK(world.drawList.Size() == 1);

    world.scheduler.Advance(world.context, world.canvas);
    CHECK(world.drawList.Size() == 1);
}

TEST_CASE("前のインスタンスの色が次へ漏れない") {
    TellerTest::World world;
    world.context.Create<Leaker>();
    world.context.Create<Follower>();

    world.scheduler.Advance(world.context, world.canvas);

    REQUIRE(world.drawList.Size() == 2);
    const Base::DrawCommand &second = world.drawList.Commands()[1];
    CHECK(second.color == Base::Color{});
    CHECK(second.alpha == doctest::Approx(1.0));
    CHECK(second.blend == Base::BlendMode::Normal);
}

TEST_CASE("描かれないインスタンスは積まない") {
    TellerTest::World world;
    const Base::InstanceId id = world.context.Create<Painter>();
    world.instances.Find(id)->visible = false;

    world.scheduler.Advance(world.context, world.canvas);
    CHECK(world.drawList.Empty());
}

TEST_CASE("depthの順に積まれる") {
    TellerTest::World world;
    const Base::InstanceId front = world.context.Create<Painter>();
    const Base::InstanceId back = world.context.Create<Painter>();
    world.instances.Find(front)->depth = -10.0;
    world.instances.Find(back)->depth = 10.0;

    world.scheduler.Advance(world.context, world.canvas);

    REQUIRE(world.drawList.Size() == 2);
    CHECK(world.drawList.Commands()[0].source == back);
    CHECK(world.drawList.Commands()[1].source == front);
}

TEST_CASE("分割数の既定は本家と同じ24") {
    CHECK(Base::kDefaultCirclePrecision == 24);

    Base::DrawList list;
    Base::Canvas canvas{list};
    CHECK(canvas.CirclePrecision() == 24);

    canvas.SetCirclePrecision(48);
    CHECK(canvas.CirclePrecision() == 48);

    // 4より小さくはならない
    canvas.SetCirclePrecision(1);
    CHECK(canvas.CirclePrecision() == 4);
}

TEST_CASE("勾配を指定しなければ全ての角が同じ色") {
    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SetColor(Base::Color{10, 20, 30, 255});
    canvas.Rectangle(0.0, 0.0, 1.0, 1.0);

    const Base::DrawCommand &command = list.Commands()[0];
    for (const Base::Color &extra : command.extra) {
        CHECK(extra == command.color);
    }
}

TEST_CASE("線の太さが積み荷に入る") {
    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.Line(0.0, 0.0, 1.0, 1.0);
    canvas.Line(0.0, 0.0, 1.0, 1.0, 8.0);

    CHECK(list.Commands()[0].lineWidth == doctest::Approx(1.0));
    CHECK(list.Commands()[1].lineWidth == doctest::Approx(8.0));
    CHECK(list.Hash() != Base::DrawList{}.Hash());
}

TEST_CASE("角で指した矩形は大きさに直る") {
    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.RectangleCorners(10.0, 20.0, 40.0, 60.0);

    const Base::DrawCommand &command = list.Commands()[0];
    CHECK(command.x == doctest::Approx(10.0));
    CHECK(command.width == doctest::Approx(30.0));
    CHECK(command.height == doctest::Approx(40.0));
}

TEST_CASE("インスタンスごとに分割数も戻る") {
    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SetCirclePrecision(4);
    canvas.Reset();
    CHECK(canvas.CirclePrecision() == Base::kDefaultCirclePrecision);
}

TEST_CASE("切り出す範囲が積み荷に入る") {
    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.Sprite(Base::ImageId{1}, 0, 0.0, 0.0);
    canvas.SpritePart(Base::ImageId{1}, 0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0);
    canvas.SpriteStretched(Base::ImageId{1}, 0, 0.0, 0.0, 30.0, 40.0);

    CHECK_FALSE(list.Commands()[0].usePart);
    CHECK_FALSE(list.Commands()[0].ignoreOrigin);

    const Base::DrawCommand &part = list.Commands()[1];
    CHECK(part.usePart);
    CHECK(part.ignoreOrigin);
    CHECK(part.partX == doctest::Approx(2.0));
    CHECK(part.partY == doctest::Approx(3.0));
    CHECK(part.partWidth == doctest::Approx(4.0));
    CHECK(part.partHeight == doctest::Approx(5.0));
    CHECK(part.x == doctest::Approx(6.0));

    const Base::DrawCommand &stretched = list.Commands()[2];
    CHECK_FALSE(stretched.usePart);
    CHECK(stretched.ignoreOrigin);
    CHECK(stretched.width == doctest::Approx(30.0));
    CHECK(stretched.height == doctest::Approx(40.0));
}

TEST_CASE("切り出す範囲が違えば値も変わる") {
    Base::DrawList first;
    Base::DrawList second;
    Base::Canvas left{first};
    Base::Canvas right{second};
    left.SpritePart(Base::ImageId{1}, 0, 0.0, 0.0, 8.0, 8.0, 0.0, 0.0);
    right.SpritePart(Base::ImageId{1}, 0, 8.0, 0.0, 8.0, 8.0, 0.0, 0.0);
    CHECK(first.Hash() != second.Hash());
}

TEST_CASE("描画先の切り替えも積み荷に入る") {
    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SetTarget(Base::ImageId{5});
    canvas.Rectangle(0.0, 0.0, 1.0, 1.0);
    canvas.ResetTarget();

    REQUIRE(list.Size() == 3);
    CHECK(list.Commands()[0].kind == Base::DrawKind::Target);
    CHECK(list.Commands()[0].image == Base::ImageId{5});
    CHECK(list.Commands()[2].kind == Base::DrawKind::Target);
    CHECK(list.Commands()[2].image == Base::ImageId::None);
}
