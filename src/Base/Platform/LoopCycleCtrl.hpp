#pragma once

#include <Base/Loop.hpp>

#include <SDL3/SDL.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#include <cstdint>

namespace TellerEngine::Base::Platform {

// 1反復で論理を何回進めるかの決め方
enum class StepPolicy {
    Locked,
    Accumulated,
};

// 反復のたびに論理と表示を回し、次の反復まで待つ
class LoopCycleCtrl {
public:
    explicit LoopCycleCtrl(LoopCycle &cycle) : cycle_(cycle), last_(SDL_GetTicksNS()) {}

    LoopCycleCtrl(const LoopCycleCtrl &) = delete;
    LoopCycleCtrl &operator=(const LoopCycleCtrl &) = delete;

    virtual ~LoopCycleCtrl() = default;

    // 1反復
    virtual void Update() {
        const std::uint64_t now = SDL_GetTicksNS();
        const std::uint64_t elapsed = now - last_;
        last_ = now;

        steps_ = Advance(elapsed);
        Present();
        Wait();
        iterations_ += 1;
    }

    void SetPolicy(StepPolicy policy) { policy_ = policy; }
    StepPolicy Policy() const { return policy_; }

    void SetMaxSteps(int maxSteps) { maxSteps_ = maxSteps; }
    int MaxSteps() const { return maxSteps_; }

    // 直前の反復で論理を進めた回数
    int LastSteps() const { return steps_; }

    std::uint64_t Iterations() const { return iterations_; }

    LoopCycle &Cycle() { return cycle_; }

protected:
    // 論理を1回進める
    virtual void Step() {}

    // 画面へ出す
    virtual void Present() {}

    // 次の反復まで待つ
    virtual void Wait() {
        const double interval = cycle_.DisplayInterval();
        if (interval <= 0.0) {
            return;
        }
        const auto target = static_cast<std::uint64_t>(interval * 1000000.0);
        const std::uint64_t spent = SDL_GetTicksNS() - last_;
        if (spent >= target) {
            return;
        }
#if defined(__EMSCRIPTEN__)
        // ここでスタックを巻き戻し、次の描画機会で同じ場所から再開する
        emscripten_sleep(static_cast<unsigned int>((target - spent) / 1000000));
#else
        SDL_DelayNS(target - spent);
#endif
    }

    // 経過した実時間から論理を進める
    int Advance(std::uint64_t elapsedNanoseconds) {
        // 1000で割った端数を持ち越す
        nanoseconds_ += static_cast<std::int64_t>(elapsedNanoseconds);
        const std::int64_t microseconds = nanoseconds_ / 1000;
        nanoseconds_ -= microseconds * 1000;
        cycle_.AddElapsedMicroseconds(microseconds);

        // 論理と表示が同じレートなら本家と同じく1反復1ステップにする
        if (policy_ == StepPolicy::Locked && cycle_.LogicRate() == cycle_.DisplayRate() &&
            cycle_.LogicRate() > 0) {
            cycle_.DropPending();
            Step();
            return 1;
        }

        int steps = 0;
        while (steps < maxSteps_ && cycle_.ConsumeStep()) {
            Step();
            steps += 1;
        }
        if (steps == maxSteps_) {
            cycle_.DropPending();
        }
        return steps;
    }

private:
    LoopCycle &cycle_;
    StepPolicy policy_ = StepPolicy::Locked;
    int maxSteps_ = 8;
    int steps_ = 0;
    std::int64_t nanoseconds_ = 0;
    std::uint64_t iterations_ = 0;
    std::uint64_t last_ = 0;
};

} // namespace TellerEngine::Base::Platform
