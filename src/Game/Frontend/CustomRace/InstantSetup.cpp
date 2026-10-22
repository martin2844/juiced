// The custom race's window instant_setup (docs/spec/02-frontend.md, "The custom race").

#include "Game/Frontend/CustomRace/InstantSetup.h"

#include "Game/Frontend/CustomRace/CustomRaceInternal.h"

namespace oj::frontend {

using namespace customrace;

namespace {

// [0x0075ED14]: the location item instant_setup highlighted last.
int32_t g_setupLocation = 0;

// 0x00531890: the routes of a track: the names of the routes of tracks\<track>\<track>.tdf ("<route>.wld"), in order.
std::vector<std::string> trackRoutes(const std::string& track) {
    std::vector<std::string> names;
    const GameFiles files(std::filesystem::current_path());
    const std::filesystem::path path = std::filesystem::path("tracks") / track / (track + ".tdf");
    const auto bytes = files.read(path);
    if (!bytes) return names;
    const std::vector<uint8_t> data(reinterpret_cast<const uint8_t*>(bytes->data()),
                                    reinterpret_cast<const uint8_t*>(bytes->data()) + bytes->size());
    const auto tdf = parseTdf(data);
    if (!tdf) return names;
    for (const TdfRoute& r : tdf->routes) names.push_back(r.name);
    return names;
}

}  // namespace

// instant_setup's enter (0x0045BFE0): +0x1C -1; a cruise off line (no network game) not asked yet (the profile's +0x634) asks "VIEW
// SHOWOFF TUTORIAL?" (1911, into +0x20) and marks it asked. The menu shown and taking the events. The map
// (0x0045B6B0): MAP3D's camera from 4000 to 5 000 000, its sky (0x005B4660), the rig's pitch within -1.5 .. -0.15,
// the atmosphere "map", its height 0 and its model 1. HELP (+0x24, none in the window); [game + 4]'s method +0x34
// with 1 (empty on PC). +0x18 -1, the page 0, the menu to fill, +0x138 and +0x13C 0. Each city place's picture
// loaded (0x005A8B70; here as it is drawn). Laps: a race of another type than 0 .. 4 and 6 3 (the settings' and the
// race's); opponents: 0 for the types 0 .. 4, 3 for a sprint, else 5; the difficulty 3.0; the time of day 1 for a
// cruise, else by rand() % 3 (1 afternoon, 2 evening, else morning); clear weather; LOCKED hidden. On the network a
// client's HELP, sel_text and selecta hidden; the host sends the laps it set, the difficulty 3, the time of day and
// the clear weather, in that order (Stock problem: a client sets its own, drawn from its rand(), until the next stage
// brings the host's).
void InstantSetup::enter(CustomRaceHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    *this = InstantSetup{};
    if (data == nullptr) return;
    GameData& d = *data;
    RaceSetup& race = d.race;
    net::NetGame* net = network();
    if (d.career && race.type == RaceType::Cruise && net == nullptr) {
        if (d.career->profile.showoffTutorialShown == 0) {
            host.message(MessageType::YesNo, texts.at(kTextShowoffTutorial), false, &m_tutorial, true);
            d.career->profile.showoffTutorialShown = 1;
        }
    }
    s.setHidden("menu", false);
    s.setMenuActive("menu", true);
    m_map.start(s);
    if (client()) {
        hideNamed(s, "HELP");
        hideNamed(s, "sel_text");
        hideNamed(s, "selecta");
    }
    save::Settings& set = host.optionSettings();
    const RaceType type = race.type;
    if (type != RaceType::PerfectLap && type != RaceType::TimedLap && type != RaceType::Sprint &&
        type != RaceType::Cruise && type != RaceType::Solo && type != RaceType::Speed) {
        set.setU32(save::Settings::kCustomLaps, 3);
        race.laps = 3;
        if (hosting()) net->session.sendLaps(3);
    }
    int32_t opponents = 5;
    if (type == RaceType::Cruise || type == RaceType::Solo || type == RaceType::PerfectLap ||
        type == RaceType::TimedLap || type == RaceType::Speed) {
        opponents = 0;
    } else if (type == RaceType::Sprint) {
        opponents = 3;
    }
    set.setU32(save::Settings::kCustomOpponents, static_cast<uint32_t>(opponents));
    race.opponents = opponents;
    set.setU32(save::Settings::kCustomDifficulty, std::bit_cast<uint32_t>(3.0f));
    race.difficulty = 3.0f;
    if (hosting()) net->session.sendDifficulty(3);
    if (type == RaceType::Cruise) {
        race.timeOfDay = 1;
    } else {
        const auto r = static_cast<uint32_t>(gameRandom().next()) % 3;
        race.timeOfDay = r == 1 ? 2 : r == 2 ? 3 : 1;
    }
    if (hosting()) net->session.sendTimeOfDay(race.timeOfDay);
    race.weather = 1;
    if (hosting()) net->session.sendWeather(1);
    s.hideContaining("LOCKED");
    m_locked = -1;
}

// 0x0045C420: the map freed (+0x10); [game + 4]'s method +0x34 with 0 (empty on PC).
void InstantSetup::leave() {
    m_map.release();
}

// The pages' common start (0x0045C4BC, 0x0045C739, 0x0045CD2F, 0x0045D01F, 0x0045D300, 0x0045D5D4, 0x0045D884):
// the menu shown and emptied (0x005AEEF0), and its transition started again, so each page's items come in from
// black.
void InstantSetup::menuCleared(ui::Screen& s) {
    s.setHidden("menu", false);
    s.clearMenu("menu");
    s.restartTransition("menu");
}

// 0x0045C450, the location page (+0x138 1). Filling: the route models taken off the map (0x0045BD10), the menu
// emptied; each city place with a route open for the race (0x0042B7B0: a cruise's of type 3, a sprint's of type 2,
// else a circuit or a point-to-point) an item: its text, its place as the value, its name; +0x18 -1; the item kept last
// ([0x0075ED14]) highlighted; HELP "select location" (58). Each frame with an item highlighted whose value is not
// +0x18: +0x18 the value (to the nearest), [0x0075ED14] the item, the map at the place (0x0045BE00: looking at its
// point, the map mode on the arm (0, 0, -240000), pitch -0.4 with the yaw kept), the race's track the item's name (31
// characters); LOCKED hidden.
void InstantSetup::showLocations(CustomRaceHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    GameData& d = *data;
    m_locationPage = true;
    const RouteType type = d.race.type == RaceType::Sprint   ? RouteType::Sprint
                           : d.race.type == RaceType::Cruise ? RouteType::Cruise
                                                             : RouteType::Any;
    if (m_fill) {
        m_map.clear(s);
        m_fill = false;
        menuCleared(s);
        const std::vector<ExeTables::CityLocation>& places = d.tables.cityLocations;
        for (size_t k = 0; k < places.size(); ++k) {
            if (network() != nullptr || locationOpen(d.arcade, d.tables, static_cast<int32_t>(k) + 1, type)) {
                s.addCommandItem("menu", places[k].text, static_cast<float>(k), places[k].name);
            }
        }
        m_shown = -1;
        if (g_setupLocation > -1) s.selectMenuItem("menu", static_cast<size_t>(g_setupLocation));
        if (hosting()) network()->session.setStage(net::SessionStage::Location);
    }
    if (client()) {
        // Stock problem: the place's index among all the city places, though the menu holds only the open ones.
        size_t k = 0;
        const std::vector<ExeTables::CityLocation>& places = d.tables.cityLocations;
        for (size_t i = 0; i < places.size(); ++i) {
            if (_stricmp(places[i].name.c_str(), network()->session.place().c_str()) == 0) k = i;
        }
        if (s.menuSize("menu") != 0) s.selectMenuItem("menu", std::min(k, s.menuSize("menu") - 1), true);
        s.setMenuActive("menu", false);
    }
    if (s.menuSize("menu") == 0) return;
    const size_t item = s.selectedMenuItem("menu");
    const float value = s.menuItemValue("menu", item);
    if (static_cast<float>(m_shown) != value) {
        m_shown = x87::nearest(value);
        g_setupLocation = static_cast<int32_t>(item);
        if (m_shown >= 0) m_map.look(host, s, static_cast<size_t>(m_shown), -0.4f);
        d.race.track = s.menuItemCommand("menu", item).substr(0, 31);
        if (hosting()) network()->session.sendPlace(d.race.track);
    }
    s.hideContaining("LOCKED", true);
}

// 0x0045C6C0, the route page (+0x13C 1). Filling: the menu emptied; the routes of the track's .tdf in order
// (0x00531890) but the cruises and the sprints (by name), each open one (0x0042B810) an item valued its place in the
// list: a reversed point-to-point "POINT-TO-POINT Reversed", a reversed route "ROUTE <n> Reversed" (n counting the
// reversed ones from 1), a point-to-point "POINT-TO-POINT" (none in a solo race), another "ROUTE <its place + 1>"; the
// item of the race's route highlighted (Stock problem: by its place among all the routes, though the routes left out
// have no item); +0x18 -1; HELP "select route" (59). Each frame a highlighted item other than +0x18: +0x18 its place,
// its name less ".wld" on the map (0x0045B720) and as the race's route (31 characters); LOCKED hidden.
void InstantSetup::showRoutes(CustomRaceHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    GameData& d = *data;
    m_routePage = true;
    const auto has = [](const std::string& name, std::initializer_list<const char*> parts) {
        for (const char* p : parts) {
            if (name.find(p) != std::string::npos) return true;
        }
        return false;
    };
    if (m_fill) {
        m_fill = false;
        menuCleared(s);
        int32_t highlight = -1;
        const std::vector<std::string> routes = trackRoutes(d.race.track);
        int32_t reversed = 1;
        for (size_t i = 0; i < routes.size(); ++i) {
            const std::string& name = routes[i];
            if (has(name, {"cruise", "CRUISE", "SPRINT", "sprint"})) continue;
            bool pointToPoint = false;
            std::wstring text;
            if (has(name, {"_P2P_REV", "_p2p_rev", "_P2P01_REV", "_P2P01_rev", "_P2P_rev"})) {
                text = texts.at(kTextPointToPointUpper) + L" " + texts.at(kTextReversed);
                pointToPoint = true;
            } else if (has(name, {"_rev", "_REV"})) {
                text = texts.at(kTextRoute) + toWide(std::format(" {} ", reversed)) +
                       texts.at(kTextReversed);
                ++reversed;
            } else if (has(name, {"_P2P", "_p2p"})) {
                text = texts.at(kTextPointToPointUpper);
                pointToPoint = true;
            } else {
                text = texts.at(kTextRoute) + toWide(std::format(" {} ", i + 1));
            }
            if ((network() != nullptr || !routeLocked(d.arcade, d.tables, name)) &&
                !(pointToPoint && d.race.type == RaceType::Solo)) {
                s.addCommandItem("menu", text, static_cast<float>(i), name);
            }
            std::string bare = name;
            if (const size_t at = bare.find(".wld"); at != std::string::npos) bare.resize(at);
            if (_stricmp(bare.c_str(), d.race.route.c_str()) == 0) highlight = static_cast<int32_t>(i);
        }
        m_shown = -1;
        if (highlight > -1) s.selectMenuItem("menu", static_cast<size_t>(highlight));
        if (hosting()) network()->session.setStage(net::SessionStage::Route);
    }
    if (s.menuSize("menu") == 0) return;
    const size_t item = s.selectedMenuItem("menu");
    if (m_shown != static_cast<int32_t>(item)) {
        m_shown = static_cast<int32_t>(item);
        std::string name = s.menuItemCommand("menu", item);
        if (const size_t at = name.find(".wld"); at != std::string::npos) name.resize(at);
        m_map.route(host, s, name, true);
        d.race.route = name.substr(0, 31);
        if (hosting()) network()->session.sendRoute(name);
    }
    s.hideContaining("LOCKED", true);
    if (client()) {
        // The first item whose name holds the host's route (Stock problem: as a part of the name).
        size_t k = 0;
        for (size_t i = 0; i < s.menuSize("menu"); ++i) {
            if (s.menuItemCommand("menu", i).find(network()->session.route()) != std::string::npos) {
                k = i;
                break;
            }
        }
        s.selectMenuItem("menu", k, true);
        s.setMenuActive("menu", false);
    }
}

// 0x0045CD10, the laps: items "1 LAP", "2 LAPS" .. "6 LAPS" (1940, 175) valued 1 .. 6; the settings' laps less 1
// highlighted (0 past the list); +0x18 -1; the host's stage 5. Each frame another highlighted item (enabled): the
// settings' laps and the race's its place + 1, the host sends them; a client highlights the session's laps less 1
// (gliding; Stock problem: its own settings take them), its menu taking no events.
void InstantSetup::showLaps(CustomRaceHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    save::Settings& set = host.optionSettings();
    if (m_fill) {
        m_fill = false;
        menuCleared(s);
        for (int32_t n = 1; n < 7; ++n) {
            s.addMenuItem("menu", toWide(std::format("{} ", n)) + texts.at(n == 1 ? kTextLap : kTextLapsUpper),
                          static_cast<float>(n));
        }
        m_shown = -1;
        uint32_t index = set.u32(save::Settings::kCustomLaps) - 1;
        if (index > s.menuSize("menu") - 1) index = 0;
        s.selectMenuItem("menu", index);
        if (hosting()) network()->session.setStage(net::SessionStage::Laps);
    }
    if (s.menuSize("menu") == 0) return;
    const size_t item = s.selectedMenuItem("menu");
    if (m_shown != static_cast<int32_t>(item) && s.menuItemEnabled("menu", item)) {
        m_shown = static_cast<int32_t>(item);
        set.setU32(save::Settings::kCustomLaps, static_cast<uint32_t>(item) + 1);
        data->race.laps = static_cast<int32_t>(item) + 1;
        if (hosting()) network()->session.sendLaps(static_cast<int32_t>(item) + 1);
    }
    if (client()) {
        const int32_t laps = network()->session.laps();
        s.selectMenuItem("menu", laps > 0 ? static_cast<size_t>(laps - 1) : 0, true);
        s.setMenuActive("menu", false);
    }
}

// 0x0045D000, the opponents: at most 6 cars (4 in a sprint, at night or in the wet) less the players; items "<n + 1> "
// "CARS" (65) valued n from 1 (0 with more players); highlighted the settings' opponents less 1, or the most less 1 when the
// settings ask for more (0 past the list); +0x18 -1. Each frame another highlighted item: the settings' and the race's
// opponents its value (to the nearest).
void InstantSetup::showOpponents(CustomRaceHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    save::Settings& set = host.optionSettings();
    RaceSetup& race = data->race;
    if (m_fill) {
        m_fill = false;
        menuCleared(s);
        int32_t most = race.type != RaceType::Sprint ? 6 : 4;
        if (race.timeOfDay == 4 || race.weather == 2) most = 4;
        most -= race.players;
        for (int32_t n = static_cast<uint32_t>(race.players) < 2 ? 1 : 0; n <= most; ++n) {
            s.addMenuItem("menu", toWide(std::format("{} ", n + 1)) + texts.at(kTextCars), static_cast<float>(n));
        }
        m_shown = -1;
        const auto saved = static_cast<int32_t>(set.u32(save::Settings::kCustomOpponents));
        const uint32_t last = static_cast<uint32_t>(s.menuSize("menu")) - 1;
        uint32_t index = static_cast<uint32_t>(most < saved ? most - 1 : saved - 1);
        if (last < index) index = 0;
        s.selectMenuItem("menu", index);
    }
    if (s.menuSize("menu") == 0) return;
    const size_t item = s.selectedMenuItem("menu");
    if (m_shown != static_cast<int32_t>(item)) {
        m_shown = static_cast<int32_t>(item);
        const int32_t n = x87::chop(s.menuItemValue("menu", item));
        set.setU32(save::Settings::kCustomOpponents, static_cast<uint32_t>(n));
        race.opponents = n;
    }
}

// 0x0045D2E0, the time limit (the settings' and the race's difficulty; no page leads to it): items "1 MINUTE",
// "3 MINUTES", "5 MINUTES" (1941, 66) valued 1, 3, 5; highlighted the first for the settings' 1.0, the third for 5.0,
// else the second; +0x18 -1; the host's stage 3. Each frame another highlighted item: the settings' and the race's
// difficulty its value, the host sends it (to the nearest); a client highlights by the session's (3 the second, 5 the
// third, else the first), its menu taking no events.
void InstantSetup::showDifficulty(CustomRaceHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    save::Settings& set = host.optionSettings();
    if (m_fill) {
        m_fill = false;
        menuCleared(s);
        for (int32_t n = 1; n < 4; ++n) {
            const int32_t minutes = n + (n - 1);
            s.addMenuItem("menu", toWide(std::format("{} ", minutes)) + texts.at(n == 1 ? kTextMinute : kTextMinutes),
                          static_cast<float>(minutes));
        }
        m_shown = -1;
        const float now = std::bit_cast<float>(set.u32(save::Settings::kCustomDifficulty));
        s.selectMenuItem("menu", now == 1.0f ? 0 : now == 5.0f ? 2 : 1);
        if (hosting()) network()->session.setStage(net::SessionStage::Difficulty);
    }
    if (s.menuSize("menu") == 0) return;
    const size_t item = s.selectedMenuItem("menu");
    if (m_shown != static_cast<int32_t>(item)) {
        m_shown = static_cast<int32_t>(item);
        const float value = s.menuItemValue("menu", item);
        set.setU32(save::Settings::kCustomDifficulty, std::bit_cast<uint32_t>(value));
        data->race.difficulty = value;
        if (hosting()) network()->session.sendDifficulty(x87::nearest(value));
    }
    if (client()) {
        const int32_t difficulty = network()->session.difficulty();
        s.selectMenuItem("menu", difficulty == 3 ? 1 : difficulty == 5 ? 2 : 0, true);
        s.setMenuActive("menu", false);
    }
}

// 0x0045D5C0, the time of day: MORNING, AFTERNOON, EVENING, NIGHT (67 ..) valued 1 .. 4; the settings' +0xDC less 1
// highlighted (0 past the list); +0x18 -1. Each frame another highlighted item: the race's time of day and the
// settings' its place + 1.
void InstantSetup::showTimeOfDay(CustomRaceHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    save::Settings& set = host.optionSettings();
    if (m_fill) {
        m_fill = false;
        menuCleared(s);
        for (int32_t n = 1; n < 5; ++n) {
            s.addMenuItem("menu", texts.at(kTextTimes + static_cast<size_t>(n - 1)), static_cast<float>(n));
        }
        m_shown = -1;
        uint32_t index = set.u32(save::Settings::kCustomTimeOfDay) - 1;
        if (index > s.menuSize("menu") - 1) index = 0;
        s.selectMenuItem("menu", index);
        if (hosting()) network()->session.setStage(net::SessionStage::TimeOfDay);
    }
    if (s.menuSize("menu") == 0) return;
    const size_t item = s.selectedMenuItem("menu");
    if (m_shown != static_cast<int32_t>(item)) {
        m_shown = static_cast<int32_t>(item);
        const int32_t time = item == 1 ? 2 : item == 2 ? 3 : item == 3 ? 4 : 1;
        data->race.timeOfDay = time;
        set.setU32(save::Settings::kCustomTimeOfDay, static_cast<uint32_t>(time));
        if (hosting()) network()->session.sendTimeOfDay(time);
    }
    if (client()) {
        // An index past the list (-1 too) highlights the first.
        const auto index = static_cast<size_t>(static_cast<uint32_t>(network()->session.timeOfDay() - 1));
        s.selectMenuItem("menu", index < s.menuSize("menu") ? index : 0, true);
        s.setMenuActive("menu", false);
    }
}

// 0x0045D870, the weather: CLEAR and WET (71, 72) valued 1, 2; the settings' +0xE0 less 1 highlighted (0 past the
// list); +0x18 -1. Each frame another highlighted item: the race's weather and the settings' 2 for WET, else 1.
void InstantSetup::showWeather(CustomRaceHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    save::Settings& set = host.optionSettings();
    if (m_fill) {
        m_fill = false;
        menuCleared(s);
        for (int32_t n = 1; n < 3; ++n) {
            s.addMenuItem("menu", texts.at(kTextWeathers + static_cast<size_t>(n - 1)), static_cast<float>(n));
        }
        m_shown = -1;
        uint32_t index = set.u32(save::Settings::kCustomWeather) - 1;
        if (index > s.menuSize("menu") - 1) index = 0;
        s.selectMenuItem("menu", index);
        if (hosting()) network()->session.setStage(net::SessionStage::Weather);
    }
    if (s.menuSize("menu") == 0) return;
    const size_t item = s.selectedMenuItem("menu");
    if (m_shown != static_cast<int32_t>(item)) {
        m_shown = static_cast<int32_t>(item);
        const int32_t weather = item == 1 ? 2 : 1;
        data->race.weather = weather;
        set.setU32(save::Settings::kCustomWeather, static_cast<uint32_t>(weather));
        if (hosting()) network()->session.sendWeather(weather);
    }
    if (client()) {
        const auto index = static_cast<size_t>(static_cast<uint32_t>(network()->session.weather() - 1));
        s.selectMenuItem("menu", index < s.menuSize("menu") ? index : 0, true);
        s.setMenuActive("menu", false);
    }
}

// 0x0045DB00: the first route of the track's .tdf whose name (lower case) holds the kind: the race's route (less
// ".wld", 31 characters) and its place; -1 when none.
int32_t InstantSetup::findRoute(CustomRaceHost& host, const char* kind) {
    GameData* data = host.gameData();
    const std::vector<std::string> routes = trackRoutes(data->race.track);
    for (size_t i = 0; i < routes.size(); ++i) {
        std::string lower = routes[i];
        for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lower.find(kind) == std::string::npos) continue;
        std::string name = routes[i];
        if (const size_t at = name.find(".wld"); at != std::string::npos) name.resize(at);
        data->race.route = name.substr(0, 31);
        if (hosting()) network()->session.sendRoute(data->race.route);
        return static_cast<int32_t>(i);
    }
    return -1;
}

// 0x0045DC60. The tutorial asked "yes" (+0x20): back here after vid_tutorial ([0x0072158C]), +0x20 -1. backmenu
// follows the menu. On the network a client takes the page of the host's stage (2 the location, 3 the time limit,
// 4 the route, 5 the laps, 6 the time of day, 7 the weather), then the shared checks (0x0046E0F0 with the stage of
// the page). The map's frame (0x0045BC40); a client past the route's page runs the location's and the route's once
// (filling) when it has not, then the page, and takes no events.
// SELECT (CLICK) by the race's type and page: a solo race from the location to the route, from the route to the time
// of day (mode 0: the race), to the weather, then the race; a cruise the race at once; a sprint from the location to
// the time of day, the weather, the opponents (one player off line; else none), the race; another race from the
// location to the route (with an item), from the route to the laps (a point-to-point route to the time of day; mode 0
// the race), the time of day, the weather, the opponents, the race (the difficulty's page goes to the route). The
// race: a cruise looks for its "cruise" route, else "No Cruise Route Found"; a sprint for its "sprint" route.
// Stock problem: the sprint's check takes the route found at place 1 for none ("No Sprint Route Found"), not -1; a
// track without a sprint route keeps the route it had. More players: split_selcar; else loading, the race's +0x54 0
// for a sprint, else the automatic box. BACK (CLICK) by the type and page: solo the route to the location, the time to
// the route, the weather to the time, else instant_seltype; a cruise the difficulty to the location, else
// instant_seltype (mode 7); a sprint the location to instant_seltype (mode 7), the time to the location, the weather
// to the time, else to the weather; another race the route to the location, the laps to the route, the opponents to
// the weather, the time to the laps, the weather to the time, else (the location, the difficulty) instant_manu (mode
// 2) with more players, instant_seltype.
void InstantSetup::update(CustomRaceHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    if (data == nullptr) return;
    GameData& d = *data;
    RaceSetup& race = d.race;
    if (m_tutorial == 1) {
        host.shared().tutorialReturn = "instant_setup";
        host.request("vid_tutorial", SwitchMode::Replace);
        m_tutorial = -1;
        return;
    }
    if (s.find("backmenu") >= 0) s.selectMenuItem("backmenu", s.selectedMenuItem("menu"));
    if (network() != nullptr) {
        if (client()) {
            // The page of the host's stage (Location .. Weather).
            static constexpr SetupPage kPageOfStage[6] = {SetupPage::Location,  SetupPage::Difficulty,
                                                          SetupPage::Route,     SetupPage::Laps,
                                                          SetupPage::TimeOfDay, SetupPage::Weather};
            const net::SessionStage stage = network()->session.stage();
            if (stage >= net::SessionStage::Location && stage <= net::SessionStage::Weather) {
                const SetupPage page =
                    kPageOfStage[static_cast<size_t>(stage) - static_cast<size_t>(net::SessionStage::Location)];
                if (m_page != page) {
                    m_page = page;
                    m_fill = true;
                }
            }
        }
        // The stage of the page (the opponents' page the location's).
        static constexpr net::SessionStage kStageOfPage[7] = {
            net::SessionStage::Location, net::SessionStage::Route,      net::SessionStage::Laps,
            net::SessionStage::Location, net::SessionStage::Difficulty, net::SessionStage::TimeOfDay,
            net::SessionStage::Weather};
        const net::SessionStage stage = m_page >= SetupPage::Location && m_page <= SetupPage::Weather
                                            ? kStageOfPage[static_cast<size_t>(m_page)]
                                            : net::SessionStage::Location;
        if (host.netKeys(event, stage, true)) return;
    }
    m_map.frame(host, s);
    if (client() && m_page >= SetupPage::Laps && m_page <= SetupPage::Weather) {
        if (!m_locationPage) {
            const bool fill = m_fill;
            m_fill = true;
            showLocations(host, s);
            m_fill = fill;
        }
        if (!m_routePage) {
            const bool fill = m_fill;
            m_fill = true;
            showRoutes(host, s);
            m_fill = fill;
        }
    }
    switch (m_page) {
    case SetupPage::Location: showLocations(host, s); break;
    case SetupPage::Route: showRoutes(host, s); break;
    case SetupPage::Laps: showLaps(host, s); break;
    case SetupPage::Opponents: showOpponents(host, s); break;
    case SetupPage::Difficulty: showDifficulty(host, s); break;
    case SetupPage::TimeOfDay: showTimeOfDay(host, s); break;
    case SetupPage::Weather: showWeather(host, s); break;
    default: break;
    }
    if (client()) return;
    const auto toPage = [&](SetupPage page) {
        m_page = page;
        m_fill = true;
    };
    const bool item = s.menuSize("menu") != 0;
    if (event == input::kSelect) {
        bool go = false;
        if (race.type == RaceType::Solo) {
            if (m_page == SetupPage::Route) {
                if (race.mode == RaceMode::Arcade) {
                    go = true;
                } else {
                    toPage(SetupPage::TimeOfDay);
                }
            } else if (m_page == SetupPage::TimeOfDay) {
                toPage(SetupPage::Weather);
            } else if (m_page == SetupPage::Weather) {
                go = true;
            } else {
                toPage(SetupPage::Route);
            }
        } else if (race.type == RaceType::Cruise) {
            go = true;
        } else if (race.type == RaceType::Sprint) {
            if (m_page == SetupPage::Opponents) {
                go = true;
            } else if (m_page == SetupPage::TimeOfDay) {
                toPage(SetupPage::Weather);
            } else if (m_page == SetupPage::Weather) {
                if (network() == nullptr && race.players == 1) {
                    toPage(SetupPage::Opponents);
                } else {
                    race.opponents = 0;
                    go = true;
                }
            } else if (race.mode != RaceMode::Arcade) {
                toPage(SetupPage::TimeOfDay);
            } else {
                go = true;
            }
        } else {
            switch (m_page) {
            case SetupPage::Route:
                if (!item) return;
                if (race.mode == RaceMode::Arcade) {
                    go = true;
                } else if (race.route.find("_P2P") != std::string::npos ||
                           race.route.find("_p2p") != std::string::npos) {
                    toPage(SetupPage::TimeOfDay);
                } else {
                    toPage(SetupPage::Laps);
                }
                break;
            case SetupPage::Laps: toPage(SetupPage::TimeOfDay); break;
            case SetupPage::Opponents: go = true; break;
            case SetupPage::TimeOfDay: toPage(SetupPage::Weather); break;
            case SetupPage::Weather:
                if (network() == nullptr && race.players == 1) {
                    toPage(SetupPage::Opponents);
                } else {
                    race.opponents = 0;
                    go = true;
                }
                break;
            default:
                if (!item) return;
                toPage(SetupPage::Route);
                break;
            }
        }
        host.sound("CLICK");
        if (!go) return;
        if (race.type == RaceType::Cruise) {
            if (findRoute(host, "cruise") <= -1) {
                host.message(MessageType::Continue, L"No Cruise Route Found", false, nullptr);
                return;
            }
        } else if (race.type == RaceType::Sprint) {
            if (findRoute(host, "sprint") == 1) {
                host.message(MessageType::Continue, L"No Sprint Route Found", false, nullptr);
                return;
            }
        }
        if (static_cast<uint32_t>(race.players) > 1) {
            host.request("split_selcar", SwitchMode::Replace);
            return;
        }
        if (network() != nullptr) {
            // 0x0045E079: the online career: the stage 8 and its race setup (0x004A44E0); else the LAN's cars.
            if (host.onlineCareer()) {
                network()->session.setStage(net::SessionStage::Cars);
                host.onlineCareerRaceSetup();
                return;
            }
            host.request("link_selcar", SwitchMode::Replace);
            network()->session.setStage(net::SessionStage::Cars);
            return;
        }
        host.request("loading", SwitchMode::Replace);
        race.gearbox =
            race.type == RaceType::Sprint ? 0 : static_cast<int32_t>(host.settingsValue(save::Settings::kAutomatic));
    } else if (event == input::kBack && network() == nullptr) {
        host.sound("CLICK");
        const SetupPage page = m_page;
        if (race.type == RaceType::Solo) {
            if (page == SetupPage::Route) {
                toPage(SetupPage::Location);
            } else if (page == SetupPage::TimeOfDay) {
                toPage(SetupPage::Route);
            } else if (page == SetupPage::Weather) {
                toPage(SetupPage::TimeOfDay);
            } else {
                host.request("instant_seltype", SwitchMode::Replace);
                m_fill = true;
            }
        } else if (race.type == RaceType::Cruise) {
            if (page == SetupPage::Difficulty) {
                toPage(SetupPage::Location);
            } else {
                host.request("instant_seltype", SwitchMode::Swap);
                m_fill = true;
            }
        } else if (race.type == RaceType::Sprint) {
            if (page == SetupPage::Location) {
                host.request("instant_seltype", SwitchMode::Swap);
                m_fill = true;
            } else if (page == SetupPage::TimeOfDay) {
                toPage(SetupPage::Location);
            } else if (page == SetupPage::Weather) {
                toPage(SetupPage::TimeOfDay);
            } else {
                toPage(SetupPage::Weather);
            }
        } else {
            switch (page) {
            case SetupPage::Route: toPage(SetupPage::Location); break;
            case SetupPage::Laps: toPage(SetupPage::Route); break;
            case SetupPage::Opponents: toPage(SetupPage::Weather); break;
            case SetupPage::TimeOfDay: toPage(SetupPage::Laps); break;
            case SetupPage::Weather: toPage(SetupPage::TimeOfDay); break;
            default:
                host.request(static_cast<uint32_t>(race.players) > 1 ? "instant_manu" : "instant_seltype",
                             static_cast<uint32_t>(race.players) > 1 ? SwitchMode::ReplaceNow : SwitchMode::Replace);
                m_fill = true;
                break;
            }
        }
    }
}

}  // namespace oj::frontend
