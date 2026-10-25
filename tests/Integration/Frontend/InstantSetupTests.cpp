// Integration tests: the custom race's setup (instant_setup), the game's window and texts through a front end of the
// tests (Framework/Fakes/FakeCustomRaceHost.h).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FakeCustomRaceHost.h"

#include "Core/Data/GameData.h"
#include "Core/Input/Input.h"
#include "Game/Frontend/CustomRace/InstantSetup.h"

#include <memory>

namespace {

using oj::test::check;
using oj::test::FakeCustomRaceHost;

}  // namespace

// Each page's items come in from black: the page's start empties the menu and starts its transition (a fade in of
// 0.2 s) again (0x0045C4BC .. 0x0045C4EE, the same in each page), so SELECT on the location brings the routes in, not
// at once.
OJ_INTEGRATION_TEST(InstantSetup, PagesComeInFromBlack) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    FakeCustomRaceHost host;
    oj::GameData data(oj::test::gameFiles().root());
    data.tables = oj::test::sharedGameData().tables;  // the city places
    data.race.mode = oj::RaceMode::Custom;
    data.race.type = oj::RaceType::TimedLap;
    host.data = &data;
    const std::shared_ptr<oj::ui::Screen> s = host.open("instant_setup");
    if (s == nullptr) return;
    oj::frontend::InstantSetup window;
    window.enter(host, *s);
    window.update(host, *s, 0);
    const oj::ui::Transition* menu = s->transitionOf("menu");
    if (menu == nullptr) {
        check(false, "instant_setup's menu: its transition");
        return;
    }
    s->finishTransitions();
    const bool settled = menu->remaining <= 0.0f && s->menuSize("menu") != 0;
    window.update(host, *s, oj::input::kSelect);
    window.update(host, *s, 0);
    check(settled && s->menuSize("menu") != 0 && !menu->out && menu->duration > 0.0f &&
              menu->remaining == menu->duration,
          "instant_setup: the route page's menu comes in again after the location's");
    window.leave();
}
