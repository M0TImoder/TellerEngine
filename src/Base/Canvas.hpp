#pragma once

#include <Base/Draw.hpp>
#include <Base/GameObject.hpp>

#include <cstdint>
#include <string_view>

namespace TellerEngine::Base {

// 描画コマンドを積む口
class Canvas {
public:
    explicit Canvas(DrawList &list) : list_(list) {}

    DrawList &List() { return list_; }
    const DrawList &List() const { return list_; }

    void SetColor(Color color) { color_ = color; }
    Color CurrentColor() const { return color_; }

    void SetAlpha(double alpha) { alpha_ = alpha; }
    double CurrentAlpha() const { return alpha_; }

    void SetBlend(BlendMode blend) { blend_ = blend; }
    BlendMode CurrentBlend() const { return blend_; }

    void SetSource(InstanceId source) { source_ = source; }
    InstanceId CurrentSource() const { return source_; }

    void SetCirclePrecision(std::uint32_t segments) {
        segments_ = segments < 4 ? 4 : segments;
    }
    std::uint32_t CirclePrecision() const { return segments_; }

    void Reset() {
        color_ = Color{};
        alpha_ = 1.0;
        blend_ = BlendMode::Normal;
        source_ = InstanceId::None;
        segments_ = kDefaultCirclePrecision;
    }

    // 以降の描画先を差し替える
    void SetTarget(ImageId surface) {
        DrawCommand command = Begin(DrawKind::Target);
        command.image = surface;
        list_.Push(command);
    }

    void ResetTarget() { SetTarget(ImageId::None); }

    void Sprite(ImageId image, std::uint32_t frame, double x, double y) {
        DrawCommand command = Begin(DrawKind::Sprite);
        command.image = image;
        command.frame = frame;
        command.x = x;
        command.y = y;
        list_.Push(command);
    }

    void SpriteExt(ImageId image, std::uint32_t frame, double x, double y,
                   double scaleX, double scaleY, double rotation) {
        DrawCommand command = Begin(DrawKind::Sprite);
        command.image = image;
        command.frame = frame;
        command.x = x;
        command.y = y;
        command.scaleX = scaleX;
        command.scaleY = scaleY;
        command.rotation = rotation;
        list_.Push(command);
    }

    // 一部だけを切り出して出す
    void SpritePart(ImageId image, std::uint32_t frame, double left, double top,
                    double width, double height, double x, double y) {
        SpritePartExt(image, frame, left, top, width, height, x, y, 1.0, 1.0);
    }

    void SpritePartExt(ImageId image, std::uint32_t frame, double left, double top,
                       double width, double height, double x, double y, double scaleX,
                       double scaleY) {
        DrawCommand command = Begin(DrawKind::Sprite);
        command.image = image;
        command.frame = frame;
        command.x = x;
        command.y = y;
        command.scaleX = scaleX;
        command.scaleY = scaleY;
        command.partX = left;
        command.partY = top;
        command.partWidth = width;
        command.partHeight = height;
        command.usePart = true;
        command.ignoreOrigin = true;
        list_.Push(command);
    }

    // 指した大きさに伸ばす
    void SpriteStretched(ImageId image, std::uint32_t frame, double x, double y,
                         double width, double height) {
        DrawCommand command = Begin(DrawKind::Sprite);
        command.image = image;
        command.frame = frame;
        command.x = x;
        command.y = y;
        command.width = width;
        command.height = height;
        command.usePart = false;
        command.ignoreOrigin = true;
        list_.Push(command);
    }

    void Rectangle(double x, double y, double width, double height, bool filled = true) {
        DrawCommand command = Begin(DrawKind::Rectangle);
        command.x = x;
        command.y = y;
        command.width = width;
        command.height = height;
        command.filled = filled;
        list_.Push(command);
    }

    void RectangleCorners(double x1, double y1, double x2, double y2, bool filled = true) {
        Rectangle(x1, y1, x2 - x1, y2 - y1, filled);
    }

    // 4つの角の色が変わる
    void RectangleColor(double x1, double y1, double x2, double y2, Color topLeft,
                        Color topRight, Color bottomRight, Color bottomLeft,
                        bool filled = true) {
        DrawCommand command = Begin(DrawKind::Rectangle);
        command.x = x1;
        command.y = y1;
        command.width = x2 - x1;
        command.height = y2 - y1;
        command.filled = filled;
        command.color = topLeft;
        command.extra = {topRight, bottomRight, bottomLeft};
        list_.Push(command);
    }

    void Line(double x1, double y1, double x2, double y2, double lineWidth = 1.0) {
        DrawCommand command = Begin(DrawKind::Line);
        command.x = x1;
        command.y = y1;
        command.secondX = x2;
        command.secondY = y2;
        command.lineWidth = lineWidth;
        list_.Push(command);
    }

    void Circle(double x, double y, double radius, bool filled = true) {
        DrawCommand command = Begin(DrawKind::Circle);
        command.x = x;
        command.y = y;
        command.radius = radius;
        command.filled = filled;
        list_.Push(command);
    }

    // 中心から縁へ色が変わる
    void CircleColor(double x, double y, double radius, Color inner, Color outer,
                     bool filled = true) {
        DrawCommand command = Begin(DrawKind::Circle);
        command.x = x;
        command.y = y;
        command.radius = radius;
        command.filled = filled;
        command.color = inner;
        command.extra = {outer, outer, outer};
        list_.Push(command);
    }

    // 囲む矩形で決まる
    void Ellipse(double x1, double y1, double x2, double y2, bool filled = true) {
        DrawCommand command = Begin(DrawKind::Ellipse);
        command.x = x1;
        command.y = y1;
        command.secondX = x2;
        command.secondY = y2;
        command.filled = filled;
        list_.Push(command);
    }

    void EllipseColor(double x1, double y1, double x2, double y2, Color inner, Color outer,
                      bool filled = true) {
        DrawCommand command = Begin(DrawKind::Ellipse);
        command.x = x1;
        command.y = y1;
        command.secondX = x2;
        command.secondY = y2;
        command.filled = filled;
        command.color = inner;
        command.extra = {outer, outer, outer};
        list_.Push(command);
    }

    void RoundRectangle(double x1, double y1, double x2, double y2, bool filled = true) {
        DrawCommand command = Begin(DrawKind::RoundRectangle);
        command.x = x1;
        command.y = y1;
        command.width = x2 - x1;
        command.height = y2 - y1;
        command.filled = filled;
        list_.Push(command);
    }

    void Triangle(double x1, double y1, double x2, double y2, double x3, double y3,
                  bool filled = true) {
        DrawCommand command = Begin(DrawKind::Triangle);
        command.x = x1;
        command.y = y1;
        command.secondX = x2;
        command.secondY = y2;
        command.thirdX = x3;
        command.thirdY = y3;
        command.filled = filled;
        list_.Push(command);
    }

    // 頂点ごとに色が変わる
    void TriangleColor(double x1, double y1, double x2, double y2, double x3, double y3,
                       Color first, Color second, Color third, bool filled = true) {
        DrawCommand command = Begin(DrawKind::Triangle);
        command.x = x1;
        command.y = y1;
        command.secondX = x2;
        command.secondY = y2;
        command.thirdX = x3;
        command.thirdY = y3;
        command.filled = filled;
        command.color = first;
        command.extra = {second, third, third};
        list_.Push(command);
    }

    void Text(double x, double y, std::string_view text) {
        DrawCommand command = Begin(DrawKind::Text);
        command.x = x;
        command.y = y;
        const auto placed = list_.AddText(text);
        command.textOffset = placed.first;
        command.textLength = placed.second;
        list_.Push(command);
    }

private:
    DrawCommand Begin(DrawKind kind) const {
        DrawCommand command;
        command.kind = kind;
        command.source = source_;
        command.color = color_;
        command.extra = {color_, color_, color_};
        command.alpha = alpha_;
        command.blend = blend_;
        command.segments = segments_;
        return command;
    }

    DrawList &list_;
    Color color_;
    double alpha_ = 1.0;
    BlendMode blend_ = BlendMode::Normal;
    InstanceId source_ = InstanceId::None;
    std::uint32_t segments_ = kDefaultCirclePrecision;
};

} // namespace TellerEngine::Base
