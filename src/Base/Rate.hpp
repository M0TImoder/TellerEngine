#pragma once

// 論理ステップを単位に持つ量

#include <cstddef>

namespace TellerEngine::Base {

// StepExponentは論理ステップの次数
// 継続時間が+1、速度が-1、加速度が-2
template <int StepExponent> class RateQuantity {
public:
    static constexpr int kStepExponent = StepExponent;

    constexpr RateQuantity() = default;
    constexpr RateQuantity(double value) : value_(value) {}

    constexpr operator double() const { return value_; }
    constexpr double Value() const { return value_; }

    // 論理レートをfactor倍したときの値
    constexpr RateQuantity Rescaled(double factor) const {
        double scale = 1.0;
        for (int i = 0; i < StepExponent; ++i) {
            scale *= factor;
        }
        for (int i = 0; i > StepExponent; --i) {
            scale /= factor;
        }
        return RateQuantity{value_ * scale};
    }

private:
    double value_ = 0.0;
};

// ステップ数
using Duration = RateQuantity<1>;

// 1ステップあたりの移動量
using Velocity = RateQuantity<-1>;

// 1ステップの2乗あたりの移動量
using Acceleration = RateQuantity<-2>;

// 次元を持たない量は素のdouble

} // namespace TellerEngine::Base
