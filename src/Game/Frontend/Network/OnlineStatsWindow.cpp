// online_stats (vtable 0x006F2EEC): the leaderboards.

#include "Game/Frontend/Network/OnlineStatsWindow.h"

#include "Core/Base/Text.h"
#include "Core/Base/X87.h"
#include "Core/Career/Money.h"
#include "Core/Cars/CarCollection.h"
#include "Core/Data/GameTexts.h"
#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Game/Frontend/WindowTools.h"
#include "Game/Race/Hud/RaceWindows.h"

#include <format>

namespace oj::frontend {

namespace {

bool same(const std::string& a, const char* b) {
    return _stricmp(a.c_str(), b) == 0;
}

// 0x0049A850: each byte widened with its sign. Stock problem: a byte from 0x80 up (a description in the ANSI code
// page) becomes 0xFF80 .. 0xFFFF, not its character. The fixed build reads it in the names' code page.
std::wstring widenSigned(const std::string& text) {
    return oj::widenSigned(text);
}

// [0x00715D44]: the rows asked for the table's top.
constexpr int32_t kTopRows = 20;

constexpr const char* kColumns[] = {"M_COL1", "M_COL2", "M_COL3", "M_COL4"};

std::wstring wide(std::u16string_view text) {
    return {text.begin(), text.end()};
}

}  // namespace

// 0x0047E3F0: GAMESPY shown; a table reader of the window's own (0x004ED270); +0x24 1, +0x18 1, +0x20 0, +0x2C 2, +0x10
// and +0x14 0, +0x30 -1, +0x38 -1; the columns emptied (0x0047E6E0) and no headers (0x0047E760 with 0); +0x28 the
// user of our last local player (vt+0x38 referenced, let go at once); the menus' text modifiers freed and the
// headers multiline (+0x128).
void OnlineStats::enter(WindowHost& host, ui::Screen& s) {
    s.show("GAMESPY", false);
    *this = OnlineStats{};
    m_reader = std::make_unique<net::stats::TableReader>();
    clearTable(s);
    headers(host, s, StatsCategory::None);
    m_user = -1;
    m_unused = -1;
    if (net::OnlineUser* u = net::onlineUser()) {
        for (size_t i = 0; i < u->localPlayers.size(); ++i) {
            const int32_t user = u->playerUser(u->localPlayers[i]);
            m_user = user;
            u->userRelease(user);
        }
    }
    for (const char* menu : {"M_COL1", "M_COL2", "M_COL3", "M_COL4", "M_RANK", "M_CATEGORY", "M_OPTION1", "M_OPTION2"}) {
        if (s.find(menu) >= 0) s.removeMenuModifier(menu);
    }
    for (const char* header : {"H_COL1", "H_COL2", "H_COL3", "H_COL4", "H_RANK"}) {
        if (s.find(header) >= 0) s.setMultiline(header, true);
    }
}

// 0x0047E6B0: the reader let go (the leaderboards reset first).
void OnlineStats::leave() {
    if (m_reader == nullptr) return;
    if (net::stats::Leaderboards* lb = net::stats::leaderboards()) lb->reset();
    m_reader.reset();
}

// 0x0047E6E0: the columns and M_RANK emptied, M_RANK's items cut to its box (+0x16C).
void OnlineStats::clearTable(ui::Screen& s) {
    for (const char* menu : kColumns) {
        if (s.find(menu) >= 0) s.clearMenu(menu);
    }
    if (s.find("M_RANK") >= 0) {
        s.clearMenu("M_RANK");
        s.setMenuClipped("M_RANK", true);
    }
}

// 0x0047E760: H_RANK "RANK"; the headers of the category: none; lap times NICKNAME, LAP TIME, CAR (M_COL3 left);
// world ranking NICKNAME, SCORE, WINS, RACES (centred); top cruise score NICKNAME, SCORE, CAR and H_COL4 "" (left);
// pink slips NICKNAME, WINS, RACES, WINNINGS (centred).
void OnlineStats::headers(WindowHost& host, ui::Screen& s, StatsCategory category) {
    const TextTable& texts = host.gameTexts();
    const auto set = [&](const char* name, size_t line) {
        if (const int e = s.find(name); e >= 0) s.setText(e, texts.at(line));
    };
    const auto clear = [&](const char* name) {
        if (const int e = s.find(name); e >= 0) s.clearText(e);
    };
    set("H_RANK", kTextRank);
    switch (category) {
    case StatsCategory::None:
        for (const char* name : {"H_COL1", "H_COL2", "H_COL3", "H_COL4"}) clear(name);
        break;
    case StatsCategory::LapTimes:
        set("H_COL1", kTextNickname);
        set("H_COL2", kTextLapTime);
        set("H_COL3", kTextCar);
        s.setMenuAlign("M_COL3", 1);
        clear("H_COL4");
        break;
    case StatsCategory::WorldRanking:
        set("H_COL1", kTextNickname);
        set("H_COL2", kTextScoreUpper);
        set("H_COL3", kTextWins);
        s.setMenuAlign("M_COL3", 0);
        set("H_COL4", kTextRaces);
        break;
    case StatsCategory::TopCruiseScore:
        set("H_COL1", kTextNickname);
        set("H_COL2", kTextScoreUpper);
        set("H_COL3", kTextCar);
        s.setMenuAlign("M_COL3", 1);
        if (const int e = s.find("H_COL4"); e >= 0) s.setText(e, L"");
        break;
    case StatsCategory::PinkSlips:
        set("H_COL1", kTextNickname);
        set("H_COL2", kTextWins);
        set("H_COL3", kTextRaces);
        s.setMenuAlign("M_COL3", 0);
        set("H_COL4", kTextWinnings);
        break;
    default: break;
    }
}

// 0x0047E940: the headers each frame; a table read not shown yet: the columns emptied, M_COL1, M_COL2 and M_RANK
// left, M_COL3's items cut to its box; each row read (the reader's; ours found by our first local player's nick
// highlighted, else the sixth) its name (fitted to M_COL1's width less 10) and by the category: lap times (with a
// car) the time and the car's name; world ranking the score, wins and races; top cruise score (with a car) the
// points with their thousands marked and the car's name; pink slips the wins, races and "$" winnings; the rank. With
// a total a last row: four empty items and "TOTAL: <n>". Shown, not busy.
void OnlineStats::fill(WindowHost& host, ui::Screen& s, StatsCategory category) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    headers(host, s, category);
    net::stats::TableReader& reader = *m_reader;
    if (reader.done == 0 || m_filled != 0) return;
    clearTable(s);
    s.setMenuAlign("M_COL1", 1);
    s.setMenuAlign("M_COL2", 1);
    s.setMenuAlign("M_RANK", 1);
    s.setMenuClipped("M_COL3", true);
    int32_t total = 0;
    const size_t rows = reader.columns.empty() ? 0 : reader.columns[0].size();
    std::u16string ourNick;
    bool haveNick = false;
    if (net::OnlineUser* u = net::onlineUser(); u != nullptr && !u->localPlayers.empty()) {
        ourNick = u->playerNick(u->localPlayers[0]);
        haveNick = true;
    }
    const int most = s.pixelWidth("M_COL1") - 10;
    const auto nameText = [&](const char16_t* name) {
        const std::u16string_view n(name);
        if (n.size() < 0x40) return s.fitTextWithin("M_COL1", wide(n), most);
        return wide(n.substr(0, 15));
    };
    const auto carName = [&](int32_t car) -> std::wstring {
        if (data == nullptr || car <= 0 || static_cast<size_t>(car) >= data->tables.carModels.size()) return {};
        const CarDef* def = findCarDef(data->defs, data->tables.carModels[static_cast<size_t>(car)]);
        return def != nullptr ? widenSigned(def->displayName) : std::wstring();
    };
    const auto carListed = [&](int32_t car) {
        return data != nullptr && car != 0 && static_cast<size_t>(car) < data->tables.carModels.size() &&
               !data->tables.carModels[static_cast<size_t>(car)].empty();
    };
    size_t highlight = 5;
    for (size_t k = 0; k < rows; ++k) {
        const auto row = static_cast<uint32_t>(k);
        const char16_t* name = nullptr;
        int32_t rank = 0;
        bool shown = false;
        switch (category) {
        case StatsCategory::LapTimes: {
            float time = 0.0f;
            float topSpeed = 0.0f;
            int32_t car = 0;
            int32_t carRating = 0;
            if (!reader.lapTimeRow(row, name, rank, time, car, carRating, topSpeed, total) || !carListed(car)) break;
            s.addMenuItem("M_COL1", nameText(name));
            s.addMenuItem("M_COL2", toWide(hudTime(time)));
            s.addMenuItem("M_COL3", carName(car));
            shown = true;
            break;
        }
        case StatsCategory::WorldRanking: {
            int32_t rating = 0, won = 0, lost = 0, disconnects = 0;
            if (!reader.worldRankingRow(row, name, rank, rating, won, lost, disconnects, total)) break;
            s.addMenuItem("M_COL1", nameText(name));
            s.addMenuItem("M_COL2", std::format(L"{}", rating));
            s.addMenuItem("M_COL3", std::format(L"{}", won));
            s.addMenuItem("M_COL4", std::format(L"{}", won + lost));
            shown = true;
            break;
        }
        case StatsCategory::TopCruiseScore: {
            int32_t points = 0, car = 0;
            if (!reader.showoffRow(row, rank, name, points, car, total) || !carListed(car)) break;
            s.addMenuItem("M_COL1", nameText(name));
            s.addMenuItem("M_COL2", toWide(groupThousands(points, host.language().index)));
            s.addMenuItem("M_COL3", carName(car));
            shown = true;
            break;
        }
        case StatsCategory::PinkSlips: {
            int32_t won = 0, lost = 0, money = 0;
            if (!reader.pinkSlipsRow(row, rank, name, won, lost, money, total)) break;
            s.addMenuItem("M_COL1", nameText(name));
            s.addMenuItem("M_COL2", std::format(L"{}", won));
            s.addMenuItem("M_COL3", std::format(L"{}", won + lost));
            s.addMenuItem("M_COL4", std::format(L"${}", money));
            shown = true;
            break;
        }
        default: break;
        }
        if (!shown) continue;
        s.addMenuItem("M_RANK", std::format(L"{}", rank));
        m_busy = 0;
        if (haveNick && name != nullptr && ourNick == name) highlight = k;
    }
    if (total != 0) {
        for (const char* menu : kColumns) s.addMenuItem(menu, L"");
        s.addMenuItem("M_RANK", texts.at(kTextTotal) + std::format(L"{}", total));
    }
    if (highlight < rows) {
        for (const char* menu : {"M_COL1", "M_COL2", "M_COL3", "M_COL4", "M_RANK"}) s.selectMenuItem(menu, highlight);
    }
    m_busy = 0;
    m_filled = 1;
}

// 0x0047F340: the category of M_CATEGORY's highlighted item (laptimes 1, worldranking 2, topcruisescore 3, pinkslips
// 4, else 0); a new one: not shown, kept, the reader reset (its total 0) and the columns emptied. A new level (+0x20
// against +0x24, +0x38 -1): its menu shown and taking the events, the others hidden and not; the first options (from
// the categories) the nine locations (value 1 .. 9); the second the routes of type 1, 2 or 4 of the location
// highlighted (0x004265C0), their place in the routes' table the value.
void OnlineStats::options(WindowHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    StatsCategory category = StatsCategory::None;
    if (s.find("M_CATEGORY") >= 0 && s.menuSize("M_CATEGORY") != 0) {
        const std::string item = s.menuItemCommand("M_CATEGORY", s.selectedMenuItem("M_CATEGORY"));
        category = same(item, "laptimes")         ? StatsCategory::LapTimes
                   : same(item, "worldranking")   ? StatsCategory::WorldRanking
                   : same(item, "topcruisescore") ? StatsCategory::TopCruiseScore
                   : same(item, "pinkslips")      ? StatsCategory::PinkSlips
                                                  : StatsCategory::None;
    }
    if (category != m_category) {
        m_filled = 0;
        m_category = category;
        m_reader->reset();
        m_reader->total = 0;
        clearTable(s);
    }
    if (m_level == m_levelShown) return;
    m_highlighted = -1;
    const auto shown = [&](const char* menu, bool on) {
        s.setMenuActive(menu, on);
        if (const int e = s.find(menu); e >= 0) s.setHidden(e, !on);
    };
    if (m_level == 0) {
        shown("M_CATEGORY", true);
        shown("M_OPTION1", false);
        shown("M_OPTION2", false);
    } else if (m_level == 1) {
        shown("M_CATEGORY", false);
        shown("M_OPTION1", true);
        shown("M_OPTION2", false);
        if (m_levelShown == 0) {
            s.clearMenu("M_OPTION1");
            for (uint32_t i = 1; i < 10; ++i) {
                s.addMenuItem("M_OPTION1", texts.at(kTextLocations + i), static_cast<float>(i));
            }
        }
    } else if (m_level == 2) {
        shown("M_CATEGORY", false);
        shown("M_OPTION1", false);
        shown("M_OPTION2", true);
        const int32_t location =
            s.menuSize("M_OPTION1") != 0 ? x87::chop(s.menuItemValue("M_OPTION1", s.selectedMenuItem("M_OPTION1"))) : 0;
        s.clearMenu("M_OPTION2");
        if (data != nullptr) {
            const std::vector<ExeTables::Route>& routes = data->tables.routes;
            for (size_t k = 0; k < routes.size() && k < 0x6D; ++k) {
                const ExeTables::Route& r = routes[k];
                if (r.location != location ||
                    !(r.type == RouteType::Circuit || r.type == RouteType::Sprint || r.type == RouteType::PointToPoint))
                    continue;
                s.addMenuItem("M_OPTION2", texts.at(frontend::routeNameLine(r.text, routes[0].text)),
                              static_cast<float>(k));
            }
        }
    }
    m_levelShown = m_level;
}

// 0x0047F590: not busy and the second between asks over: the table asked for (the next ask the other way: our
// position, 20 rows of our user, or the top, 20 rows): lap times of the route highlighted (none for 0), the world
// ranking, the top cruise score of the location highlighted, the pink slips; asked: not shown, busy.
bool OnlineStats::ask(ui::Screen& s) {
    if (m_busy != 0 || 0.0f < m_timer) return false;
    const bool around = m_aroundMe != 0;
    m_timer = 1.0f;
    const int32_t user = around ? m_user : -1;
    const int32_t rows = around ? 20 : kTopRows;
    m_aroundMe = around ? 0 : 1;
    const auto valueOf = [&](const char* menu) {
        return s.menuSize(menu) != 0 ? x87::chop(s.menuItemValue(menu, s.selectedMenuItem(menu))) : 0;
    };
    bool asked = false;
    switch (m_category) {
    case StatsCategory::LapTimes: {
        const int32_t route = valueOf("M_OPTION2");
        asked = route != 0 && m_reader->lapTimes(rows, route, user);
        break;
    }
    case StatsCategory::WorldRanking: asked = m_reader->worldRanking(rows, user); break;
    case StatsCategory::TopCruiseScore: asked = m_reader->showoff(rows, valueOf("M_OPTION1"), user); break;
    case StatsCategory::PinkSlips: asked = m_reader->pinkSlips(rows, user); break;
    default: break;
    }
    if (asked) {
        m_filled = 0;
        m_busy = 1;
    }
    return true;
}

// 0x0047F6C0: SELECT: the top cruise score's categories go on to the locations, the lap times' to the locations and
// the routes; else the table asked for.
bool OnlineStats::select(ui::Screen& s) {
    if (m_category == StatsCategory::TopCruiseScore && m_level == 0) {
        m_level = 1;
        return true;
    }
    if (m_category == StatsCategory::LapTimes && static_cast<uint32_t>(m_level) < 2) {
        ++m_level;
        return true;
    }
    if (m_level == 3) return false;
    ask(s);
    return true;
}

// 0x0047F720: the second between asks running down; the level's menu taking the events while not busy and no message
// is open; the statistics' problem: any but 6 the note "An error occurred .." and online_main; 6 cleared with the note
// "Your nickname was not found ..". SEL_TEXT "SELECT" while a category or location is to be chosen, else the next
// ask's "YOUR POSITION" or "TOP STATS"; the table (0x0047E940); BUSYICON and BUSYTEXT shown while busy. SELECT
// (CLICK) goes on (0x0047F6C0); else another highlight of the level's menu makes the next ask our position, and BACK
// (CLICK) goes a level back or to online_main. Then the options (0x0047F340).
void OnlineStats::update(WindowHost& host, ui::Screen& s, uint32_t event) {
    const TextTable& texts = host.gameTexts();
    if (m_reader == nullptr) return;
    if (0.0f < m_timer) m_timer = m_timer - host.frameTime();
    const char* menu = m_level == 0 ? "M_CATEGORY" : m_level == 1 ? "M_OPTION1" : m_level == 2 ? "M_OPTION2" : nullptr;
    const bool idle = !host.messageUp();
    if (menu != nullptr && s.find(menu) >= 0) s.setMenuActive(menu, m_busy == 0 && idle);
    if (net::stats::leaderboards() != nullptr && net::stats::problem() != 0) {
        if (net::stats::problem() != 6) {
            host.message(MessageType::Continue, texts.at(kTextStatsError), false, nullptr);
            host.request("online_main", SwitchMode::Replace);
            return;
        }
        net::stats::problem() = 0;
        host.message(MessageType::Continue, texts.at(kTextNotFound), false, nullptr);
    }
    if (const int e = s.find("SEL_TEXT"); e >= 0) {
        const bool choosing = (m_category == StatsCategory::TopCruiseScore && m_level < 1) ||
                              (m_category == StatsCategory::LapTimes && static_cast<uint32_t>(m_level) < 2);
        s.setText(e, texts.at(choosing ? kTextSelect : m_aroundMe != 0 ? kTextYourPosition : kTextTopStats));
    }
    fill(host, s, m_category);
    if (const int e = s.find("BUSYICON"); e >= 0) s.setHidden(e, m_busy == 0);
    if (const int e = s.find("BUSYTEXT"); e >= 0) s.setHidden(e, m_busy == 0);
    if (event == input::kSelect && idle) {
        if (select(s)) {
            host.sound("CLICK");
            options(host, s);
            return;
        }
    } else {
        if (idle && m_busy == 0 && menu != nullptr && s.find(menu) >= 0 &&
            m_highlighted != static_cast<int32_t>(s.selectedMenuItem(menu))) {
            m_aroundMe = 1;
            m_highlighted = static_cast<int32_t>(s.selectedMenuItem(menu));
        }
        if (event == input::kBack && idle) {
            host.sound("CLICK");
            if (m_level != 0) {
                --m_level;
                options(host, s);
                return;
            }
            host.request("online_main", SwitchMode::Replace);
        }
    }
    options(host, s);
}

}  // namespace oj::frontend
