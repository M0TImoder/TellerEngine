#pragma once

#include <Base/GameObject.hpp>
#include <Base/Globals.hpp>
#include <Base/Instances.hpp>
#include <Base/Loop.hpp>
#include <Base/Random.hpp>

#include <utility>

namespace TellerEngine::Base {

// フックが受け取る文脈
// インスタンスの一覧とグローバルと乱数への入り口になる
class Context {
public:
    Context(Instances &instances, Globals &globals, Random &random, const VirtualClock &clock)
        : instances(instances), globals(globals), random(random), clock(clock) {}

    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;

    Instances &instances;
    Globals &globals;
    Random &random;

    // 本家のcurrent_timeにあたる
    const VirtualClock &clock;

    // 宣言した型と合わなければnullptr
    template <typename T> T *Global() { return dynamic_cast<T *>(&globals); }

    template <typename T> const T *Global() const { return dynamic_cast<const T *>(&globals); }

    // 生成してCreateを呼ぶ
    template <typename T, typename... Args> InstanceId Create(Args &&...args) {
        const InstanceId id = instances.Create<T>(std::forward<Args>(args)...);
        instances.Find(id)->Create(*this);
        return id;
    }

    // Destroyを呼んでから存在しなくする
    void Destroy(InstanceId id) {
        GameObject *object = instances.Find(id);
        if (object == nullptr) {
            return;
        }
        object->Destroy(*this);
        instances.Destroy(id);
    }
};

} // namespace TellerEngine::Base
