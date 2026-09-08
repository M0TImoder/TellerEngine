#pragma once

#include <cmath>

namespace TellerEngine::Base {

inline constexpr double kPi = 3.14159265358979323846;

// 丁度半分のときは偶数側へ丸める
inline double Round(double value) {
    const double floored = std::floor(value);
    const double fraction = value - floored;
    if (fraction > 0.5) {
        return floored + 1.0;
    }
    if (fraction < 0.5) {
        return floored;
    }
    return std::fmod(floored, 2.0) == 0.0 ? floored : floored + 1.0;
}

inline double DegToRad(double degrees) { return degrees * kPi / 180.0; }

inline double RadToDeg(double radians) { return radians * 180.0 / kPi; }

// 向きは度で、y軸は下向き
inline double LengthDirX(double length, double direction) {
    return length * std::cos(DegToRad(direction));
}

inline double LengthDirY(double length, double direction) {
    return -length * std::sin(DegToRad(direction));
}

inline double PointDistance(double x1, double y1, double x2, double y2) {
    return std::hypot(x2 - x1, y2 - y1);
}

// 0以上360未満
inline double PointDirection(double x1, double y1, double x2, double y2) {
    const double degrees = RadToDeg(std::atan2(y1 - y2, x2 - x1));
    return degrees < 0.0 ? degrees + 360.0 : degrees;
}

} // namespace TellerEngine::Base
