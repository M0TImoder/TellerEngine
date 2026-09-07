#pragma once

#include <Base/Context.hpp>
#include <Base/GameObject.hpp>
#include <Base/Instances.hpp>

#include <cstdint>

namespace TellerEngine::Base {

// 1論理ステップの中でフックを呼ぶ順序を持つ
class Scheduler {
public:
    using CollisionResolver = void (*)(Context &context);

    // BeginStepからDrawまでを1回進める
    void Advance(Context &context) {
        const auto boundary = static_cast<std::uint32_t>(context.instances.NextId());

        RunPhase(context, boundary, &GameObject::BeginStep);
        RunPhase(context, boundary, &GameObject::Alarm);
        RunPhase(context, boundary, &GameObject::Step);
        if (collision_ != nullptr) {
            collision_(context);
        }
        RunPhase(context, boundary, &GameObject::EndStep);

        // このフレームに生まれたものも描く
        context.instances.ForEachByDepth([&context](GameObject &object) {
            if (object.visible) {
                object.Draw(context);
            }
        });

        context.instances.Collect();
        frame_ += 1;
    }

    void SetCollisionResolver(CollisionResolver resolver) { collision_ = resolver; }

    std::uint64_t Frame() const { return frame_; }
    void SetFrame(std::uint64_t frame) { frame_ = frame; }

private:
    // このフレームに生まれたインスタンスはStep系のフェーズに入らない
    static void RunPhase(Context &context, std::uint32_t boundary,
                         void (GameObject::*phase)(Context &)) {
        context.instances.ForEach([&](GameObject &object) {
            if (static_cast<std::uint32_t>(object.id) < boundary) {
                (object.*phase)(context);
            }
        });
    }

    CollisionResolver collision_ = nullptr;
    std::uint64_t frame_ = 0;
};

} // namespace TellerEngine::Base
