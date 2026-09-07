#pragma once

#include <Base/Variables.hpp>

#include <string_view>

namespace TellerEngine::Base {

// どのインスタンスにも属さない状態
// 継承して自分の分を足す
class Globals {
public:
    Globals() = default;
    virtual ~Globals() = default;

    Globals(const Globals &) = default;
    Globals &operator=(const Globals &) = default;
    Globals(Globals &&) = default;
    Globals &operator=(Globals &&) = default;

    static constexpr auto Variables() { return MakeVariables(); }
};

// 名前でも原名でも探す
template <typename T> bool GlobalExists(const T &globals, std::string_view name) {
    return static_cast<bool>(FindVariable(globals, name));
}

} // namespace TellerEngine::Base
