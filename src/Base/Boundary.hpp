#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace TellerEngine::Base {

struct Position {
    double x = 0.0;
    double y = 0.0;
};

struct Segment {
    Position from;
    Position to;
};

// 枠の形
// 内側が負、外側が正、縁が0になる距離をひとつ返す
// 当たり判定も縁の形もこの1つから導く
class Boundary {
public:
    virtual ~Boundary() = default;

    virtual double Distance(Position point) const = 0;

    bool Contains(Position point) const { return Distance(point) <= 0.0; }

    // 外向きの法線
    Position Normal(Position point) const {
        constexpr double step = 0.01;
        const double dx = Distance({point.x + step, point.y}) -
                          Distance({point.x - step, point.y});
        const double dy = Distance({point.x, point.y + step}) -
                          Distance({point.x, point.y - step});
        const double length = std::hypot(dx, dy);
        if (length <= 0.0) {
            return Position{0.0, 0.0};
        }
        return Position{dx / length, dy / length};
    }

    // 外に出ていれば縁まで押し戻す
    Position Project(Position point) const {
        constexpr int kSteps = 8;
        for (int i = 0; i < kSteps; ++i) {
            const double distance = Distance(point);
            if (distance <= 0.0) {
                return point;
            }
            const Position normal = Normal(point);
            if (normal.x == 0.0 && normal.y == 0.0) {
                return point;
            }
            point.x -= normal.x * distance;
            point.y -= normal.y * distance;
        }
        return point;
    }

    // 縁を通る線分の集まり
    // 距離が0になる場所を格子の各辺で求めて繋ぐ
    std::vector<Segment> Outline(Position center, double halfWidth, double halfHeight,
                                 double cell = 2.0) const {
        std::vector<Segment> segments;
        if (cell <= 0.0 || halfWidth <= 0.0 || halfHeight <= 0.0) {
            return segments;
        }

        const double left = center.x - halfWidth;
        const double top = center.y - halfHeight;
        const auto columns = static_cast<int>(std::ceil(halfWidth * 2.0 / cell));
        const auto rows = static_cast<int>(std::ceil(halfHeight * 2.0 / cell));

        std::vector<double> upper(static_cast<std::size_t>(columns) + 1);
        std::vector<double> lower(static_cast<std::size_t>(columns) + 1);
        for (int column = 0; column <= columns; ++column) {
            upper[static_cast<std::size_t>(column)] = Distance({left + column * cell, top});
        }

        for (int row = 0; row < rows; ++row) {
            const double nextY = top + (row + 1) * cell;
            for (int column = 0; column <= columns; ++column) {
                lower[static_cast<std::size_t>(column)] =
                    Distance({left + column * cell, nextY});
            }

            for (int column = 0; column < columns; ++column) {
                const double x0 = left + column * cell;
                const double y0 = top + row * cell;
                const double x1 = x0 + cell;
                const double y1 = nextY;

                const double d00 = upper[static_cast<std::size_t>(column)];
                const double d10 = upper[static_cast<std::size_t>(column) + 1];
                const double d11 = lower[static_cast<std::size_t>(column) + 1];
                const double d01 = lower[static_cast<std::size_t>(column)];

                Position crossings[4];
                int found = 0;
                const auto edge = [&](double ax, double ay, double da, double bx, double by,
                                      double db) {
                    if ((da <= 0.0) == (db <= 0.0)) {
                        return;
                    }
                    const double span = db - da;
                    const double t = span == 0.0 ? 0.5 : -da / span;
                    crossings[found++] = Position{ax + (bx - ax) * t, ay + (by - ay) * t};
                };

                edge(x0, y0, d00, x1, y0, d10);
                edge(x1, y0, d10, x1, y1, d11);
                edge(x1, y1, d11, x0, y1, d01);
                edge(x0, y1, d01, x0, y0, d00);

                if (found == 2) {
                    segments.push_back({crossings[0], crossings[1]});
                } else if (found == 4) {
                    segments.push_back({crossings[0], crossings[1]});
                    segments.push_back({crossings[2], crossings[3]});
                }
            }
            upper.swap(lower);
        }
        return segments;
    }
};

// 壁に当たったときの進み方
enum class SlideMode {
    // 軸ごとに独立に判定し、通る軸だけ動かす
    Axis,
    // 壁の法線の成分を抜く
    Block,
    // 壁に沿って、速さを保ったまま進む
    Preserve,
};

// 枠の中に収めながら動かす
inline Position Slide(const Boundary &boundary, Position from, double deltaX, double deltaY,
                      SlideMode mode = SlideMode::Axis) {
    const Position target{from.x + deltaX, from.y + deltaY};
    if (boundary.Contains(target)) {
        return target;
    }

    if (mode == SlideMode::Axis) {
        // 通らなければ壁まで寄せる
        const auto step = [&boundary](Position start, double moveX, double moveY) {
            if (moveX == 0.0 && moveY == 0.0) {
                return start;
            }
            const Position wanted{start.x + moveX, start.y + moveY};
            if (boundary.Contains(wanted)) {
                return wanted;
            }
            if (!boundary.Contains(start)) {
                return start;
            }

            Position inside = start;
            Position outside = wanted;
            for (int i = 0; i < 20; ++i) {
                const Position middle{(inside.x + outside.x) * 0.5,
                                      (inside.y + outside.y) * 0.5};
                if (boundary.Contains(middle)) {
                    inside = middle;
                } else {
                    outside = middle;
                }
            }
            return inside;
        };

        Position position = boundary.Project(from);
        position = step(position, deltaX, 0.0);
        position = step(position, 0.0, deltaY);
        return position;
    }

    const double speed = std::hypot(deltaX, deltaY);
    if (speed <= 0.0) {
        return boundary.Project(from);
    }

    if (mode == SlideMode::Block) {
        const Position normal = boundary.Normal(target);
        if (normal.x == 0.0 && normal.y == 0.0) {
            return boundary.Project(target);
        }
        const double into = deltaX * normal.x + deltaY * normal.y;
        return boundary.Project(
            {from.x + deltaX - normal.x * into, from.y + deltaY - normal.y * into});
    }

    // 縁に沿わせるため小刻みに進める
    // 一度決めた向きを持ち越すので、法線が進行方向と並ぶ場所でも折り返さない
    constexpr int kSubSteps = 8;
    const double sub = speed / kSubSteps;

    Position position = boundary.Project(from);
    double dirX = deltaX / speed;
    double dirY = deltaY / speed;

    for (int step = 0; step < kSubSteps; ++step) {
        const Position wanted{position.x + dirX * sub, position.y + dirY * sub};
        if (boundary.Contains(wanted)) {
            position = wanted;
            continue;
        }

        const Position normal = boundary.Normal(wanted);
        if (normal.x == 0.0 && normal.y == 0.0) {
            break;
        }

        const double into = dirX * normal.x + dirY * normal.y;
        const double alongX = dirX - normal.x * into;
        const double alongY = dirY - normal.y * into;
        const double alongLength = std::hypot(alongX, alongY);

        // 壁を真正面から押している
        if (alongLength < 0.05) {
            break;
        }

        dirX = alongX / alongLength;
        dirY = alongY / alongLength;
        position = boundary.Project({position.x + dirX * sub, position.y + dirY * sub});
    }

    return position;
}

// 半径radiusの丸が収まる範囲へ狭める
class Inset : public Boundary {
public:
    Inset(const Boundary &inner, double radius) : inner_(&inner), radius_(radius) {}

    double Distance(Position point) const override {
        return inner_->Distance(point) + radius_;
    }

private:
    const Boundary *inner_;
    double radius_ = 0.0;
};

class RectangleBoundary : public Boundary {
public:
    RectangleBoundary(Position center, double halfWidth, double halfHeight)
        : center_(center), halfWidth_(halfWidth), halfHeight_(halfHeight) {}

    double Distance(Position point) const override {
        const double dx = std::abs(point.x - center_.x) - halfWidth_;
        const double dy = std::abs(point.y - center_.y) - halfHeight_;
        const double outside = std::hypot(std::max(dx, 0.0), std::max(dy, 0.0));
        return outside + std::min(std::max(dx, dy), 0.0);
    }

    Position Center() const { return center_; }
    void SetCenter(Position center) { center_ = center; }

    void Resize(double halfWidth, double halfHeight) {
        halfWidth_ = halfWidth;
        halfHeight_ = halfHeight;
    }

private:
    Position center_;
    double halfWidth_ = 0.0;
    double halfHeight_ = 0.0;
};

class CircleBoundary : public Boundary {
public:
    CircleBoundary(Position center, double radius) : center_(center), radius_(radius) {}

    double Distance(Position point) const override {
        return std::hypot(point.x - center_.x, point.y - center_.y) - radius_;
    }

    Position Center() const { return center_; }
    void SetCenter(Position center) { center_ = center; }

    double Radius() const { return radius_; }
    void SetRadius(double radius) { radius_ = radius; }

private:
    Position center_;
    double radius_ = 0.0;
};

// 上下の縁が波打つ枠
class WaveBoundary : public Boundary {
public:
    WaveBoundary(Position center, double halfWidth, double halfHeight, double amplitude,
                 double wavelength, double phase = 0.0)
        : center_(center), halfWidth_(halfWidth), halfHeight_(halfHeight),
          amplitude_(amplitude), wavelength_(wavelength), phase_(phase) {}

    double Distance(Position point) const override {
        const double offset = Offset(point.x);
        const double left = center_.x - halfWidth_ - point.x;
        const double right = point.x - (center_.x + halfWidth_);
        const double top = (center_.y - halfHeight_ + offset) - point.y;
        const double bottom = point.y - (center_.y + halfHeight_ + offset);
        return std::max(std::max(left, right), std::max(top, bottom));
    }

    void SetPhase(double phase) { phase_ = phase; }
    double Phase() const { return phase_; }

private:
    double Offset(double x) const {
        if (wavelength_ == 0.0) {
            return 0.0;
        }
        constexpr double kPi = 3.14159265358979323846;
        return amplitude_ * std::sin(2.0 * kPi * (x - center_.x) / wavelength_ + phase_);
    }

    Position center_;
    double halfWidth_ = 0.0;
    double halfHeight_ = 0.0;
    double amplitude_ = 0.0;
    double wavelength_ = 0.0;
    double phase_ = 0.0;
};

// 好きな式で枠を決める
class FunctionBoundary : public Boundary {
public:
    explicit FunctionBoundary(std::function<double(Position)> distance)
        : distance_(std::move(distance)) {}

    double Distance(Position point) const override { return distance_(point); }

private:
    std::function<double(Position)> distance_;
};

} // namespace TellerEngine::Base
