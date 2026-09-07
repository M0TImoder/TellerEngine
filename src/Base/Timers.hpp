#pragma once

#include <Base/Rate.hpp>

#include <cstddef>
#include <tuple>

namespace TellerEngine::Base {

// 数え終わったときに呼ぶメソッドと残りを持つメンバの組
template <typename Class> struct TimerEntry {
    Duration Class::*counter = nullptr;
    void (Class::*action)() = nullptr;
};

template <typename Class>
constexpr auto Timer(Duration Class::*counter, void (Class::*action)()) {
    return TimerEntry<Class>{counter, action};
}

template <typename... Entries> struct TimerList {
    std::tuple<Entries...> entries;
    static constexpr std::size_t kCount = sizeof...(Entries);
};

template <typename... Entries> constexpr auto MakeTimers(Entries... entries) {
    return TimerList<Entries...>{std::tuple{entries...}};
}

template <typename... Inherited, typename... Own>
constexpr auto ExtendTimers(TimerList<Inherited...> inherited, TimerList<Own...> own) {
    return TimerList<Inherited..., Own...>{std::tuple_cat(inherited.entries, own.entries)};
}

template <typename T> constexpr std::size_t TimerCount() {
    return decltype(T::Timers())::kCount;
}

// 残りを1減らし、0になったらメソッドを呼んで止める
// 0より大きい間だけ動く
template <typename T> void TickTimers(T &object) {
    constexpr auto list = T::Timers();
    std::apply(
        [&](const auto &...entry) {
            (
                [&] {
                    Duration &counter = object.*(entry.counter);
                    if (counter.Value() <= 0.0) {
                        return;
                    }
                    counter = counter.Value() - 1.0;
                    if (counter.Value() <= 0.0) {
                        counter = -1.0;
                        (object.*(entry.action))();
                    }
                }(),
                ...);
        },
        list.entries);
}

} // namespace TellerEngine::Base
