#include <Base/Context.hpp>
#include <Base/Canvas.hpp>
#include <Base/Draw.hpp>
#include <Base/GameObject.hpp>
#include <Base/Globals.hpp>
#include <Base/Instances.hpp>
#include <Base/Loop.hpp>
#include <Base/Random.hpp>
#include <Base/Platform/DebugPanels.hpp>
#include <Base/Platform/DebugUi.hpp>
#include <Base/Platform/SdlRenderer.hpp>
#include <Base/Platform/System.hpp>
#include <Base/Platform/Window.hpp>

#include <doctest/doctest.h>

#include <SDL3/SDL.h>

#ifdef TELLER_DEBUG_UI
#include <imgui.h>
#endif

#include <string>
#include <vector>

namespace Base = TellerEngine::Base;
namespace Platform = TellerEngine::Base::Platform;

namespace {

struct Heart : Base::GameObject {
    static constexpr auto Variables() { return Base::GameObject::Variables(); }
};

struct Bullet : Base::GameObject {
    static constexpr auto Variables() { return Base::GameObject::Variables(); }
};

struct HomingBullet : Bullet {
    static constexpr auto Variables() { return Bullet::Variables(); }
};

struct Tracked : Base::GameObject {
    double speed = 1.5;
    int hits = 0;
    bool armed = true;
    std::string tag = "つよい";
    Base::Duration wait{30.0};

    static constexpr auto Variables() {
        return Base::Extend(Base::GameObject::Variables(),
                            Base::MakeVariables(Base::Var(&Tracked::speed, "speed", "spd"),
                                                Base::Var(&Tracked::hits, "hits"),
                                                Base::Var(&Tracked::armed, "armed"),
                                                Base::Var(&Tracked::tag, "tag"),
                                                Base::Var(&Tracked::wait, "wait")));
    }
};

bool Has(const std::string &text, const char *part) {
    return text.find(part) != std::string::npos;
}

// 窓に渡すひとそろい
struct World {
    Base::Instances instances;
    Base::Globals globals;
    Base::Random random;
    Base::LoopCycle cycle;
    Base::Input input;
    Base::Context context{instances, globals, random, cycle.Clock(), input};
};

// 破棄されたことを覚える
struct Fading : Base::GameObject {
    static inline int gone = 0;

    void Destroy(Base::Context &) override { gone += 1; }

    static constexpr auto Variables() { return Base::GameObject::Variables(); }
};

} // namespace

TEST_CASE("型の名前が読める形で取れる") {
    Base::Instances instances;
    instances.Create<Heart>();

    Base::GameObject *object = instances.First<Heart>();
    REQUIRE(object != nullptr);
    CHECK(Has(Platform::TypeNameOf(*object), "Heart"));
}

TEST_CASE("種別ごとにまとまる") {
    Base::Instances instances;
    instances.Create<Heart>();
    const Base::InstanceId first = instances.Create<Bullet>();
    const Base::InstanceId second = instances.Create<Bullet>();
    instances.Create<HomingBullet>();

    const auto groups = Platform::GroupInstances(instances);
    REQUIRE(groups.size() == 3);

    // 種別は名前の順
    CHECK(Has(groups[0].type, "Bullet"));
    CHECK(Has(groups[1].type, "Heart"));
    CHECK(Has(groups[2].type, "HomingBullet"));

    // インスタンスは生成順
    REQUIRE(groups[0].instances.size() == 2);
    CHECK(groups[0].instances[0] == first);
    CHECK(groups[0].instances[1] == second);
}

TEST_CASE("派生した型は別のまとまりになる") {
    Base::Instances instances;
    instances.Create<Bullet>();
    instances.Create<HomingBullet>();

    const auto groups = Platform::GroupInstances(instances);
    CHECK(groups.size() == 2);
}

TEST_CASE("activeでないものも並ぶ") {
    Base::Instances instances;
    const Base::InstanceId id = instances.Create<Heart>();
    instances.Create<Heart>();
    instances.Deactivate(id);

    const auto groups = Platform::GroupInstances(instances);
    REQUIRE(groups.size() == 1);
    CHECK(groups[0].instances.size() == 2);
}

TEST_CASE("破棄したものは並ばない") {
    Base::Instances instances;
    const Base::InstanceId id = instances.Create<Heart>();
    instances.Create<Heart>();
    instances.Destroy(id);

    const auto groups = Platform::GroupInstances(instances);
    REQUIRE(groups.size() == 1);
    CHECK(groups[0].instances.size() == 1);
}

TEST_CASE("1つも無ければまとまりも無い") {
    Base::Instances instances;
    CHECK(Platform::GroupInstances(instances).empty());
}

TEST_CASE("基底越しでも派生の変数まで回る") {
    Base::Instances instances;
    const Base::InstanceId id = instances.Create<Tracked>();

    std::vector<std::string> names;
    instances.WithVariables(id, [&names](const Base::VariableDescriptor &descriptor,
                                         Base::VariableRef) {
        names.emplace_back(descriptor.name);
    });

    REQUIRE(names.size() == Base::VariableCount<Tracked>());
    CHECK(names.front() == "id");
    CHECK(names[6] == "speed");
    CHECK(names.back() == "wait");
}

TEST_CASE("表から書き換えられる") {
    Base::Instances instances;
    const Base::InstanceId id = instances.Create<Tracked>();

    instances.WithVariables(id, [](const Base::VariableDescriptor &descriptor,
                                   Base::VariableRef ref) {
        if (descriptor.name == "speed") {
            *ref.As<double>() = 4.0;
        }
        if (descriptor.name == "hits") {
            *ref.As<int>() = 7;
        }
        if (descriptor.name == "tag") {
            *ref.As<std::string>() = "よわい";
        }
        if (descriptor.name == "wait") {
            *ref.As<Base::Duration>() = Base::Duration{5.0};
        }
    });

    const Tracked *object = instances.Find<Tracked>(id);
    REQUIRE(object != nullptr);
    CHECK(object->speed == doctest::Approx(4.0));
    CHECK(object->hits == 7);
    CHECK(object->tag == "よわい");
    CHECK(object->wait.Value() == doctest::Approx(5.0));
}

TEST_CASE("原名も一緒に取れる") {
    Base::Instances instances;
    const Base::InstanceId id = instances.Create<Tracked>();

    std::string original;
    instances.WithVariables(id, [&original](const Base::VariableDescriptor &descriptor,
                                            Base::VariableRef) {
        if (descriptor.name == "speed") {
            original = descriptor.original;
        }
    });
    CHECK(original == "spd");
}

TEST_CASE("無いインスタンスは1つも回らない") {
    Base::Instances instances;
    const Base::InstanceId id = instances.Create<Tracked>();
    instances.Destroy(id);

    std::size_t seen = 0;
    instances.WithVariables(id, [&seen](const Base::VariableDescriptor &, Base::VariableRef) {
        seen += 1;
    });
    instances.WithVariables(Base::InstanceId::None,
                            [&seen](const Base::VariableDescriptor &, Base::VariableRef) {
                                seen += 1;
                            });
    CHECK(seen == 0);
}

TEST_CASE("巻き戻した後も表を回せる") {
    Base::Instances instances;
    const Base::InstanceId id = instances.Create<Tracked>();
    const auto snapshot = instances.Save();

    instances.Find<Tracked>(id)->hits = 99;
    instances.Restore(snapshot);

    int hits = -1;
    instances.WithVariables(id, [&hits](const Base::VariableDescriptor &descriptor,
                                        Base::VariableRef ref) {
        if (descriptor.name == "hits") {
            hits = *ref.As<int>();
        }
    });
    CHECK(hits == 0);
}

TEST_CASE("書き換えられる種類が決まっている") {
    CHECK(Platform::Editable(Base::VariableKind::Boolean));
    CHECK(Platform::Editable(Base::VariableKind::Integer));
    CHECK(Platform::Editable(Base::VariableKind::Number));
    CHECK(Platform::Editable(Base::VariableKind::Text));
    CHECK(Platform::Editable(Base::VariableKind::Duration));
    CHECK(Platform::Editable(Base::VariableKind::Velocity));
    CHECK(Platform::Editable(Base::VariableKind::Acceleration));

    // 取りうる値が分からないものは触らせない
    CHECK_FALSE(Platform::Editable(Base::VariableKind::Enumeration));
    CHECK_FALSE(Platform::Editable(Base::VariableKind::Other));
}

// ブラウザの外ではSDLが画面を作れない
#if !defined(__EMSCRIPTEN__)
TEST_CASE("ヒエラルキーの窓を出せる") {
    if constexpr (!Platform::DebugUi::Enabled()) {
        MESSAGE("デバッグUIが入っていないので飛ばす");
        return;
    }

    auto system = Platform::System::Create(SDL_INIT_VIDEO);
    if (!system.has_value()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }
    auto window = Platform::Window::Create("TellerHierarchy", 320, 240);
    REQUIRE(window.has_value());
    auto renderer = Platform::SdlRenderer::Create(*window);
    REQUIRE(renderer.has_value());

    auto ui = Platform::DebugUi::Create(window->Handle(), renderer->Handle());
    REQUIRE(ui.has_value());

    // 本体の窓の中だけで確かめる
    ui->SetViewports(false);

    World world;
    Base::Instances &instances = world.instances;
    const Base::InstanceId id = instances.Create<Heart>();
    instances.Create<Bullet>();
    instances.Create<Bullet>();

    Platform::DebugSelection selection;
    selection.instance = id;

    bool open = true;
    for (int i = 0; i < 3; ++i) {
        ui->NewFrame();
        Platform::DrawHierarchy("hierarchy", open, world.context, "room_battle", selection);
        ui->Render(renderer->Handle());
    }

    CHECK(open);
    CHECK(ui->DrawCount() > 0);
    CHECK(selection.instance == id);

    // 閉じていれば何も積まれない
    open = false;
    ui->NewFrame();
    Platform::DrawHierarchy("hierarchy", open, world.context, "room_battle", selection);
    ui->Render(renderer->Handle());
    CHECK(ui->DrawCount() == 0);
}
#endif

// ブラウザの外ではSDLが画面を作れない
#if !defined(__EMSCRIPTEN__)
TEST_CASE("インスペクタの窓を出せる") {
    if constexpr (!Platform::DebugUi::Enabled()) {
        MESSAGE("デバッグUIが入っていないので飛ばす");
        return;
    }

    auto system = Platform::System::Create(SDL_INIT_VIDEO);
    if (!system.has_value()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }
    auto window = Platform::Window::Create("TellerInspector", 320, 240);
    REQUIRE(window.has_value());
    auto renderer = Platform::SdlRenderer::Create(*window);
    REQUIRE(renderer.has_value());

    auto ui = Platform::DebugUi::Create(window->Handle(), renderer->Handle());
    REQUIRE(ui.has_value());
    ui->SetViewports(false);

    World world;
    Base::Instances &instances = world.instances;
    const Base::InstanceId id = instances.Create<Tracked>();

    Platform::DebugSelection selection;
    bool open = true;

    // 選んでいなくても出せる
    for (int i = 0; i < 3; ++i) {
        ui->NewFrame();
        Platform::DrawInspector("inspector", open, world.context, selection);
        ui->Render(renderer->Handle());
    }
    const std::size_t empty = ui->DrawCount();
    CHECK(empty > 0);

    selection.instance = id;
    for (int i = 0; i < 3; ++i) {
        ui->NewFrame();
        Platform::DrawInspector("inspector", open, world.context, selection);
        ui->Render(renderer->Handle());
    }

    // 変数が並ぶぶんだけ増える
    CHECK(ui->DrawCount() > empty);
    CHECK(open);

    open = false;
    ui->NewFrame();
    Platform::DrawInspector("inspector", open, world.context, selection);
    ui->Render(renderer->Handle());
    CHECK(ui->DrawCount() == 0);
}
#endif

TEST_CASE("消すと破棄の手続きが通る") {
    World world;
    Fading::gone = 0;

    const Base::InstanceId id = world.context.Create<Fading>();
    world.instances.Create<Fading>();

    Platform::DebugSelection selection;
    selection.instance = id;

    Platform::DestroyInstance(world.context, selection, id);
    CHECK(Fading::gone == 1);
    CHECK(world.instances.Count() == 1);

    // 選んでいたものなら選択も外れる
    CHECK(selection.instance == Base::InstanceId::None);
}

TEST_CASE("選んでいないものを消しても選択は残る") {
    World world;
    Fading::gone = 0;

    const Base::InstanceId kept = world.context.Create<Fading>();
    const Base::InstanceId id = world.context.Create<Fading>();

    Platform::DebugSelection selection;
    selection.instance = kept;

    Platform::DestroyInstance(world.context, selection, id);
    CHECK(Fading::gone == 1);
    CHECK(selection.instance == kept);
}

TEST_CASE("無いものを消しても何も起きない") {
    World world;
    Fading::gone = 0;

    const Base::InstanceId id = world.context.Create<Fading>();
    Platform::DebugSelection selection;
    Platform::DestroyInstance(world.context, selection, id);
    CHECK(Fading::gone == 1);

    // 二度目は通らない
    Platform::DestroyInstance(world.context, selection, id);
    Platform::DestroyInstance(world.context, selection, Base::InstanceId::None);
    CHECK(Fading::gone == 1);
}

TEST_CASE("消したものはヒエラルキーから消える") {
    World world;
    const Base::InstanceId id = world.context.Create<Heart>();
    world.context.Create<Heart>();

    Platform::DebugSelection selection;
    Platform::DestroyInstance(world.context, selection, id);

    const auto groups = Platform::GroupInstances(world.instances);
    REQUIRE(groups.size() == 1);
    CHECK(groups[0].instances.size() == 1);
    CHECK(groups[0].instances[0] != id);
}

TEST_CASE("種類の名前が全て付いている") {
    CHECK(std::string{Platform::NameOf(Base::DrawKind::Sprite)} == "sprite");
    CHECK(std::string{Platform::NameOf(Base::DrawKind::Rectangle)} == "rectangle");
    CHECK(std::string{Platform::NameOf(Base::DrawKind::RoundRectangle)} == "roundrect");
    CHECK(std::string{Platform::NameOf(Base::DrawKind::Line)} == "line");
    CHECK(std::string{Platform::NameOf(Base::DrawKind::Circle)} == "circle");
    CHECK(std::string{Platform::NameOf(Base::DrawKind::Ellipse)} == "ellipse");
    CHECK(std::string{Platform::NameOf(Base::DrawKind::Triangle)} == "triangle");
    CHECK(std::string{Platform::NameOf(Base::DrawKind::Text)} == "text");
    CHECK(std::string{Platform::NameOf(Base::DrawKind::Target)} == "target");

    CHECK(std::string{Platform::NameOf(Base::BlendMode::Normal)} == "normal");
    CHECK(std::string{Platform::NameOf(Base::BlendMode::None)} == "none");
    CHECK(std::string{Platform::NameOf(Base::BlendMode::Add)} == "add");
    CHECK(std::string{Platform::NameOf(Base::BlendMode::Subtract)} == "subtract");
    CHECK(std::string{Platform::NameOf(Base::BlendMode::Multiply)} == "multiply");
}

// ブラウザの外ではSDLが画面を作れない
#if !defined(__EMSCRIPTEN__)
TEST_CASE("積み荷の窓を出せる") {
    if constexpr (!Platform::DebugUi::Enabled()) {
        MESSAGE("デバッグUIが入っていないので飛ばす");
        return;
    }

    auto system = Platform::System::Create(SDL_INIT_VIDEO);
    if (!system.has_value()) {
        MESSAGE("画面が無いので飛ばす");
        return;
    }
    auto window = Platform::Window::Create("TellerCommands", 320, 240);
    REQUIRE(window.has_value());
    auto renderer = Platform::SdlRenderer::Create(*window);
    REQUIRE(renderer.has_value());

    auto ui = Platform::DebugUi::Create(window->Handle(), renderer->Handle());
    REQUIRE(ui.has_value());
    ui->SetViewports(false);

    // 種類をひととおり積む
    Base::DrawList list;
    Base::Canvas canvas{list};
    canvas.SetSource(static_cast<Base::InstanceId>(1));
    canvas.Rectangle(10.0, 20.0, 30.0, 40.0);
    canvas.Circle(50.0, 60.0, 12.0);
    canvas.Line(0.0, 0.0, 20.0, 20.0, 2.0);
    canvas.Text(4.0, 8.0, "howdy");
    canvas.SetSource(static_cast<Base::InstanceId>(2));
    canvas.Ellipse(70.0, 80.0, 20.0, 10.0);
    canvas.Triangle(0.0, 0.0, 5.0, 0.0, 0.0, 5.0);
    REQUIRE(list.Size() == 6);

    Platform::DebugSelection selection;
    selection.instance = static_cast<Base::InstanceId>(1);
    bool onlySelected = false;
    bool open = true;

    const auto run = [&]() {
        for (int i = 0; i < 3; ++i) {
            ui->NewFrame();
            Platform::DrawCommandList("commands", open, list, selection, onlySelected);
            ui->Render(renderer->Handle());
        }
        return ui->DrawCount();
    };

    const std::size_t all = run();
    CHECK(all > 0);

    // 絞ると出る量が減る
    onlySelected = true;
    const std::size_t some = run();
    CHECK(some > 0);
    CHECK(some < all);

    // 積み荷が無ければ表は空
    Base::DrawList emptyList;
    onlySelected = false;
    ui->NewFrame();
    Platform::DrawCommandList("commands", open, emptyList, selection, onlySelected);
    ui->Render(renderer->Handle());
    const std::size_t nothing = ui->DrawCount();
    CHECK(nothing > 0);
    CHECK(nothing < all);

    open = false;
    ui->NewFrame();
    Platform::DrawCommandList("commands", open, list, selection, onlySelected);
    ui->Render(renderer->Handle());
    CHECK(ui->DrawCount() == 0);
}
#endif
