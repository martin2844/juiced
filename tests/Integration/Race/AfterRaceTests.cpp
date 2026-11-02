// Integration tests: the front end's first window after a race (Game/Race/RaceGrids, windowAfterRace, the stock's
// job 0x00461A10 on the loading thread) with the game's data.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"

#include "Core/Base/Random.h"
#include "Core/Data/GameData.h"
#include "Game/Race/RaceGrids.h"

#include <string>

namespace {

using oj::test::check;

}  // namespace

// A career race's end: the session's end (0x004A3A40) draws the loading thread's rand() (the job runs there, one
// thread for the game), going on from its state, and none of the game's.
OJ_INTEGRATION_TEST(AfterRace, CareerSessionEndDrawsTheLoadingThreads) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    if (!oj::test::sharedGameData().carDefsRead) return;
    oj::GameData data(oj::test::gameFiles().root());
    oj::CrtRandom random;
    std::string error;
    if (!data.load(random, &error) || !data.career) {
        check(false, "game data and a first career loaded: " + error);
        return;
    }
    data.enterCareer(random, "Tester");
    check(data.race.mode == oj::RaceMode::Career && data.race.type != oj::RaceType::TestTrack,
          "a career race set up");
    oj::CrtRandom& loading = oj::loadingRandom();
    const uint64_t loadingBefore = loading.draws();
    const uint64_t gameBefore = oj::gameRandom().draws();
    const std::string window = app::windowAfterRace(data);
    const uint64_t drawn = loading.draws() - loadingBefore;
    check(drawn > 0 && oj::gameRandom().draws() == gameBefore,
          "the session's end drew " + std::to_string(drawn) + " of the loading thread's rand(), none of the game's (" +
              window + " next)");
}
