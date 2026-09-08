#pragma once

// テスト用にContextの一式をまとめる

#include <Base/Canvas.hpp>
#include <Base/Context.hpp>
#include <Base/Draw.hpp>
#include <Base/Globals.hpp>
#include <Base/Input.hpp>
#include <Base/Instances.hpp>
#include <Base/Loop.hpp>
#include <Base/Random.hpp>
#include <Base/Scheduler.hpp>

namespace TellerTest {

struct World {
    TellerEngine::Base::Instances instances;
    TellerEngine::Base::Globals globals;
    TellerEngine::Base::Random random;
    TellerEngine::Base::LoopCycle cycle;
    TellerEngine::Base::Input input;
    TellerEngine::Base::Context context{instances, globals, random, cycle.Clock(), input};
    TellerEngine::Base::DrawList drawList;
    TellerEngine::Base::Canvas canvas{drawList};
    TellerEngine::Base::Scheduler scheduler;
};

} // namespace TellerTest
