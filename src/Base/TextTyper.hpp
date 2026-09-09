#pragma once

#include <Base/Compat.hpp>
#include <Base/Text.hpp>
#include <Base/Utf8.hpp>

#include <cstdint>
#include <vector>

namespace TellerEngine::Base {

struct TextTyperSettings {
    // 1文字にかけるステップ数
    double interval = 1.0;

    // 1回で出す文字数
    int burst = 1;
};

// 文字を1つずつ出していく
class TextTyper {
public:
    enum class State {
        Idle,
        Typing,

        // 入力を待っている
        Waiting,

        // 文の終わり
        Closed,

        // 出し切った
        Done,
    };

    void Start(const Text &text, const TextTyperSettings &settings = {}) {
        text_ = &text;
        settings_ = settings;
        revealed_ = 0;
        event_ = 0;
        pause_ = 0;
        progress_ = 0.0;
        waitKind_ = 0;
        closeKind_ = 0;
        spoke_ = false;
        fired_.clear();
        state_ = State::Typing;
    }

    void Step() {
        fired_.clear();
        spoke_ = false;
        if (text_ == nullptr || state_ != State::Typing) {
            return;
        }
        if (pause_ > 0) {
            pause_ -= 1;
            return;
        }

        progress_ += Speed();
        while (state_ == State::Typing && pause_ == 0 && progress_ >= settings_.interval) {
            progress_ -= settings_.interval;
            for (int i = 0; i < settings_.burst && state_ == State::Typing && pause_ == 0;
                 ++i) {
                Reveal();
            }
        }
    }

    // 待ちに当たるまで一気に出す
    void Skip() {
        if (text_ == nullptr || state_ != State::Typing) {
            return;
        }
        pause_ = 0;
        while (state_ == State::Typing) {
            Reveal();
            pause_ = 0;
        }
        progress_ = 0.0;
    }

    // 入力待ちを解く
    void Continue() {
        if (state_ == State::Waiting) {
            state_ = State::Typing;
        }
    }

    std::uint32_t Revealed() const { return revealed_; }
    State Current() const { return state_; }
    bool Waiting() const { return state_ == State::Waiting; }
    bool Closed() const { return state_ == State::Closed; }
    bool Done() const { return state_ == State::Done || state_ == State::Closed; }

    // 入力待ちの種類
    std::int32_t WaitKind() const { return waitKind_; }

    // 0なら次の文へ、1なら全部閉じる
    std::int32_t CloseKind() const { return closeKind_; }

    // このステップで起きた指示
    Span<const TextEvent> Fired() const { return Span<const TextEvent>{fired_}; }

    // このステップで打鍵音が鳴るか
    bool Spoke() const { return spoke_; }

private:
    static bool Blank(char32_t character) {
        return character == U' ' || character == U'　';
    }

    double Speed() const {
        const auto position = static_cast<std::uint32_t>(
            revealed_ < text_->Length() ? revealed_ : text_->Length());
        const double speed = text_->StyleAt(position).speed;
        return speed > 0.0 ? speed : 1.0;
    }

    // 打つのを止める指示に当たったらfalse
    bool TakeEvents() {
        const Span<const TextEvent> events = text_->Events();
        while (event_ < events.size() && events[event_].at <= revealed_) {
            const TextEvent event = events[event_];
            event_ += 1;
            if (event.at < revealed_) {
                continue;
            }
            switch (event.kind) {
            case TextEventKind::Break:
                break;
            case TextEventKind::Pause:
                if (event.value > 0) {
                    pause_ = event.value - 1;
                    return false;
                }
                break;
            case TextEventKind::Wait:
                waitKind_ = event.value;
                state_ = State::Waiting;
                return false;
            case TextEventKind::Close:
                closeKind_ = event.value;
                state_ = State::Closed;
                return false;
            default:
                fired_.push_back(event);
                break;
            }
        }
        return true;
    }

    void Reveal() {
        if (!TakeEvents()) {
            return;
        }
        if (revealed_ >= text_->Length()) {
            state_ = State::Done;
            return;
        }

        const Utf8Char letter = DecodeUtf8(text_->Characters(), revealed_);
        const TextStyle style = text_->StyleAt(revealed_);
        revealed_ += letter.size;
        if (style.sound && !Blank(letter.code)) {
            spoke_ = true;
        }
    }

    const Text *text_ = nullptr;
    TextTyperSettings settings_;
    std::vector<TextEvent> fired_;
    std::uint32_t revealed_ = 0;
    std::size_t event_ = 0;
    std::int32_t pause_ = 0;
    std::int32_t waitKind_ = 0;
    std::int32_t closeKind_ = 0;
    double progress_ = 0.0;
    bool spoke_ = false;
    State state_ = State::Idle;
};

} // namespace TellerEngine::Base
