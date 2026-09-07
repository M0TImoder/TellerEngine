#pragma once

// インスタンスの状態を名前で引けるようにする表

#include <Base/Rate.hpp>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace TellerEngine::Base {

// 型ごとに1つだけ存在する番地を型の識別子に使う
template <typename T> struct TypeSentinel {
    static constexpr char kValue = 0;
};

using TypeId = const void *;

template <typename T> constexpr TypeId TypeIdOf() { return &TypeSentinel<T>::kValue; }

// 値の区分
enum class VariableKind {
    Boolean,
    Integer,
    Number,
    Text,
    Duration,
    Velocity,
    Acceleration,
    Enumeration,
    Other,
};

template <typename T> constexpr VariableKind KindOf() {
    using Bare = std::remove_cv_t<T>;
    if constexpr (std::is_same_v<Bare, bool>) {
        return VariableKind::Boolean;
    } else if constexpr (std::is_same_v<Bare, Base::Duration>) {
        return VariableKind::Duration;
    } else if constexpr (std::is_same_v<Bare, Base::Velocity>) {
        return VariableKind::Velocity;
    } else if constexpr (std::is_same_v<Bare, Base::Acceleration>) {
        return VariableKind::Acceleration;
    } else if constexpr (std::is_same_v<Bare, std::string>) {
        return VariableKind::Text;
    } else if constexpr (std::is_enum_v<Bare>) {
        return VariableKind::Enumeration;
    } else if constexpr (std::is_integral_v<Bare>) {
        return VariableKind::Integer;
    } else if constexpr (std::is_floating_point_v<Bare>) {
        return VariableKind::Number;
    } else {
        return VariableKind::Other;
    }
}

// インスタンスを伴わない情報
struct VariableDescriptor {
    std::string_view name;
    std::string_view original;
    VariableKind kind = VariableKind::Other;
    TypeId type = nullptr;
    std::size_t size = 0;
};

// 見つからなかった参照はdataがnullptrになる
template <typename Pointer> struct BasicVariableRef {
    VariableDescriptor descriptor;
    Pointer data = nullptr;

    constexpr explicit operator bool() const { return data != nullptr; }

    template <typename T> constexpr auto As() const {
        using Result = std::conditional_t<std::is_const_v<std::remove_pointer_t<Pointer>>,
                                          const T *, T *>;
        if (data == nullptr || descriptor.type != TypeIdOf<T>()) {
            return static_cast<Result>(nullptr);
        }
        return static_cast<Result>(data);
    }
};

using VariableRef = BasicVariableRef<void *>;
using ConstVariableRef = BasicVariableRef<const void *>;

// 表に載る1つ分
template <typename Class, typename Member> struct VariableEntry {
    std::string_view name;
    std::string_view original;
    Member Class::*pointer = nullptr;

    constexpr VariableDescriptor Describe() const {
        return VariableDescriptor{name, original.empty() ? name : original, KindOf<Member>(),
                                  TypeIdOf<Member>(), sizeof(Member)};
    }
};

// originalは本家での綴り
// 省いた場合はnameと同じものが入る
template <typename Class, typename Member>
constexpr auto Var(Member Class::*pointer, std::string_view name,
                   std::string_view original = {}) {
    return VariableEntry<Class, Member>{name, original, pointer};
}

template <typename... Entries> struct VariableList {
    std::tuple<Entries...> entries;
    static constexpr std::size_t kCount = sizeof...(Entries);
};

template <typename... Entries> constexpr auto MakeVariables(Entries... entries) {
    return VariableList<Entries...>{std::tuple{entries...}};
}

// 基底の表に自前のエントリを継ぎ足す
template <typename... Inherited, typename... Own>
constexpr auto Extend(VariableList<Inherited...> inherited, VariableList<Own...> own) {
    return VariableList<Inherited..., Own...>{
        std::tuple_cat(inherited.entries, own.entries)};
}

template <typename T> constexpr std::size_t VariableCount() {
    return decltype(T::Variables())::kCount;
}

// 全エントリにfn(descriptor, ref)を渡す
template <typename T, typename Fn> constexpr void ForEachVariable(T &object, Fn &&fn) {
    constexpr auto list = T::Variables();
    std::apply(
        [&](const auto &...entry) {
            (fn(entry.Describe(),
                VariableRef{entry.Describe(),
                            static_cast<void *>(&(object.*(entry.pointer)))}),
             ...);
        },
        list.entries);
}

template <typename T, typename Fn> constexpr void ForEachVariable(const T &object, Fn &&fn) {
    constexpr auto list = T::Variables();
    std::apply(
        [&](const auto &...entry) {
            (fn(entry.Describe(),
                ConstVariableRef{entry.Describe(),
                                 static_cast<const void *>(&(object.*(entry.pointer)))}),
             ...);
        },
        list.entries);
}

// 名前でも原名でも引ける
// 見つからない場合は空の参照を返す
template <typename T> constexpr VariableRef FindVariable(T &object, std::string_view name) {
    VariableRef found;
    ForEachVariable(object, [&](const VariableDescriptor &descriptor, VariableRef ref) {
        if (!found && (descriptor.name == name || descriptor.original == name)) {
            found = ref;
        }
    });
    return found;
}

template <typename T>
constexpr ConstVariableRef FindVariable(const T &object, std::string_view name) {
    ConstVariableRef found;
    ForEachVariable(object, [&](const VariableDescriptor &descriptor, ConstVariableRef ref) {
        if (!found && (descriptor.name == name || descriptor.original == name)) {
            found = ref;
        }
    });
    return found;
}

// インスタンスを持たずに一覧だけ取る
template <typename T> constexpr auto DescribeVariables() {
    constexpr auto list = T::Variables();
    return std::apply(
        [](const auto &...entry) {
            return std::array<VariableDescriptor, sizeof...(entry)>{entry.Describe()...};
        },
        list.entries);
}

// 登録済みメンバの大きさの合計
template <typename T> constexpr std::size_t RegisteredBytes() {
    std::size_t total = 0;
    for (const VariableDescriptor &descriptor : DescribeVariables<T>()) {
        total += descriptor.size;
    }
    return total;
}

} // namespace TellerEngine::Base
