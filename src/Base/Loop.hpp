#pragma once

#include <cstdint>

namespace TellerEngine::Base {

// 1秒あたりの回数
struct LoopRates {
    int logic = 30;
    int display = 30;
};

// ゲームコードが読む唯一の実時間
class VirtualClock {
public:
    constexpr std::uint64_t Milliseconds() const { return microseconds_ / 1000; }
    constexpr std::uint64_t Microseconds() const { return microseconds_; }
    constexpr void Advance(std::uint64_t delta) { microseconds_ += delta; }
    constexpr void SetMicroseconds(std::uint64_t microseconds) { microseconds_ = microseconds; }

private:
    std::uint64_t microseconds_ = 0;
};

// 経過した実時間を論理ステップの回数に変える
class LoopCycle {
public:
    struct Snapshot {
        LoopRates rates;
        std::int64_t pending = 0;
        std::int64_t carry = 0;
        std::uint64_t clock = 0;
    };

    LoopCycle() = default;
    explicit LoopCycle(LoopRates rates) : rates_(rates) {}

    LoopRates Rates() const { return rates_; }

    void SetRates(LoopRates rates) {
        rates_ = rates;
        carry_ = 0;
    }

    // 実行中に変わる
    int LogicRate() const { return rates_.logic; }

    void SetLogicRate(int rate) {
        rates_.logic = rate;
        carry_ = 0;
    }

    int DisplayRate() const { return rates_.display; }
    void SetDisplayRate(int rate) { rates_.display = rate; }

    // 1回の反復にかける目安のミリ秒
    double DisplayInterval() const {
        return rates_.display > 0 ? 1000.0 / rates_.display : 0.0;
    }

    VirtualClock &Clock() { return clock_; }
    const VirtualClock &Clock() const { return clock_; }

    void AddElapsedMicroseconds(std::int64_t microseconds) {
        if (microseconds <= 0) {
            return;
        }
        pending_ += microseconds;
        clock_.Advance(static_cast<std::uint64_t>(microseconds));
    }

    void AddElapsed(double milliseconds) {
        AddElapsedMicroseconds(static_cast<std::int64_t>(milliseconds * 1000.0 + 0.5));
    }

    // 1ステップ分溜まっていれば消費して真を返す
    // 論理レートは呼ぶたびに読み直す
    bool ConsumeStep() {
        if (rates_.logic <= 0) {
            return false;
        }
        const std::int64_t rate = rates_.logic;
        std::int64_t interval = kSecond / rate;
        std::int64_t carry = carry_ + kSecond % rate;
        if (carry >= rate) {
            carry -= rate;
            interval += 1;
        }
        if (pending_ < interval) {
            return false;
        }
        pending_ -= interval;
        carry_ = carry;
        return true;
    }

    // 溜まった分を捨てる
    void DropPending() { pending_ = 0; }

    std::int64_t PendingMicroseconds() const { return pending_; }

    Snapshot Save() const { return Snapshot{rates_, pending_, carry_, clock_.Microseconds()}; }

    void Restore(const Snapshot &snapshot) {
        rates_ = snapshot.rates;
        pending_ = snapshot.pending;
        carry_ = snapshot.carry;
        clock_.SetMicroseconds(snapshot.clock);
    }

private:
    static constexpr std::int64_t kSecond = 1000000;

    LoopRates rates_;
    std::int64_t pending_ = 0;
    std::int64_t carry_ = 0;
    VirtualClock clock_;
};

} // namespace TellerEngine::Base
