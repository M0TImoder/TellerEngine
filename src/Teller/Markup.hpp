#pragma once

#include <Base/MarkupDiagnostic.hpp>
#include <Base/Text.hpp>
#include <Base/TextParser.hpp>

#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace TellerEngine::Teller {

// {名前:引数|範囲} の記法
// {{ が { そのものになる
// } は構文の外ならそのまま文字として通る
class Markup : public Base::TextParser {
public:
    Base::Text Parse(std::string_view source) const override {
        Base::Text text;
        Scan(source, Base::TextStyle{}, text);
        return text;
    }

    // 名前で引ける色
    static constexpr std::optional<Base::Color> ColorOf(std::string_view name) {
        if (name == "white") {
            return Base::Color{255, 255, 255, 255};
        }
        if (name == "black") {
            return Base::Color{0, 0, 0, 255};
        }
        if (name == "red") {
            return Base::Color{255, 0, 0, 255};
        }
        if (name == "green") {
            return Base::Color{0, 255, 0, 255};
        }
        if (name == "blue") {
            return Base::Color{0, 0, 255, 255};
        }
        if (name == "yellow") {
            return Base::Color{255, 255, 0, 255};
        }
        if (name == "orange") {
            return Base::Color{255, 168, 64, 255};
        }
        if (name == "lightblue") {
            return Base::Color{142, 194, 253, 255};
        }
        if (name == "magenta") {
            return Base::Color{255, 0, 255, 255};
        }
        if (name == "pink") {
            return Base::Color{255, 192, 212, 255};
        }
        return std::nullopt;
    }

    // 名前に無ければ #rrggbb
    static std::optional<Base::Color> ParseColor(std::string_view value) {
        if (const std::optional<Base::Color> named = ColorOf(value)) {
            return named;
        }
        if (value.size() != 7 || value.front() != '#') {
            return std::nullopt;
        }
        Base::Color color;
        std::uint8_t *parts[3] = {&color.red, &color.green, &color.blue};
        for (int i = 0; i < 3; ++i) {
            unsigned int part = 0;
            const char *begin = value.data() + 1 + i * 2;
            const auto result = std::from_chars(begin, begin + 2, part, 16);
            if (result.ec != std::errc{} || result.ptr != begin + 2) {
                return std::nullopt;
            }
            *parts[i] = static_cast<std::uint8_t>(part);
        }
        return color;
    }

    // 記法の誤りを検出する
    // 解く側は誤りを落とさずに通す
    static constexpr Base::MarkupDiagnostic Diagnose(std::string_view source) {
        std::size_t position = 0;
        while (position < source.size()) {
            if (source[position] != '{') {
                position += 1;
                continue;
            }
            if (position + 1 < source.size() && source[position + 1] == '{') {
                position += 2;
                continue;
            }

            const std::size_t close = FindClose(source, position);
            if (close == std::string_view::npos) {
                return {Base::MarkupProblem::Unclosed, position};
            }

            const Base::MarkupDiagnostic found =
                DiagnoseBody(source.substr(position + 1, close - position - 1), position + 1);
            if (!found) {
                return found;
            }
            position = close + 1;
        }
        return {};
    }

private:
    static constexpr bool Hex(char letter) {
        return (letter >= '0' && letter <= '9') || (letter >= 'a' && letter <= 'f') ||
               (letter >= 'A' && letter <= 'F');
    }

    static constexpr bool ValidColor(std::string_view value) {
        if (ColorOf(value)) {
            return true;
        }
        if (value.size() != 7 || value.front() != '#') {
            return false;
        }
        for (std::size_t i = 1; i < value.size(); ++i) {
            if (!Hex(value[i])) {
                return false;
            }
        }
        return true;
    }

    static constexpr bool ValidNumber(std::string_view value) {
        if (value.empty()) {
            return false;
        }
        std::size_t position = value.front() == '-' ? 1 : 0;
        if (position >= value.size()) {
            return false;
        }
        bool dot = false;
        bool digit = false;
        for (; position < value.size(); ++position) {
            if (value[position] == '.') {
                if (dot) {
                    return false;
                }
                dot = true;
                continue;
            }
            if (value[position] < '0' || value[position] > '9') {
                return false;
            }
            digit = true;
        }
        return digit;
    }

    static constexpr bool ValidWhole(std::string_view value) {
        if (value.empty()) {
            return false;
        }
        const std::size_t begin = value.front() == '-' ? 1 : 0;
        if (begin >= value.size()) {
            return false;
        }
        for (std::size_t i = begin; i < value.size(); ++i) {
            if (value[i] < '0' || value[i] > '9') {
                return false;
            }
        }
        return true;
    }

    static constexpr bool Ranged(std::string_view name) {
        return name == "color" || name == "shake" || name == "wave" || name == "speed" ||
               name == "ruby" || name == "small" || name == "quiet" || name == "face" ||
               name == "emotion" || name == "typer";
    }

    static constexpr bool Plain(std::string_view name) {
        return name == "br" || name == "end" || name == "close" || name == "wait" ||
               name == "choice" || name == "flag" || name == "sound";
    }

    static constexpr Base::MarkupDiagnostic DiagnoseBody(std::string_view body,
                                                         std::size_t at) {
        const std::size_t bar = SplitBar(body);
        const bool hasRange = bar != std::string_view::npos;
        const std::string_view head = body.substr(0, hasRange ? bar : body.size());
        const std::string_view range = hasRange ? body.substr(bar + 1) : std::string_view{};

        const std::size_t colon = head.find(':');
        const std::string_view name = head.substr(0, colon);
        const std::string_view argument =
            colon == std::string_view::npos ? std::string_view{} : head.substr(colon + 1);

        if (!Ranged(name) && !Plain(name)) {
            return {Base::MarkupProblem::UnknownName, at};
        }
        if (Ranged(name) && !hasRange) {
            return {Base::MarkupProblem::MissingRange, at};
        }

        if (name == "color" || name == "ruby") {
            if (argument.empty()) {
                return {Base::MarkupProblem::MissingArgument, at};
            }
        }
        if (name == "color" && !ValidColor(argument)) {
            return {Base::MarkupProblem::BadArgument, at};
        }
        if ((name == "shake" || name == "wave" || name == "speed" || name == "wait") &&
            !argument.empty() && !ValidNumber(argument)) {
            return {Base::MarkupProblem::BadArgument, at};
        }
        if ((name == "face" || name == "emotion" || name == "typer" || name == "flag" ||
             name == "sound" || name == "end") &&
            !argument.empty() && !ValidWhole(argument)) {
            return {Base::MarkupProblem::BadArgument, at};
        }
        if (name == "close" && !argument.empty() && argument != "all") {
            return {Base::MarkupProblem::BadArgument, at};
        }

        if (!hasRange) {
            return {};
        }
        return Diagnose(range);
    }

    static double Number(std::string_view value, double fallback) {
        if (value.empty()) {
            return fallback;
        }
        try {
            return std::stod(std::string{value});
        } catch (...) {
            return fallback;
        }
    }

    static std::int32_t Whole(std::string_view value) {
        int result = 0;
        const auto parsed =
            std::from_chars(value.data(), value.data() + value.size(), result);
        return parsed.ec == std::errc{} ? result : 0;
    }

    // 対応する } を探す
    static constexpr std::size_t FindClose(std::string_view source, std::size_t open) {
        int depth = 0;
        for (std::size_t i = open; i < source.size(); ++i) {
            if (source[i] == '{') {
                if (i + 1 < source.size() && source[i + 1] == '{') {
                    i += 1;
                    continue;
                }
                depth += 1;
            } else if (source[i] == '}') {
                depth -= 1;
                if (depth == 0) {
                    return i;
                }
            }
        }
        return std::string_view::npos;
    }

    static void Scan(std::string_view source, Base::TextStyle style, Base::Text &text) {
        std::string run;

        const auto flush = [&]() {
            if (!run.empty()) {
                text.Append(run, style);
                run.clear();
            }
        };

        std::size_t position = 0;
        while (position < source.size()) {
            const char letter = source[position];

            if (letter == '{' && position + 1 < source.size() && source[position + 1] == '{') {
                run.push_back('{');
                position += 2;
                continue;
            }
            if (letter != '{') {
                run.push_back(letter);
                position += 1;
                continue;
            }

            const std::size_t close = FindClose(source, position);
            if (close == std::string_view::npos) {
                run.push_back(letter);
                position += 1;
                continue;
            }

            flush();
            Apply(source.substr(position + 1, close - position - 1), style, text);
            position = close + 1;
        }

        flush();
    }

    static void Apply(std::string_view body, Base::TextStyle style, Base::Text &text) {
        const std::size_t bar = SplitBar(body);
        const std::string_view head = body.substr(0, bar == std::string_view::npos ? body.size()
                                                                                  : bar);
        const bool hasRange = bar != std::string_view::npos;
        const std::string_view range = hasRange ? body.substr(bar + 1) : std::string_view{};

        const std::size_t colon = head.find(':');
        const std::string_view name = head.substr(0, colon);
        const std::string_view argument =
            colon == std::string_view::npos ? std::string_view{} : head.substr(colon + 1);

        if (name == "br") {
            text.Add(Base::TextEventKind::Break);
            return;
        }
        if (name == "end") {
            text.Add(Base::TextEventKind::Wait, argument.empty() ? 1 : Whole(argument));
            return;
        }
        if (name == "close") {
            text.Add(Base::TextEventKind::Close, argument == "all" ? 1 : 0);
            return;
        }
        if (name == "wait") {
            text.Add(Base::TextEventKind::Pause,
                     static_cast<std::int32_t>(Number(argument, 0.0) * 30.0));
            return;
        }
        if (name == "choice") {
            text.Add(Base::TextEventKind::Choice);
            return;
        }
        if (name == "flag") {
            text.Add(Base::TextEventKind::Flag, Whole(argument));
            return;
        }
        if (name == "sound") {
            text.Add(Base::TextEventKind::Sound, Whole(argument));
            return;
        }

        if (name == "color") {
            if (const std::optional<Base::Color> color = ParseColor(argument)) {
                style.color = *color;
            }
        } else if (name == "shake") {
            style.shake = Number(argument, 1.0);
        } else if (name == "wave") {
            style.wave = Number(argument, 1.0);
        } else if (name == "speed") {
            style.speed = Number(argument, 1.0);
        } else if (name == "face") {
            style.face = static_cast<std::uint8_t>(Whole(argument));
        } else if (name == "emotion") {
            style.emotion = static_cast<std::uint8_t>(Whole(argument));
        } else if (name == "typer") {
            style.typer = static_cast<std::uint8_t>(Whole(argument));
        } else if (name == "small") {
            style.halfSize = true;
        } else if (name == "quiet") {
            style.sound = false;
        } else if (name == "ruby") {
            const auto begin = static_cast<std::uint32_t>(text.Length());
            Scan(range, style, text);
            text.AddRuby(begin, static_cast<std::uint32_t>(text.Length()),
                         std::string{argument});
            return;
        }

        if (hasRange) {
            Scan(range, style, text);
        }
    }

    // 入れ子の外側にある | を探す
    static constexpr std::size_t SplitBar(std::string_view body) {
        int depth = 0;
        for (std::size_t i = 0; i < body.size(); ++i) {
            if (body[i] == '{') {
                if (i + 1 < body.size() && body[i + 1] == '{') {
                    i += 1;
                    continue;
                }
                depth += 1;
            } else if (body[i] == '}') {
                depth -= 1;
            } else if (body[i] == '|' && depth == 0) {
                return i;
            }
        }
        return std::string_view::npos;
    }
};

// 実行時に組み立てる
inline Base::Text Parse(std::string_view source) { return Markup{}.Parse(source); }

} // namespace TellerEngine::Teller
