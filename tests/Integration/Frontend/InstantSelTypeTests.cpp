// Integration tests: the custom race's choice of the race's type (instant_seltype), the game's window and texts
// through a front end of the tests (Framework/Fakes/FakeCustomRaceHost.h), off the network and on it.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FakeCustomRaceHost.h"

#include "Core/Base/Random.h"
#include "Core/Data/GameData.h"
#include "Core/Input/Input.h"
#include "Core/Network/NetSession.h"
#include "Game/Frontend/CustomRace/InstantSelType.h"

#include <memory>
#include <string>
#include <vector>

namespace {

using oj::test::check;
using oj::test::FakeCustomRaceHost;

// The Menu item of that name, or the menu's size.
size_t itemNamed(const oj::ui::Screen& s, const char* name) {
    for (size_t i = 0; i < s.menuSize("Menu"); ++i) {
        if (_stricmp(s.menuItemCommand("Menu", i).c_str(), name) == 0) return i;
    }
    return s.menuSize("Menu");
}

}  // namespace

// Off the network: SELECT on sprint makes a race of three heats; BACK goes to instant_selcar after the transitions.
OJ_INTEGRATION_TEST(InstantSelType, OffLine) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    FakeCustomRaceHost host;
    oj::GameData data(oj::test::gameFiles().root());
    host.data = &data;
    const std::shared_ptr<oj::ui::Screen> s = host.open("instant_seltype");
    if (s == nullptr) return;
    oj::frontend::InstantSelType window;
    window.enter(host, *s);
    s->selectMenuItem("Menu", itemNamed(*s, "sprint"));
    window.update(host, *s, oj::input::kSelect);
    const bool sprint = data.race.type == oj::RaceType::Sprint && data.race.heats == 3 &&
                        host.states() == std::vector<std::string>{"instant_setup"};
    host.requests.clear();
    host.switchPending = false;
    window.update(host, *s, oj::input::kBack);
    check(sprint && host.states() == std::vector<std::string>{"instant_selcar"} && host.netKeysAsked == 0,
          "instant_seltype off line: sprint of three heats, BACK to instant_selcar");
}

// A client (0x0045B3F2): the shared checks, then the item of the type the host sent highlighted gliding, Menu taking
// no events, nothing more (SELECT and BACK do nothing here).
OJ_INTEGRATION_TEST(InstantSelType, ClientFollowsTheHost) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    FakeCustomRaceHost host;
    oj::GameData data(oj::test::gameFiles().root());
    host.data = &data;
    const std::shared_ptr<oj::ui::Screen> s = host.open("instant_seltype");
    if (s == nullptr) return;
    oj::net::Transport::Options& options = oj::net::netOptions();
    const oj::net::Transport::Options kept = options;
    options.lanPort = 18766;
    options.gamePort = 18767;
    options.shareLanPort = true;
    oj::CrtRandom random;
    oj::net::NetGame& game = oj::net::startNetGame(random);
    const auto type = static_cast<size_t>(game.session.raceType());
    oj::frontend::InstantSelType window;
    s->selectMenuItem("Menu", type == 0 ? 2 : 0);
    window.update(host, *s, oj::input::kSelect);
    const bool followed = s->selectedMenuItem("Menu") == type && s->menuPosition("Menu") != static_cast<float>(type);
    check(followed && host.netKeysAsked == 1 && host.requests.empty() && host.sounds.empty(),
          "instant_seltype's client: the host's type highlighted gliding, SELECT not taken");
    oj::net::endNetGame();
    options = kept;
}
