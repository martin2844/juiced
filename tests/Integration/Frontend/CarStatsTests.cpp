// Integration tests: the car's stats window Car_Stats2 on a career of the game's data, the game's window and texts
// through a front end of the tests (Framework/Fakes/FakeCareerHost.h).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FakeCareerHost.h"

#include "Core/Base/Random.h"
#include "Core/Data/GameData.h"
#include "Game/Frontend/CustomRace/CustomRaceInternal.h"
#include "Game/Frontend/Garage/CarStats.h"
#include "Game/Frontend/WindowTools.h"

#include <memory>
#include <string>

namespace {

using oj::test::check;

}  // namespace

// The window is kept from one visit to the next: its leave (0x004C4710) puts the blinking icons' alpha back to 1, so
// an icon that blinked on the last visit and does not on the next (the part bought: ui_mod_got) is not left faded.
OJ_INTEGRATION_TEST(CarStats, LeaveRestoresBlinkAlpha) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    if (!oj::test::sharedGameData().carDefsRead) return;
    oj::GameData data(oj::test::gameFiles().root());
    oj::CrtRandom random;
    std::string error;
    if (!data.load(random, &error) || !data.career || data.defs.cars.empty()) {
        check(false, "game data and a first career loaded: " + error);
        return;
    }
    oj::Car car;
    car.def = &data.defs.cars.front();
    oj::frontend::customrace::clearParts(car);
    data.career->profile.partsA[static_cast<size_t>(car.def->id)][0] = 0;  // the suspension's first level unlocked
    oj::test::FakeCareerHost host;
    host.data = &data;
    host.dt = 0.5f;  // the phase 4: the alpha (sin 4 + 1) x 0.5, 0.12
    const std::shared_ptr<oj::ui::Screen> screen = host.open("Car_Stats2");
    if (screen == nullptr) return;
    oj::frontend::g_statsCar = &car;
    oj::frontend::CarStats stats;
    stats.setGarage(1);
    stats.enter(host, *screen);
    stats.update(host, *screen, 0);
    const int key = screen->find("KeyIcon3");
    const int part = screen->find("P2-1");
    const uint32_t keyFaded = screen->colourOf(key) >> 24;
    const uint32_t partFaded = screen->colourOf(part) >> 24;
    stats.leave(*screen);
    const uint32_t keyAfter = screen->colourOf(key) >> 24;
    const uint32_t partAfter = screen->colourOf(part) >> 24;
    oj::frontend::g_statsCar = nullptr;
    check(keyFaded == 31 && partFaded == 31,
          "a garage car's KeyIcon3 and its first suspension level (no part, the level unlocked) blink (" +
              std::to_string(keyFaded) + ", " + std::to_string(partFaded) + ")");
    check(keyAfter == 255 && partAfter == 255,
          "the leave: their alpha back to 1 (" + std::to_string(keyAfter) + ", " + std::to_string(partAfter) + ")");
}
