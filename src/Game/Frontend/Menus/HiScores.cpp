// viewhiscores: the routes' records.

#include "Game/Frontend/Menus/HiScores.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

namespace oj::frontend {

// 0x0046A300: M_TRACK emptied and given the nine tracks (575 + n, values 1 .. 9), its first highlighted (its help
// ticker +0x184 the item's help: none in this window) and its text modifier deleted (0x0046A3D7: +0x10C 0, its
// highlighted item does not blink); M_COL1 .. M_COL4 (+0x10 ..) emptied, each its modifier deleted (0x0046A452); +0x24
// 0, +0x20 the highlighted track (to the nearest).
void HiScores::enter(WindowHost& host, ui::Screen& s) {
    s.clearMenu("M_TRACK");
    for (int i = 1; i < 10; ++i) {
        s.addMenuItem("M_TRACK", host.gameTexts().at(kTextLocations + static_cast<size_t>(i)), static_cast<float>(i));
    }
    s.selectMenuItem("M_TRACK", 0);
    s.removeMenuModifier("M_TRACK");
    for (int n = 1; n <= 4; ++n) {
        const std::string column = std::format("M_COL{}", n);
        s.clearMenu(column);
        s.removeMenuModifier(column);
    }
    m_built = 0;
    m_track = s.menuSize("M_TRACK") != 0 ? x87::chop(s.menuItemValue("M_TRACK", 0)) : 0;
}

// 0x0046A520: a new track highlighted clears +0x24. +0x24 0: the four lists emptied and for each route of the track
// (the routes' table in order, 0x004265C0) M_COL1 its name (value its place in the table), M_COL2 its record's driver
// fitted to the list's width (0x005A8F50), M_COL4 the record's car (211 + its id) or "-", M_COL3 the record: a showoff
// (type 3) its points with the thousands marked, 300 s "--:--:--", else the time (0x0044FBD0); each list's highlight
// then M_COL1's before (0 past the list; their help tickers: none in the window) and +0x24 1. BACK (CLICK) extras.
void HiScores::update(WindowHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr) return;
    const int32_t track =
        s.menuSize("M_TRACK") != 0 ? x87::chop(s.menuItemValue("M_TRACK", s.selectedMenuItem("M_TRACK"))) : 0;
    if (m_track != track) {
        m_track = track;
        m_built = 0;
    }
    if (m_built == 0) {
        const size_t kept = s.selectedMenuItem("M_COL1");
        for (int n = 1; n <= 4; ++n) s.clearMenu(std::format("M_COL{}", n));
        const std::vector<ExeTables::Route>& routes = data->tables.routes;
        for (size_t k = 0; k < routes.size() && k < HighScores::kRoutes; ++k) {
            const ExeTables::Route& r = routes[k];
            if (r.location != track) continue;
            s.addMenuItem("M_COL1", texts.at(routeNameLine(r.text, routes[0].text)), static_cast<float>(k));
            const RouteRecord& record = data->highScores.routes[k];
            std::wstring driver;
            for (size_t c = 0; c + 1 < record.name.size(); c += 2) {
                const auto ch = static_cast<wchar_t>(record.name[c] | (record.name[c + 1] << 8));
                if (ch == 0) break;
                driver.push_back(ch);
            }
            s.addMenuItem("M_COL2", s.fitText("M_COL2", driver));
            s.addMenuItem("M_COL4", record.car != 0 ? texts.at(kTextModels + static_cast<size_t>(record.car))
                                                    : std::wstring(L"-"));
            std::string time;
            if (r.type == RouteType::Cruise) {
                time = groupThousands(x87::chop(record.time), host.language().index);
            } else if (std::bit_cast<uint32_t>(record.time) == 0x43960000) {
                time = "--:--:--";
            } else {
                time = hudTime(record.time);
            }
            s.addMenuItem("M_COL3", toWide(time));
        }
        for (int n = 1; n <= 4; ++n) s.selectMenuItem(std::format("M_COL{}", n), kept);
        m_built = 1;
    }
    if (event == input::kBack) {
        host.sound("CLICK");
        host.request("extras", SwitchMode::Replace);
    }
}

}  // namespace oj::frontend
