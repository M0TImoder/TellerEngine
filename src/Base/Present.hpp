#pragma once

#include <Base/Draw.hpp>
#include <Base/GameObject.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace TellerEngine::Base {

// 表示のときに積み荷をどう扱うか
enum class DisplayMode {
    Native,
    Repeat,
    Interpolate,
};

// 論理フレームの積み荷を受け取り、表示のたびに出すものを決める
class Presenter {
public:
    DisplayMode Mode() const { return mode_; }
    void SetMode(DisplayMode mode) { mode_ = mode; }

    // 論理フレームが1つ終わるたびに渡す
    void Submit(const DrawList &list) {
        previous_ = current_;
        current_ = list;
        submitted_ += 1;
    }

    // betweenは前の積み荷から今の積み荷までのどこか
    const DrawList &Frame(double between) {
        if (mode_ != DisplayMode::Interpolate || submitted_ < 2) {
            return current_;
        }
        Blend(between < 0.0 ? 0.0 : (between > 1.0 ? 1.0 : between));
        return blended_;
    }

    const DrawList &Latest() const { return current_; }

    std::uint64_t Submitted() const { return submitted_; }

private:
    void Blend(double between) {
        blended_.Clear();
        Index();

        std::unordered_map<std::uint32_t, std::size_t> seen;
        for (const DrawCommand &command : current_.Commands()) {
            const auto source = static_cast<std::uint32_t>(command.source);
            const std::size_t ordinal = seen[source]++;

            DrawCommand output = command;
            if (const DrawCommand *before = Older(source, ordinal, command.kind)) {
                output.x = Mix(before->x, command.x, between);
                output.y = Mix(before->y, command.y, between);
                output.width = Mix(before->width, command.width, between);
                output.height = Mix(before->height, command.height, between);
                output.thirdX = Mix(before->thirdX, command.thirdX, between);
                output.thirdY = Mix(before->thirdY, command.thirdY, between);
                output.scaleX = Mix(before->scaleX, command.scaleX, between);
                output.scaleY = Mix(before->scaleY, command.scaleY, between);
                output.rotation = MixAngle(before->rotation, command.rotation, between);
                output.alpha = Mix(before->alpha, command.alpha, between);
                output.color = MixColor(before->color, command.color, between);
            }

            if (output.kind == DrawKind::Text) {
                const auto placed = blended_.AddText(current_.TextOf(command));
                output.textOffset = placed.first;
                output.textLength = placed.second;
            }
            blended_.Push(output);
        }
    }

    void Index() {
        older_.clear();
        std::unordered_map<std::uint32_t, std::size_t> seen;
        const auto commands = previous_.Commands();
        for (std::size_t i = 0; i < commands.size(); ++i) {
            const auto source = static_cast<std::uint32_t>(commands[i].source);
            older_[Key(source, seen[source]++)] = i;
        }
    }

    const DrawCommand *Older(std::uint32_t source, std::size_t ordinal, DrawKind kind) const {
        const auto found = older_.find(Key(source, ordinal));
        if (found == older_.end()) {
            return nullptr;
        }
        const DrawCommand &command = previous_.Commands()[found->second];
        return command.kind == kind ? &command : nullptr;
    }

    static std::uint64_t Key(std::uint32_t source, std::size_t ordinal) {
        return (static_cast<std::uint64_t>(source) << 32) | static_cast<std::uint32_t>(ordinal);
    }

    static double Mix(double from, double to, double between) {
        return from + (to - from) * between;
    }

    // 短い側を回る
    static double MixAngle(double from, double to, double between) {
        double difference = std::fmod(to - from, 360.0);
        if (difference > 180.0) {
            difference -= 360.0;
        } else if (difference < -180.0) {
            difference += 360.0;
        }
        return from + difference * between;
    }

    static Color MixColor(Color from, Color to, double between) {
        const auto part = [&](std::uint8_t a, std::uint8_t b) {
            return static_cast<std::uint8_t>(std::lround(Mix(a, b, between)));
        };
        return Color{part(from.red, to.red), part(from.green, to.green),
                     part(from.blue, to.blue), part(from.alpha, to.alpha)};
    }

    DisplayMode mode_ = DisplayMode::Native;
    DrawList previous_;
    DrawList current_;
    DrawList blended_;
    std::unordered_map<std::uint64_t, std::size_t> older_;
    std::uint64_t submitted_ = 0;
};

} // namespace TellerEngine::Base
