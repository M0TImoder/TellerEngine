#pragma once

#include <Base/Compat.hpp>
#include <Base/GameObject.hpp>

#include <cstdint>
#include <array>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace TellerEngine::Base {

struct Color {
    std::uint8_t red = 255;
    std::uint8_t green = 255;
    std::uint8_t blue = 255;
    std::uint8_t alpha = 255;

    friend constexpr bool operator==(const Color &, const Color &) = default;
};

// 非線形の空間で混ぜる
enum class BlendMode {
    Normal,
    None,
    Add,
    Subtract,
    Multiply,
};

enum class DrawKind {
    Sprite,
    Rectangle,
    RoundRectangle,
    Line,
    Circle,
    Ellipse,
    Triangle,
    Text,

    // 以降の描画先を切り替える
    Target,
};

inline constexpr std::uint32_t kDefaultCirclePrecision = 24;

// 読み込んだスプライトや背景を指す
enum class ImageId : std::uint32_t {
    None = 0,
};

// 画面に出す1つ分
struct DrawCommand {
    DrawKind kind = DrawKind::Sprite;

    // 積んだインスタンス
    InstanceId source = InstanceId::None;

    double x = 0.0;
    double y = 0.0;

    double width = 0.0;
    double height = 0.0;

    double secondX = 0.0;
    double secondY = 0.0;

    double thirdX = 0.0;
    double thirdY = 0.0;

    double radius = 0.0;

    double scaleX = 1.0;
    double scaleY = 1.0;
    double rotation = 0.0;

    Color color;

    // 矩形は右上・右下・左下、三角形は2点目と3点目、楕円は縁
    std::array<Color, 3> extra{};

    double alpha = 1.0;
    BlendMode blend = BlendMode::Normal;

    double lineWidth = 1.0;

    // 円と楕円をいくつに分けるか
    std::uint32_t segments = kDefaultCirclePrecision;

    ImageId image = ImageId::None;
    std::uint32_t frame = 0;

    // 切り出す範囲
    double partX = 0.0;
    double partY = 0.0;
    double partWidth = 0.0;
    double partHeight = 0.0;
    bool usePart = false;
    bool ignoreOrigin = false;

    std::uint32_t textOffset = 0;
    std::uint32_t textLength = 0;

    bool filled = true;

    friend constexpr bool operator==(const DrawCommand &, const DrawCommand &) = default;
};

// 1論理フレーム分の積み荷
class DrawList {
public:
    void Clear() {
        commands_.clear();
        text_.clear();
    }

    void Push(const DrawCommand &command) { commands_.push_back(command); }

    // 文字列を置いて位置を返す
    std::pair<std::uint32_t, std::uint32_t> AddText(std::string_view text) {
        const auto offset = static_cast<std::uint32_t>(text_.size());
        text_.append(text);
        return {offset, static_cast<std::uint32_t>(text.size())};
    }

    std::string_view TextOf(const DrawCommand &command) const {
        return std::string_view{text_}.substr(command.textOffset, command.textLength);
    }

    Span<const DrawCommand> Commands() const { return Span<const DrawCommand>{commands_}; }

    std::size_t Size() const { return commands_.size(); }
    bool Empty() const { return commands_.empty(); }

    // 環境や実装をまたいで突き合わせるための値
    std::uint64_t Hash() const {
        std::uint64_t value = 0xcbf29ce484222325ull;
        for (const DrawCommand &command : commands_) {
            Mix(value, static_cast<std::uint64_t>(command.kind));
            Mix(value, static_cast<std::uint64_t>(command.source));
            MixReal(value, command.x);
            MixReal(value, command.y);
            MixReal(value, command.width);
            MixReal(value, command.height);
            MixReal(value, command.secondX);
            MixReal(value, command.secondY);
            MixReal(value, command.thirdX);
            MixReal(value, command.thirdY);
            MixReal(value, command.radius);
            MixReal(value, command.scaleX);
            MixReal(value, command.scaleY);
            MixReal(value, command.rotation);
            Mix(value, command.color.red);
            Mix(value, command.color.green);
            Mix(value, command.color.blue);
            Mix(value, command.color.alpha);
            for (const Color &extra : command.extra) {
                Mix(value, extra.red);
                Mix(value, extra.green);
                Mix(value, extra.blue);
                Mix(value, extra.alpha);
            }
            MixReal(value, command.alpha);
            MixReal(value, command.lineWidth);
            Mix(value, command.segments);
            Mix(value, static_cast<std::uint64_t>(command.blend));
            Mix(value, static_cast<std::uint64_t>(command.image));
            Mix(value, command.frame);
            MixReal(value, command.partX);
            MixReal(value, command.partY);
            MixReal(value, command.partWidth);
            MixReal(value, command.partHeight);
            Mix(value, command.usePart ? 1u : 0u);
            Mix(value, command.ignoreOrigin ? 1u : 0u);
            Mix(value, command.filled ? 1u : 0u);
            for (const char character : TextOf(command)) {
                Mix(value, static_cast<std::uint8_t>(character));
            }
        }
        return value;
    }

private:
    static void Mix(std::uint64_t &value, std::uint64_t part) {
        for (int i = 0; i < 8; ++i) {
            value ^= (part >> (i * 8)) & 0xffull;
            value *= 0x100000001b3ull;
        }
    }

    static void MixReal(std::uint64_t &value, double part) {
        std::uint64_t bits = 0;
        static_assert(sizeof(bits) == sizeof(part));
        std::memcpy(&bits, &part, sizeof(bits));
        Mix(value, bits);
    }

    std::vector<DrawCommand> commands_;
    std::string text_;
};

} // namespace TellerEngine::Base
