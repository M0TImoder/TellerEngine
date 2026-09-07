#pragma once

#include <Base/Globals.hpp>
#include <Base/Input.hpp>
#include <Base/Instances.hpp>
#include <Base/Loop.hpp>
#include <Base/Random.hpp>
#include <Base/Scheduler.hpp>

#include <cstdint>
#include <type_traits>

namespace TellerEngine::Base {

// 巻き戻しに要るものを1つにまとめる
template <typename G> struct StateSnapshot {
    static_assert(std::is_base_of_v<Globals, G>);

    Instances::Snapshot instances;
    G globals;
    Random::Snapshot random;
    LoopCycle::Snapshot loop;
    Input::Snapshot input;
    std::uint64_t frame = 0;
};

template <typename G>
StateSnapshot<G> SaveState(const Instances &instances, const G &globals, const Random &random,
                           const LoopCycle &loop, const Input &input,
                           const Scheduler &scheduler) {
    StateSnapshot<G> snapshot;
    snapshot.instances = instances.Save();
    snapshot.globals = globals;
    snapshot.random = random.Save();
    snapshot.loop = loop.Save();
    snapshot.input = input.Save();
    snapshot.frame = scheduler.Frame();
    return snapshot;
}

template <typename G>
void RestoreState(const StateSnapshot<G> &snapshot, Instances &instances, G &globals,
                  Random &random, LoopCycle &loop, Input &input, Scheduler &scheduler) {
    instances.Restore(snapshot.instances);
    globals = snapshot.globals;
    random.Restore(snapshot.random);
    loop.Restore(snapshot.loop);
    input.Restore(snapshot.input);
    scheduler.SetFrame(snapshot.frame);
}

} // namespace TellerEngine::Base
