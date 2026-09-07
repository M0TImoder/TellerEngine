#pragma once

#include <Base/Timers.hpp>
#include <Base/Variables.hpp>

#include <cstdint>

namespace TellerEngine::Base {

class Context;

// インスタンスを指す値
// Noneはどのインスタンスも指さない
enum class InstanceId : std::uint32_t {
    None = 0,
};

// VanillaとTellerが継承する共通の基底
class GameObject {
public:
    GameObject() = default;
    virtual ~GameObject() = default;

    GameObject(const GameObject &) = default;
    GameObject &operator=(const GameObject &) = default;
    GameObject(GameObject &&) = default;
    GameObject &operator=(GameObject &&) = default;

    // 生成した直後に1度だけ呼ばれる
    virtual void Create(Context &context) { (void)context; }

    // 破棄する直前に1度だけ呼ばれる
    virtual void Destroy(Context &context) { (void)context; }

    virtual void BeginStep(Context &context) { (void)context; }
    virtual void Alarm(Context &context) { (void)context; }
    virtual void Step(Context &context) { (void)context; }
    virtual void Collision(Context &context, GameObject &other) {
        (void)context;
        (void)other;
    }
    virtual void EndStep(Context &context) { (void)context; }
    virtual void Draw(Context &context) { (void)context; }

    InstanceId id = InstanceId::None;
    double x = 0.0;
    double y = 0.0;

    // 小さいほど手前に描かれる
    double depth = 0.0;

    bool visible = true;

    // falseの間はどのフェーズにも入らず、名前でも引けない
    bool active = true;

    static constexpr auto Variables() {
        return MakeVariables(Var(&GameObject::id, "id"), Var(&GameObject::x, "x"),
                             Var(&GameObject::y, "y"), Var(&GameObject::depth, "depth"),
                             Var(&GameObject::visible, "visible"),
                             Var(&GameObject::active, "active"));
    }

    static constexpr auto Timers() { return MakeTimers(); }
};

} // namespace TellerEngine::Base
