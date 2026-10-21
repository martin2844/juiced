// The custom race's window instant_manu (docs/spec/02-frontend.md, "The custom race").

#include "Game/Frontend/CustomRace/InstantManu.h"

#include "Game/Frontend/CustomRace/CustomRaceInternal.h"

namespace oj::frontend {

using namespace customrace;

namespace {

// [0x007253B0]: set at start; instant_manu's first visit highlights the first group with an open car, then clears it.
bool g_manuFirstVisit = true;

}  // namespace

// instant_manu's enter (0x0045E490): [game + 0x18] + 0x2C -1 (the second player's device let go); CarMenu emptied,
// +0x10 -1; Menu taking the events. On the first visit ([0x007253B0]) Menu highlights the first group holding a car
// that is not locked (the arcade progress's +0x52F0). On the network a client's TITLE reads "please wait..." (1551)
// and its sel_text and selecta go (nothing is sent: Stock problem, the clients show the group sent last until the host
// moves). Every arcade car's parts cleared (0x004600C0's fields).
void InstantManu::enter(CustomRaceHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    *this = InstantManu{};
    host.askSecondPlayerRelease();
    if (s.find("CarMenu") >= 0) s.clearMenu("CarMenu");
    if (data == nullptr) return;
    GameData& d = *data;
    for (size_t g = 0; g < d.arcadeGroups.ids.size(); ++g) {
        for (int32_t id : d.arcadeGroups.ids[g]) {
            if (id == 0) break;
            if (!carLocked(d.arcade, id) && g_manuFirstVisit) {
                const size_t items = s.menuSize("Menu");
                s.selectMenuItem("Menu", items != 0 && g > items - 1 ? 0 : g);
                g_manuFirstVisit = false;
            }
        }
    }
    if (client()) {
        if (s.find("TITLE") >= 0) s.setText("TITLE", texts.at(kTextPleaseWait));
        hideNamed(s, "sel_text");
        hideNamed(s, "selecta");
    }
    for (Car& car : d.arcadeCars) clearParts(car);
}

// 0x0045E790: when Menu's highlight is not the group CarMenu lists (+0x10) CarMenu is filled again: group 7 "ALL CARS"
// (1910), else the display names of the group's cars (0x0048CC30), each locked one disabled off line; the middle item
// highlighted, CarMenu's text modifier deleted (0x0045E8FD: +0x10C 0, so its highlighted item does not blink) and
// CarMenu taking no events (+0x178 0). BackMenu follows Menu. On the network the shared checks
// (0x0046E0F0, the stage 1); a client highlights the group the host sent ([0x007191CC], gliding) with Menu and
// backmenu taking no events, and goes no further. UP / DOWN of the host send the group highlighted (0x004EA7F0).
// SELECT on an item (CLICK): the group its value less 1 ([G+0x84]+0x784), instant_selcar (a split screen race, mode 3:
// instant_setup; the host sends the group and goes to instant_setup). BACK (CLICK, off line): menu (mode 3: multi).
void InstantManu::update(CustomRaceHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr) return;
    GameData& d = *data;
    const size_t group = s.selectedMenuItem("Menu");
    if (s.find("CarMenu") >= 0 && m_shown != static_cast<int32_t>(group)) {
        m_shown = static_cast<int32_t>(group);
        s.clearMenu("CarMenu");
        if (group == 7) {
            s.addMenuItem("CarMenu", texts.at(kTextAllCars), 0.0f);
        } else if (group < d.arcadeGroups.ids.size()) {
            for (int32_t id : d.arcadeGroups.ids[group]) {
                if (id == 0) break;
                const CarDef* def = d.defs.byId(id);
                s.addMenuItem("CarMenu", def != nullptr ? latin(def->displayName) : std::wstring(), 0.0f);
                if (carLocked(d.arcade, id) && network() == nullptr) {
                    s.setMenuItemEnabledAt("CarMenu", s.menuSize("CarMenu") - 1, false);
                }
            }
        }
        if (const size_t items = s.menuSize("CarMenu"); items != 0) {
            s.selectMenuItem("CarMenu", items / 2);
            s.removeMenuModifier("CarMenu");
            s.setMenuActive("CarMenu", false);
        }
    }
    if (s.find("backmenu") >= 0) s.selectMenuItem("backmenu", group);
    if (network() != nullptr && host.netKeys(event, net::SessionStage::RaceKind, true)) return;
    if (s.find("Menu") < 0) return;
    if (client()) {
        s.selectMenuItem("Menu", static_cast<size_t>(network()->session.raceType()), true);
        s.setMenuActive("Menu", false);
        if (s.find("backmenu") >= 0) s.setMenuActive("backmenu", false);
        return;
    }
    if (s.find("backmenu") >= 0) s.setMenuActive("backmenu", true);
    if ((event == input::kUp || event == input::kDown) && hosting() && s.menuSize("Menu") != 0) {
        network()->session.sendRaceType(x87::chop(s.menuItemValue("Menu", s.selectedMenuItem("Menu"))) - 1);
    } else if (event == input::kSelect && s.menuSize("Menu") != 0) {
        d.arcadeGroups.group = x87::chop(s.menuItemValue("Menu", group)) - 1;
        host.sound("CLICK");
        if (d.race.mode == RaceMode::SplitScreen) {
            host.request("instant_setup", SwitchMode::Replace);
        } else if (network() != nullptr) {
            network()->session.sendRaceType(d.arcadeGroups.group);
            host.request("instant_setup", SwitchMode::Replace);
        } else {
            host.request("instant_selcar", SwitchMode::Replace);
        }
    } else if (event == input::kBack && network() == nullptr) {
        host.sound("CLICK");
        host.request(d.race.mode == RaceMode::SplitScreen ? "multi" : "menu", SwitchMode::Replace);
    }
}

}  // namespace oj::frontend
