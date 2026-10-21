// Unit tests: the race HUD (Game/Race/Hud/RaceHud.h): its frame hiding the windows (0x0044D827) and the races keeping
// the records' panels.

#include "Framework/Test.h"
#include "Framework/Fakes/NullRenderDevice.h"

#include "Core/Data/Language.h"
#include "Game/Race/Hud/HudInternal.h"
#include "Game/Race/Hud/RaceHud.h"
#include "Game/Race/RaceCamera.h"

namespace {

using oj::test::check;
using Camera = oj::scene::RaceCamera;

bool hides(int mode, bool lookBack, float pan, size_t players) {
    oj::test::NullRenderDevice device;
    const oj::Language language = *oj::Language::byName("english");
    oj::TextTable texts;
    oj::RaceHud hud(device, language, texts);
    oj::HudRace race;
    race.players.resize(players);
    race.cameraMode = mode;
    race.cameraLookBack = lookBack;
    race.cameraPan = pan;
    hud.update(race, 1.0f / 60.0f);
    return hud.hidesWindows();
}

}  // namespace

// The fly-by and the sweep before the start hide the windows whatever the players: the HUD the pause's CONTINUE asks
// for during the fly-by shows nothing until the race's view (the HUD drawn early after a pause in the fly-by).
OJ_UNIT_TEST(RaceHudWindows, FlyByAndSweep) {
    check(hides(Camera::kFlyBy, false, 0.0f, 1) && hides(Camera::kBeforeStart, false, 0.0f, 1) &&
              hides(Camera::kFlyBy, false, 0.0f, 2) && hides(Camera::kBeforeStart, false, 0.0f, 2),
          "the fly-by and the sweep: no window drawn, with one player or two");
    check(!hides(Camera::kChase, false, 0.0f, 1) && !hides(Camera::kBumper, false, 0.0f, 1),
          "the views behind and on the car: the windows drawn");
}

// With one player looking back, or panning from the bumper or the bonnet, the windows are not drawn; with two they are.
OJ_UNIT_TEST(RaceHudWindows, LookingAside) {
    check(hides(Camera::kChase, true, 0.0f, 1) && hides(Camera::kBumper, false, 0.5f, 1) &&
              hides(Camera::kBonnet, false, -0.5f, 1),
          "one player: looking back, the bumper's and the bonnet's pan hide the windows");
    check(!hides(Camera::kChase, false, 0.5f, 1), "a pan from behind the car does not");
    check(!hides(Camera::kChase, true, 0.0f, 2) && !hides(Camera::kBumper, false, 0.5f, 2),
          "two players: looking back or panning does not");
}

// The races whose records' panels never fade (0x00448E01, 0x0044C6E0: types 1, 2, 6, 4, 3, 0): the lap time
// challenge keeps its target (it faded after the start, and again after each pause, till the next lap).
OJ_UNIT_TEST(RaceHudPanels, Kept) {
    using oj::RaceType;
    using oj::hud::keepsPanels;
    check(keepsPanels(RaceType::TimedLap) && keepsPanels(RaceType::PerfectLap) && keepsPanels(RaceType::Speed) &&
              keepsPanels(RaceType::Solo) && keepsPanels(RaceType::Cruise) && keepsPanels(RaceType::Sprint),
          "the lap time, perfect lap, speed, solo, cruise and sprint races keep the panels");
    check(!keepsPanels(RaceType::Circuit) && !keepsPanels(RaceType::PointToPoint) &&
              !keepsPanels(RaceType::Unused8) && !keepsPanels(RaceType::TestTrack),
          "the circuit, point to point, type 8 and test track races fade them");
}
