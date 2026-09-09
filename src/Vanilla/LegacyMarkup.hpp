#pragma once

#include <Base/MarkupDiagnostic.hpp>
#include <Base/Text.hpp>
#include <Base/TextParser.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace TellerEngine::Vanilla {

// 移植元の記法
// \R などの色、\E などの引数付き、^n の間、& / % の区切り
class LegacyMarkup : public Base::TextParser {
public:
    Base::Text Parse(std::string_view source) const override {
        Base::Text text;
        Base::TextStyle style;

        std::size_t position = 0;
        std::size_t runBegin = 0;

        const auto flush = [&](std::size_t end) {
            if (end > runBegin) {
                text.Append(source.substr(runBegin, end - runBegin), style);
            }
        };

        while (position < source.size()) {
            const char letter = source[position];

            if (letter == '\\' && position + 1 < source.size()) {
                flush(position);
                position = Escape(source, position + 1, text, style);
                runBegin = position;
                continue;
            }

            if (letter == '^' && position + 1 < source.size() && source[position + 1] != '0') {
                flush(position);
                text.Add(Base::TextEventKind::Pause, Digit(source[position + 1]) * 10);
                position += 2;
                runBegin = position;
                continue;
            }

            if (letter == '&') {
                flush(position);
                text.Add(Base::TextEventKind::Break);
                position += 1;
                runBegin = position;
                continue;
            }

            if (letter == '/') {
                flush(position);
                text.Add(Base::TextEventKind::Wait, HaltKind(source, position));
                position += 1;
                runBegin = position;
                continue;
            }

            if (letter == '%') {
                flush(position);
                const bool last = position + 1 < source.size() && source[position + 1] == '%';
                text.Add(Base::TextEventKind::Close, last ? 1 : 0);
                position += last ? 2 : 1;
                runBegin = position;
                continue;
            }

            position += 1;
        }

        flush(source.size());
        return text;
    }

    // 移植元の色
    static constexpr std::optional<Base::Color> ColorOf(char code) {
        switch (code) {
        case 'R':
            return Base::Color{255, 0, 0, 255};
        case 'G':
            return Base::Color{0, 255, 0, 255};
        case 'W':
            return Base::Color{255, 255, 255, 255};
        case 'Y':
            return Base::Color{255, 255, 0, 255};
        case 'X':
            return Base::Color{0, 0, 0, 255};
        case 'B':
            return Base::Color{0, 0, 255, 255};
        case 'O':
            return Base::Color{255, 168, 64, 255};
        case 'L':
            return Base::Color{142, 194, 253, 255};
        case 'P':
            return Base::Color{255, 0, 255, 255};
        case 'p':
            return Base::Color{255, 192, 212, 255};
        default:
            return std::nullopt;
        }
    }

    // \T が指す書き手
    static constexpr std::optional<std::uint8_t> TyperOf(char code) {
        switch (code) {
        case 'T':
            return 4;
        case 't':
            return 48;
        case '0':
            return 5;
        case 'S':
            return 10;
        case 'F':
            return 16;
        case 's':
            return 17;
        case 'P':
            return 18;
        case 'M':
            return 27;
        case 'U':
            return 37;
        case 'A':
            return 47;
        case 'a':
            return 60;
        case 'R':
            return 76;
        default:
            return std::nullopt;
        }
    }

    // 記法の誤りを検出する
    // 解く側は誤りを落とさずに通す
    static constexpr Base::MarkupDiagnostic Diagnose(std::string_view source) {
        std::size_t position = 0;
        while (position < source.size()) {
            const char letter = source[position];

            if (letter == '\\') {
                if (position + 1 >= source.size()) {
                    return {Base::MarkupProblem::MissingArgument, position};
                }
                const char code = source[position + 1];
                if (ColorOf(code) || code == 'C') {
                    position += 2;
                    continue;
                }
                if (code == 'E' || code == 'F' || code == 'M' || code == 'S' ||
                    code == 'T' || code == 'z' || code == '*' || code == '>') {
                    if (position + 2 >= source.size()) {
                        return {Base::MarkupProblem::MissingArgument, position};
                    }
                    position += 3;
                    continue;
                }
                return {Base::MarkupProblem::UnknownName, position + 1};
            }

            if (letter == '^') {
                if (position + 1 >= source.size()) {
                    return {Base::MarkupProblem::MissingArgument, position};
                }
                const char digit = source[position + 1];
                if (digit < '0' || digit > '9') {
                    return {Base::MarkupProblem::BadArgument, position + 1};
                }
                position += 2;
                continue;
            }

            position += 1;
        }
        return {};
    }

private:
    static std::int32_t Digit(char letter) {
        return letter >= '0' && letter <= '9' ? letter - '0' : 0;
    }

    // / の直後で待ち方が変わる
    static std::int32_t HaltKind(std::string_view source, std::size_t position) {
        if (position + 1 >= source.size()) {
            return 1;
        }
        const char next = source[position + 1];
        if (next == '%') {
            return 2;
        }
        if (next == '*') {
            return 6;
        }
        if (next == '^' && position + 2 < source.size() && source[position + 2] != '0') {
            return 4;
        }
        return 1;
    }

    static std::size_t Escape(std::string_view source, std::size_t position, Base::Text &text,
                              Base::TextStyle &style) {
        const char code = source[position];

        if (const std::optional<Base::Color> color = ColorOf(code)) {
            style.color = *color;
            return position + 1;
        }

        const auto argument = [&]() -> char {
            return position + 1 < source.size() ? source[position + 1] : '\0';
        };

        switch (code) {
        case 'C':
            text.Add(Base::TextEventKind::Choice);
            return position + 1;
        case 'E':
            style.emotion = static_cast<std::uint8_t>(Digit(argument()));
            return position + 2;
        case 'F':
            style.face = static_cast<std::uint8_t>(Digit(argument()));
            return position + 2;
        case 'M':
            text.Add(Base::TextEventKind::Flag, Digit(argument()));
            return position + 2;
        case 'S':
            if (argument() == '+') {
                style.sound = true;
            } else if (argument() == '-') {
                style.sound = false;
            } else if (argument() == 'p') {
                text.Add(Base::TextEventKind::Sound, 105);
            }
            return position + 2;
        case 'T':
            if (argument() == '-') {
                style.halfSize = true;
            } else if (argument() == '+') {
                style.halfSize = false;
            } else if (const std::optional<std::uint8_t> typer = TyperOf(argument())) {
                style.typer = *typer;
            }
            return position + 2;
        case 'z':
            text.Add(Base::TextEventKind::Symbol, Digit(argument()));
            return position + 2;
        case '*':
            text.Add(Base::TextEventKind::Icon, static_cast<std::int32_t>(
                                                    static_cast<unsigned char>(argument())));
            return position + 2;
        case '>':
            return position + 2;
        default:
            return position + 1;
        }
    }
};

// 実行時に組み立てる
inline Base::Text Parse(std::string_view source) { return LegacyMarkup{}.Parse(source); }

} // namespace TellerEngine::Vanilla
