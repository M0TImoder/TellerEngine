#pragma once

#include <Base/Compat.hpp>
#include <Base/Draw.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace TellerEngine::Base {

// 文字に付く見た目
struct TextStyle {
    Color color{255, 255, 255, 255};

    // 顔と表情
    std::uint8_t face = 0;
    std::uint8_t emotion = 0;

    // フォント・速度・音をまとめた書き手
    std::uint8_t typer = 0;

    bool halfSize = false;
    bool sound = true;

    // 揺れと波の強さ
    double shake = 0.0;
    double wave = 0.0;

    // 打鍵の速さの倍率
    double speed = 1.0;

    friend constexpr bool operator==(const TextStyle &, const TextStyle &) = default;
};

// 文字の流れの途中に挟まる指示
enum class TextEventKind {
    Break,
    Wait,
    Close,
    Pause,
    Choice,
    Flag,
    Sound,
    Icon,
    Symbol,
};

struct TextEvent {
    TextEventKind kind = TextEventKind::Break;

    // charactersの何バイト目の直前か
    std::uint32_t at = 0;

    std::int32_t value = 0;

    friend constexpr bool operator==(const TextEvent &, const TextEvent &) = default;
};

// 同じ見た目が続く範囲
struct TextSpan {
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
    TextStyle style;

    friend constexpr bool operator==(const TextSpan &, const TextSpan &) = default;
};

// 文字の範囲に振る読み
struct TextRuby {
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
    std::string reading;

    friend bool operator==(const TextRuby &, const TextRuby &) = default;
};

// 記法を解いた結果
// どの記法から作っても同じ型になる
class Text {
public:
    void Clear() {
        characters_.clear();
        spans_.clear();
        events_.clear();
        rubies_.clear();
    }

    void Append(std::string_view text, const TextStyle &style) {
        if (text.empty()) {
            return;
        }
        const auto begin = static_cast<std::uint32_t>(characters_.size());
        characters_.append(text);
        const auto end = static_cast<std::uint32_t>(characters_.size());

        if (!spans_.empty() && spans_.back().style == style && spans_.back().end == begin) {
            spans_.back().end = end;
            return;
        }
        spans_.push_back(TextSpan{begin, end, style});
    }

    void Add(TextEventKind kind, std::int32_t value = 0) {
        events_.push_back(
            TextEvent{kind, static_cast<std::uint32_t>(characters_.size()), value});
    }

    void AddRuby(std::uint32_t begin, std::uint32_t end, std::string reading) {
        if (begin >= end || reading.empty()) {
            return;
        }
        rubies_.push_back(TextRuby{begin, end, std::move(reading)});
    }

    std::string_view Characters() const { return characters_; }
    Span<const TextSpan> Spans() const { return Span<const TextSpan>{spans_}; }
    Span<const TextEvent> Events() const { return Span<const TextEvent>{events_}; }
    Span<const TextRuby> Rubies() const { return Span<const TextRuby>{rubies_}; }

    bool Empty() const {
        return characters_.empty() && events_.empty() && rubies_.empty();
    }
    std::size_t Length() const { return characters_.size(); }

    // 指定した位置の見た目
    TextStyle StyleAt(std::uint32_t position) const {
        for (const TextSpan &span : spans_) {
            if (position >= span.begin && position < span.end) {
                return span.style;
            }
        }
        return spans_.empty() ? TextStyle{} : spans_.back().style;
    }

private:
    std::string characters_;
    std::vector<TextSpan> spans_;
    std::vector<TextEvent> events_;
    std::vector<TextRuby> rubies_;
};

} // namespace TellerEngine::Base
