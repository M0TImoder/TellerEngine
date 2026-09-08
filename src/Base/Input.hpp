#pragma once

#include <cstddef>
#include <cstdint>

namespace TellerEngine::Base {

// ゲームが読む論理的な入力
enum class Button {
    Left,
    Right,
    Up,
    Down,
    Confirm,
    Cancel,
    Menu,
};

inline constexpr std::size_t kButtonCount = 7;

class Input {
public:
    struct Snapshot {
        std::uint32_t current = 0;
        std::uint32_t previous = 0;
    };

    constexpr bool Held(Button button) const { return (current_ & Bit(button)) != 0; }

    constexpr bool Pressed(Button button) const {
        return (current_ & Bit(button)) != 0 && (previous_ & Bit(button)) == 0;
    }

    constexpr bool Released(Button button) const {
        return (current_ & Bit(button)) == 0 && (previous_ & Bit(button)) != 0;
    }

    // 論理ステップの頭で1度だけ呼ぶ
    constexpr void BeginFrame() { previous_ = current_; }

    constexpr void Set(Button button, bool down) {
        if (down) {
            current_ |= Bit(button);
        } else {
            current_ &= ~Bit(button);
        }
    }

    constexpr void Clear() { current_ = 0; }

    constexpr Snapshot Save() const { return Snapshot{current_, previous_}; }

    constexpr void Restore(const Snapshot &snapshot) {
        current_ = snapshot.current;
        previous_ = snapshot.previous;
    }

private:
    static constexpr std::uint32_t Bit(Button button) {
        return std::uint32_t{1} << static_cast<std::uint32_t>(button);
    }

    std::uint32_t current_ = 0;
    std::uint32_t previous_ = 0;
};

} // namespace TellerEngine::Base
