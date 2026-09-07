#pragma once

#include <Base/GameObject.hpp>
#include <Base/VariableCheck.hpp>
#include <Base/Variables.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace TellerEngine::Base {

// インスタンスの所有と、IDから実体への対応を持つ
class Instances {
public:
    using CoverageHandler = void (*)(TypeId type, const VariableCoverage &coverage);

    template <typename T, typename... Args> InstanceId Create(Args &&...args) {
        static_assert(std::is_base_of_v<GameObject, T>);

        const InstanceId id = static_cast<InstanceId>(nextId_);
        nextId_ += 1;

        static_assert(std::is_copy_constructible_v<T>);

        Slot slot;
        slot.id = id;
        slot.type = TypeIdOf<T>();
        slot.clone = &CloneOf<T>;
        slot.object = std::make_unique<T>(std::forward<Args>(args)...);
        slot.alive = true;
        slot.object->id = id;

        T &object = static_cast<T &>(*slot.object);

        byId_.emplace(static_cast<std::uint32_t>(id), slots_.size());
        byType_[slot.type].push_back(id);
        slots_.push_back(std::move(slot));

        ReportCoverage<T>(object);
        (void)object;
        return id;
    }

    // 直ちに存在しなくなるが、領域はCollectまで残る
    void Destroy(InstanceId id) {
        Slot *slot = FindSlot(id);
        if (slot == nullptr || !slot->alive) {
            return;
        }
        slot->alive = false;
    }

    bool Exists(InstanceId id) const {
        const Slot *slot = FindSlot(id);
        return slot != nullptr && slot->alive && slot->object->active;
    }

    // 有効でなくなる
    // IDを指した直接の参照だけは残る
    void Deactivate(InstanceId id) {
        if (GameObject *object = Find(id)) {
            object->active = false;
        }
    }

    void Activate(InstanceId id) {
        if (GameObject *object = Find(id)) {
            object->active = true;
        }
    }

    void DeactivateAll() {
        for (Slot &slot : slots_) {
            if (slot.alive) {
                slot.object->active = false;
            }
        }
    }

    void DeactivateAllExcept(InstanceId id) {
        DeactivateAll();
        Activate(id);
    }

    // Tとその派生だけ
    template <typename T> void DeactivateAll() {
        static_assert(std::is_base_of_v<GameObject, T>);
        for (Slot &slot : slots_) {
            if (slot.alive && dynamic_cast<T *>(slot.object.get()) != nullptr) {
                slot.object->active = false;
            }
        }
    }

    void ActivateAll() {
        for (Slot &slot : slots_) {
            if (slot.alive) {
                slot.object->active = true;
            }
        }
    }

    template <typename T> void ActivateAll() {
        static_assert(std::is_base_of_v<GameObject, T>);
        for (Slot &slot : slots_) {
            if (slot.alive && dynamic_cast<T *>(slot.object.get()) != nullptr) {
                slot.object->active = true;
            }
        }
    }

    GameObject *Find(InstanceId id) {
        Slot *slot = FindSlot(id);
        return slot != nullptr && slot->alive ? slot->object.get() : nullptr;
    }

    const GameObject *Find(InstanceId id) const {
        const Slot *slot = FindSlot(id);
        return slot != nullptr && slot->alive ? slot->object.get() : nullptr;
    }

    // 生成したときの型と一致しなければnullptrになる
    template <typename T> T *Find(InstanceId id) {
        Slot *slot = FindSlot(id);
        if (slot == nullptr || !slot->alive || slot->type != TypeIdOf<T>()) {
            return nullptr;
        }
        return static_cast<T *>(slot->object.get());
    }

    // Tとその派生の中で最初の有効なインスタンス
    template <typename T> T *First() {
        static_assert(std::is_base_of_v<GameObject, T>);
        const std::size_t count = slots_.size();
        for (std::size_t i = 0; i < count; ++i) {
            Slot &slot = slots_[i];
            if (!slot.alive || !slot.object->active) {
                continue;
            }
            if (T *typed = dynamic_cast<T *>(slot.object.get())) {
                return typed;
            }
        }
        return nullptr;
    }

    // Tとその派生を生成順にfn(T&)へ渡す
    template <typename T, typename Fn> void With(Fn &&fn) {
        static_assert(std::is_base_of_v<GameObject, T>);
        const std::size_t count = slots_.size();
        for (std::size_t i = 0; i < count; ++i) {
            Slot &slot = slots_[i];
            if (!slot.alive || !slot.object->active) {
                continue;
            }
            if (T *typed = dynamic_cast<T *>(slot.object.get())) {
                fn(*typed);
            }
        }
    }

    // 生成したときの型が丁度Tのものだけを生成順に渡す
    template <typename T, typename Fn> void WithExact(Fn &&fn) {
        const TypeId type = TypeIdOf<T>();
        const auto found = byType_.find(type);
        if (found == byType_.end()) {
            return;
        }
        const std::size_t count = found->second.size();
        for (std::size_t i = 0; i < count; ++i) {
            const InstanceId id = byType_.find(type)->second[i];
            Slot *slot = FindSlot(id);
            if (slot != nullptr && slot->alive && slot->object->active) {
                fn(static_cast<T &>(*slot->object));
            }
        }
    }

    // 有効なら1回、そうでなければ0回fn(GameObject&)を呼ぶ
    template <typename Fn> void WithId(InstanceId id, Fn &&fn) {
        if (!Exists(id)) {
            return;
        }
        fn(*Find(id));
    }

    // 型が合わなければ0回
    template <typename T, typename Fn> void WithId(InstanceId id, Fn &&fn) {
        if (!Exists(id)) {
            return;
        }
        if (T *typed = dynamic_cast<T *>(Find(id))) {
            fn(*typed);
        }
    }

    // Tとその派生のうち、有効なものを生成順に数えたindex番目
    template <typename T> T *Nth(std::size_t index) {
        static_assert(std::is_base_of_v<GameObject, T>);
        std::size_t seen = 0;
        for (Slot &slot : slots_) {
            if (!slot.alive || !slot.object->active) {
                continue;
            }
            if (T *typed = dynamic_cast<T *>(slot.object.get())) {
                if (seen == index) {
                    return typed;
                }
                seen += 1;
            }
        }
        return nullptr;
    }

    // Tとその派生が1つでも有効なら真
    template <typename T> bool Exists() {
        return First<T>() != nullptr;
    }

    // 生成順にfn(GameObject&)を渡す
    template <typename Fn> void ForEach(Fn &&fn) {
        const std::size_t count = slots_.size();
        for (std::size_t i = 0; i < count; ++i) {
            Slot &slot = slots_[i];
            if (slot.alive && slot.object->active) {
                fn(*slot.object);
            }
        }
    }

    // depthの大きい順にfn(GameObject&)を渡す
    // 同じdepthは生成順
    template <typename Fn> void ForEachByDepth(Fn &&fn) {
        order_.clear();
        const std::size_t count = slots_.size();
        for (std::size_t i = 0; i < count; ++i) {
            if (slots_[i].alive && slots_[i].object->active) {
                order_.push_back(i);
            }
        }

        std::stable_sort(order_.begin(), order_.end(),
                         [this](std::size_t lhs, std::size_t rhs) {
                             const double left = slots_[lhs].object->depth;
                             const double right = slots_[rhs].object->depth;
                             if (std::isnan(left)) {
                                 return false;
                             }
                             if (std::isnan(right)) {
                                 return true;
                             }
                             return left > right;
                         });

        for (const std::size_t index : order_) {
            Slot &slot = slots_[index];
            if (slot.alive && slot.object->active) {
                fn(*slot.object);
            }
        }
    }

    std::size_t Count() const {
        std::size_t total = 0;
        for (const Slot &slot : slots_) {
            if (slot.alive && slot.object->active) {
                total += 1;
            }
        }
        return total;
    }

    // Tとその派生を数える
    template <typename T> std::size_t Count() const {
        static_assert(std::is_base_of_v<GameObject, T>);
        std::size_t total = 0;
        for (const Slot &slot : slots_) {
            if (slot.alive && slot.object->active &&
                dynamic_cast<const T *>(slot.object.get()) != nullptr) {
                total += 1;
            }
        }
        return total;
    }

    // 生成したときの型が丁度Tのものを数える
    template <typename T> std::size_t CountExact() const {
        const auto found = byType_.find(TypeIdOf<T>());
        if (found == byType_.end()) {
            return 0;
        }
        std::size_t total = 0;
        for (const InstanceId id : found->second) {
            const Slot *slot = FindSlot(id);
            if (slot != nullptr && slot->alive && slot->object->active) {
                total += 1;
            }
        }
        return total;
    }

    // 破棄済みの領域を捨てる
    void Collect() {
        std::vector<Slot> kept;
        kept.reserve(slots_.size());
        for (Slot &slot : slots_) {
            if (slot.alive) {
                kept.push_back(std::move(slot));
            }
        }
        slots_ = std::move(kept);
        Reindex();
    }

    // 次に振られるID
    InstanceId NextId() const { return static_cast<InstanceId>(nextId_); }

    void SetCoverageHandler(CoverageHandler handler) { coverage_ = handler; }

private:
    using CloneFn = std::unique_ptr<GameObject> (*)(const GameObject &);

    template <typename T>
    static std::unique_ptr<GameObject> CloneOf(const GameObject &source) {
        return std::make_unique<T>(static_cast<const T &>(source));
    }

    struct Slot {
        InstanceId id = InstanceId::None;
        TypeId type = nullptr;
        CloneFn clone = nullptr;
        std::unique_ptr<GameObject> object;
        bool alive = false;

        Slot Copy() const {
            Slot copy;
            copy.id = id;
            copy.type = type;
            copy.clone = clone;
            copy.object = clone != nullptr ? clone(*object) : nullptr;
            copy.alive = alive;
            return copy;
        }
    };

public:
    // 巻き戻しに必要な状態の全て
    class Snapshot {
    public:
        Snapshot() = default;
        Snapshot(const Snapshot &) = delete;
        Snapshot &operator=(const Snapshot &) = delete;
        Snapshot(Snapshot &&) = default;
        Snapshot &operator=(Snapshot &&) = default;

    private:
        friend class Instances;

        std::vector<Slot> slots;
        std::uint32_t nextId = 1;
    };

    Snapshot Save() const {
        Snapshot snapshot;
        snapshot.slots.reserve(slots_.size());
        for (const Slot &slot : slots_) {
            snapshot.slots.push_back(slot.Copy());
        }
        snapshot.nextId = nextId_;
        return snapshot;
    }

    // 何度でも戻せる
    void Restore(const Snapshot &snapshot) {
        slots_.clear();
        slots_.reserve(snapshot.slots.size());
        for (const Slot &slot : snapshot.slots) {
            slots_.push_back(slot.Copy());
        }
        nextId_ = snapshot.nextId;
        Reindex();
    }

private:

    void Reindex() {
        byId_.clear();
        byType_.clear();
        for (std::size_t i = 0; i < slots_.size(); ++i) {
            byId_.emplace(static_cast<std::uint32_t>(slots_[i].id), i);
            byType_[slots_[i].type].push_back(slots_[i].id);
        }
    }

    Slot *FindSlot(InstanceId id) {
        const auto found = byId_.find(static_cast<std::uint32_t>(id));
        return found == byId_.end() ? nullptr : &slots_[found->second];
    }

    const Slot *FindSlot(InstanceId id) const {
        const auto found = byId_.find(static_cast<std::uint32_t>(id));
        return found == byId_.end() ? nullptr : &slots_[found->second];
    }

    template <typename T> void ReportCoverage(const T &object) {
#ifndef NDEBUG
        if (coverage_ == nullptr) {
            return;
        }
        static bool reported = false;
        if (reported) {
            return;
        }
        reported = true;
        const VariableCoverage coverage = CheckVariableCoverage(object);
        if (!coverage.Ok()) {
            coverage_(TypeIdOf<T>(), coverage);
        }
#else
        (void)object;
#endif
    }

    std::vector<Slot> slots_;
    std::vector<std::size_t> order_;
    std::unordered_map<std::uint32_t, std::size_t> byId_;
    std::unordered_map<TypeId, std::vector<InstanceId>> byType_;
    std::uint32_t nextId_ = 1;
    CoverageHandler coverage_ = nullptr;
};

} // namespace TellerEngine::Base
