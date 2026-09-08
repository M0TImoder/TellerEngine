#pragma once

#include <Base/Canvas.hpp>
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
    void Advance(Context &context, Canvas &canvas) {
        const auto boundary = static_cast<std::uint32_t>(context.instances.NextId());

        RunPhase(context, boundary, &GameObject::BeginStep);
        RunPhase(context, boundary, &GameObject::Alarm);
        RunPhase(context, boundary, &GameObject::Step);
        if (collision_ != nullptr) {
            collision_(context);
        }
        RunPhase(context, boundary, &GameObject::EndStep);

        // このフレームに生まれたものも描く
        canvas.List().Clear();
        context.instances.ForEachByDepth([&context, &canvas](GameObject &object) {
            if (object.visible) {
                canvas.Reset();
                canvas.SetSource(object.id);
                object.Draw(context, canvas);
            }
        });

        context.instances.Collect();

        // 次のステップから見た「1つ前」を確定させる
        context.AdvanceInput();
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
