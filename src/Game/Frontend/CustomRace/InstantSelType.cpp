// The custom race's window instant_seltype (docs/spec/02-frontend.md, "The custom race").

#include "Game/Frontend/CustomRace/InstantSelType.h"

#include "Game/Frontend/CustomRace/CustomRaceInternal.h"

namespace oj::frontend {

using namespace customrace;

// instant_seltype's enter (0x0045B0B0): the base enter; Menu and BackMenu back to their items (0x005AECF0), their first
// highlighted and both taking the events (+0x178 1). In a network game solo goes (with BackMenu's first item); online
// ([0x0075EF84]) cruise too in a session of a crew ([0x00728CEC] over 1, BackMenu's first item, 0x005AFAA0) and sprint
// in one of a crew or of more than four players ([0x0076A1E0]); a client's INSTRUCTION reads "please wait..." (1551)
// with every sel_text and selecta hidden. The item named by the race's type (3 solo, 4 cruise, 6 sprint, else race)
// is highlighted with BackMenu's of its place, the host sending it (0x004EA7F0); +0x0C -1.
void InstantSelType::enter(CustomRaceHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    s.restoreMenu("Menu");
    s.restoreMenu("BackMenu");
    s.selectMenuItem("Menu", 0);
    s.selectMenuItem("BackMenu", 0);
    s.setMenuActive("Menu", true);
    s.setMenuActive("BackMenu", true);
    if (net::NetGame* net = network(); net != nullptr) {
        const auto backFirstOut = [&s] {
            if (s.menuSize("BackMenu") != 0) s.removeMenuItemAt("BackMenu", 0);
        };
        s.removeMenuItem("Menu", "solo");
        backFirstOut();
        if (net::matchmaking() != nullptr) {
            const int32_t team = net::onlineHosted().career.team;
            if (team > 1) {
                s.removeMenuItem("Menu", "cruise");
                backFirstOut();
            }
            if (team > 1 || net->session.players().size() > 4) {
                s.removeMenuItem("Menu", "sprint");
                backFirstOut();
            }
        }
        if (!hosting()) {
            if (s.find("INSTRUCTION") >= 0) s.setText("INSTRUCTION", texts.at(kTextPleaseWait));
            hideNamed(s, "sel_text");
            hideNamed(s, "selecta");
        }
    }
    if (data == nullptr) return;
    const RaceType type = data->race.type;
    const char* name = type == RaceType::Solo     ? "solo"
                       : type == RaceType::Cruise ? "cruise"
                       : type == RaceType::Sprint ? "sprint"
                                                  : "race";
    for (size_t i = 0; i < s.menuSize("Menu"); ++i) {
        if (same(s.menuItemCommand("Menu", i), name)) {
            s.selectMenuItem("Menu", i);
            s.selectMenuItem("BackMenu", i);
            if (hosting()) network()->session.sendRaceType(static_cast<int32_t>(i));
        }
    }
}

// 0x0045B390: BackMenu follows Menu. On the network the shared checks (0x0046E0F0, the stage 1, as instant_manu's);
// a client highlights the item whose value is the type the host sent ([0x007191CC], gliding), Menu taking no events
// (+0x178 0), and goes no further. SELECT on an item: the host sends its value (0x004EA7F0); CLICK; cruise the
// profile's +0x634 0, type 4, laps 99999, one player, no opponents, clear (1) at morning (1), on the network the laps
// sent (0x004EA760); solo type 3, laps 99999, one player, no opponents; race type 5, a lap, one player; sprint type
// 6, +0x08 0, +0x0C 3 (1 on the network), a lap, one player; then instant_setup. BACK off the network (CLICK):
// instant_selcar (mode 3); on it the shared checks take BACK.
void InstantSelType::update(CustomRaceHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const size_t item = s.selectedMenuItem("Menu");
    if (s.find("BackMenu") >= 0) s.selectMenuItem("BackMenu", item);
    if (network() != nullptr) {
        if (host.netKeys(event, net::SessionStage::RaceKind, true)) return;
        if (client()) {
            const auto type = static_cast<float>(network()->session.raceType());
            for (size_t i = 0; i < s.menuSize("Menu"); ++i) {
                if (s.menuItemValue("Menu", i) == type) s.selectMenuItem("Menu", i, true);
            }
            s.setMenuActive("Menu", false);
            return;
        }
    }
    if (data == nullptr) return;
    RaceSetup& race = data->race;
    if (event == input::kSelect && s.menuSize("Menu") != 0) {
        if (hosting()) network()->session.sendRaceType(x87::chop(s.menuItemValue("Menu", item)));
        host.sound("CLICK");
        const std::string name = s.menuItemCommand("Menu", item);
        if (same(name, "cruise")) {
            if (data->career) data->career->profile.showoffTutorialShown = 0;
            race.type = RaceType::Cruise;
            race.laps = 99999;
            race.players = 1;
            race.opponents = 0;
            race.weather = 1;
            race.timeOfDay = 1;
            if (network() != nullptr) network()->session.sendLaps(99999);
        } else if (same(name, "solo")) {
            race.type = RaceType::Solo;
            race.laps = 99999;
            race.players = 1;
            race.opponents = 0;
        } else if (same(name, "race")) {
            race.type = RaceType::Circuit;
            race.laps = 1;
            race.players = 1;
        } else if (same(name, "sprint")) {
            race.type = RaceType::Sprint;
            race.heat = 0;
            race.heats = network() == nullptr ? 3 : 1;
            race.laps = 1;
            race.players = 1;
        }
        host.request("instant_setup", SwitchMode::Replace);
    } else if (event == input::kBack && network() == nullptr) {
        host.sound("CLICK");
        host.request("instant_selcar", SwitchMode::ReplaceAfterTransitions);
    }
}

}  // namespace oj::frontend
