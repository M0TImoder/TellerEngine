#pragma once

// テスト用にContextの一式をまとめる

#include <Base/Context.hpp>
#include <Base/Globals.hpp>
#include <Base/Input.hpp>
#include <Base/Instances.hpp>
#include <Base/Loop.hpp>
#include <Base/Random.hpp>

namespace TellerTest {

struct World {
    TellerEngine::Base::Instances instances;
    TellerEngine::Base::Globals globals;
    TellerEngine::Base::Random random;
    TellerEngine::Base::LoopCycle cycle;
    TellerEngine::Base::Input input;
    TellerEngine::Base::Context context{instances, globals, random, cycle.Clock(), input};
};

} // namespace TellerTest
