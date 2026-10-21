// The arcade championships' windows of the front end (docs/spec/09-arcade.md, "The windows"): Arcade_Championship,
// Arcade_Race, Arcade_Loading and Arcade_Results.

#include "Game/Frontend/Arcade/ArcadeWindows.h"

#include "Core/Base/Log.h"
#include "Core/Base/Random.h"
#include "Core/Base/Text.h"
#include "Core/Base/X87.h"
#include "Core/Career/Money.h"
#include "Core/Data/GameTexts.h"
#include "Core/Race/Arcade.h"
#include "Game/Frontend/WindowTools.h"
#include "Game/Race/Hud/RaceWindows.h"

#include <cmath>
#include <cstring>
#include <format>

namespace oj::frontend {

namespace {

bool same(const std::string& a, const char* b) {
    return _stricmp(a.c_str(), b) == 0;
}

// An 8-bit text widened character by character (0x0040ACE0).
std::wstring latin(const std::string& text) {
    std::wstring wide;
    for (char c : text) wide += static_cast<wchar_t>(static_cast<unsigned char>(c));
    return wide;
}

// Lines of the game's text table.
constexpr size_t kTextResultTitles[4] = {1899, 1901, 1903, 1905};    // final race, series, next series, arcade
constexpr size_t kTextResultComments[4] = {1900, 1902, 1904, 1906};

// [0x007252FC]: set at start; Arcade_Championship's first visit clears it.
bool g_championshipFirstVisit = true;
// [0x0075ECF4]: the championship Arcade_Championship highlighted last.
int32_t g_championship = 0;
// [0x0075ECF0]: Arcade_Race comes back from vid_tutorial (the race starts at its enter).
bool g_arcadeVideo = false;

}  // namespace

// Arcade_Championship's enter (0x0042DA40): the profile's +0x634 0, the arcade's championship (+0x5120) 0; on the first
// visit ([0x007252FC], cleared) +0x10 1; ImageBox1's picture UI_ARCADE_IMAGE1. CHAMPIONSHIP_MENU emptied, its +0x100 1
// (read by nothing for a menu); for each championship read an item valued its number: a developers' championship (its
// first race's +0x08) "DEVELOPMENT TEAM .." (1943 + the developers' items so far) with them open (+0x5138), an extreme
// nitrous one (+0x04) "EXTREME NITROUS .." (1948 + ..) with them open (+0x5134), another its name (1888 + its number),
// disabled while its first race is locked; the championship of [0x0075ECF4] highlighted (Stock problem: by its number
// as the item's place, though items before it may be left out). +0x0C -1.
void ArcadeChampionship::enter(WindowHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    *this = ArcadeChampionship{};
    if (data == nullptr) return;
    Arcade& a = data->arcade;
    if (data->career) data->career->profile.showoffTutorialShown = 0;
    a.put(Arcade::kChampionshipIndex, 0);
    if (g_championshipFirstVisit) {
        g_championshipFirstVisit = false;
        m_showParity = 1;
    }
    s.setTexture("ImageBox1", std::format("UI_ARCADE_IMAGE{}", a.i32(Arcade::kChampionshipIndex) + 1));
    s.clearMenu("CHAMPIONSHIP_MENU");
    size_t developers = 0;
    size_t extreme = 0;
    for (int32_t c = 0; c < a.count(); ++c) {
        const size_t first = Arcade::race(c, 0);
        if (a.i32(first + Arcade::kRaceDevelopers) == 0) {
            if (a.i32(first + Arcade::kRaceExtreme) == 0) {
                s.addMenuItem("CHAMPIONSHIP_MENU", texts.at(kTextChampionships + static_cast<size_t>(c)),
                              static_cast<float>(c));
                if (a.i32(first + Arcade::kRaceLocked) != 0) {
                    s.setMenuItemEnabledAt("CHAMPIONSHIP_MENU", s.menuSize("CHAMPIONSHIP_MENU") - 1, false);
                }
            } else if (a.i32(Arcade::kExtreme) != 0) {
                s.addMenuItem("CHAMPIONSHIP_MENU", texts.at(kTextExtreme + extreme++), static_cast<float>(c));
            }
        } else if (a.i32(Arcade::kDevelopers) != 0) {
            s.addMenuItem("CHAMPIONSHIP_MENU", texts.at(kTextDevelopers + developers++), static_cast<float>(c));
        }
        if (g_championship == c) {
            const size_t items = s.menuSize("CHAMPIONSHIP_MENU");
            s.selectMenuItem("CHAMPIONSHIP_MENU", items != 0 && static_cast<size_t>(c) > items - 1 ? 0 : c);
        }
    }
    m_shown = -1;
}

// 0x0042DD00: a result waiting (+0x53C4, cleared) asks for the autosave over the window when the save system saves.
// The arcade's championship and [0x0075ECF4] the highlighted item's value (to the nearest). BACK (CLICK) menu; SELECT
// on a disabled item NO, else CLICK and Arcade_Race. A championship other than +0x0C: every Locked, LockedBox and
// Completed hidden; Locked and LockedBox shown while its first race is locked; Completed shown while not: with its
// fifth race not completed "<races completed> / 5 Completed" in white, else "Completed" (338) in green 0xFF32FF32;
// ImageBox1's picture UI_ARCADE_IMAGE<championship + 1>; +0x10 flipped.
void ArcadeChampionship::update(WindowHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr) return;
    Arcade& a = data->arcade;
    if (a.i32(Arcade::kResultWaits) != 0) {
        a.put(Arcade::kResultWaits, 0);
        if (data->saving != 0) host.request("autosave", SwitchMode::Push);
    }
    const bool any = s.menuSize("CHAMPIONSHIP_MENU") != 0;
    const size_t item = any ? s.selectedMenuItem("CHAMPIONSHIP_MENU") : 0;
    if (any) {
        const int32_t c = x87::chop(s.menuItemValue("CHAMPIONSHIP_MENU", item));
        a.put(Arcade::kChampionshipIndex, c);
        g_championship = c;
    }
    if (event == input::kBack) {
        host.sound("CLICK");
        host.request("menu", SwitchMode::Replace);
    } else if (event == input::kSelect && any) {
        if (!s.menuItemEnabled("CHAMPIONSHIP_MENU", item)) {
            host.sound("NO");
        } else {
            host.sound("CLICK");
            host.request("Arcade_Race", SwitchMode::Replace);
        }
    }
    const int32_t c = a.i32(Arcade::kChampionshipIndex);
    if (m_shown == c) return;
    for (const char* name : {"Locked", "LockedBox", "Completed"}) {
        for (int e = 0; e < s.size(); ++e) {
            if (same(s.nameOf(e), name)) s.setHidden(e, true);
        }
    }
    m_shown = c;
    const bool locked = a.i32(Arcade::race(c, 0) + Arcade::kRaceLocked) != 0;
    if (locked) {
        s.show("Locked", false);
        s.show("LockedBox", false);
    }
    if (const int e = s.find("Completed"); e >= 0) {
        s.setHidden(e, locked);
        if (a.i32(Arcade::race(c, 4) + Arcade::kRaceCompleted) == 0) {
            int32_t done = 0;
            for (int32_t r = 0; r < 5; ++r) done += a.i32(Arcade::race(c, r) + Arcade::kRaceCompleted);
            s.setText(e, toWide(std::format("{} / 5 ", done)) + texts.at(kTextCompleted));
            s.setFontColours("Completed", 0xFFFFFFFF, 0xFFFFFFFF);
        } else {
            s.setText(e, texts.at(kTextCompleted));
            s.setFontColours("Completed", 0xFF32FF32, 0xFF32FF32);
        }
    }
    s.setTexture("ImageBox1", std::format("UI_ARCADE_IMAGE{}", c + 1));
    m_showParity = 1 - m_showParity;
}

// Arcade_Loading's enter (0x0042E3F0; its record 0x0071FA6C, which no window of the front end asks for: a race's load
// shows arcade_loading of its own front end): ImageBox2, then ImageBox1, the picture UI_ARCADE_IMAGE<the arcade's
// championship (+0x5120) + 1>.
void ArcadeLoading::enter(WindowHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    if (data == nullptr) return;
    const std::string image = std::format("UI_ARCADE_IMAGE{}", data->arcade.i32(Arcade::kChampionshipIndex) + 1);
    s.setTexture("ImageBox2", image);
    s.setTexture("ImageBox1", image);
}

// Arcade_Race's enter (0x0042C5B0). Back from the showoff tutorial ([0x0075ECF0]) the race starts (0x0042D7D0) and
// +0x20 is 0. Else +0x20 -1; Race_Menu emptied (+0x100 1, read by nothing); Cross shown; for each race of the
// championship an item valued its place: "<n> " and a challenge's kind (1815 + its +0x0C), a race on a route of type 4
// "POINT-TO-POINT" (150), else the race's kind (147 + kind); tick<n> shown for a completed race, else hidden; a locked
// race's item disabled, an open fifth race hides Cross; the first race neither locked nor completed highlighted. +0x0C
// 0. The map (0x0045B6B0, +0x10) on MAP3D: at the first city place, pitch -0.8, each race's route model added hidden
// (0x0045BB40). The car view (+0x14, 0x0045A2C0 on CAR3D, LOGO and ORIGIN (none in the window), the zoom 0.6, the
// mirror image, the car's folder kept and the shadow); FADEBOX (+0x18); every race's car made (0x0042AD40); +0x1C 0.
void ArcadeRace::enter(WindowHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    *this = ArcadeRace{};
    if (data == nullptr) return;
    if (g_arcadeVideo) {
        startRace(host);
        g_arcadeVideo = false;
        m_tutorial = 0;
        return;
    }
    Arcade& a = data->arcade;
    m_tutorial = -1;
    s.clearMenu("Race_Menu");
    s.show("Cross", false);
    const int32_t c = a.i32(Arcade::kChampionshipIndex);
    bool highlighted = false;
    for (int32_t r = 0; r < 5; ++r) {
        const size_t race = Arcade::race(c, r);
        std::wstring text = toWide(std::format("{} ", r + 1));
        const EventKind kind = a.raceKind(race);
        const int32_t route = a.i32(race + Arcade::kRaceRoute);
        const std::vector<ExeTables::Route>& routes = data->tables.routes;
        if (kind == EventKind::Challenge) {
            text += texts.at(kTextChallenges + static_cast<size_t>(a.raceChallenge(race)));
        } else if (route > 0x6C || static_cast<size_t>(route) >= routes.size() ||
                   routes[static_cast<size_t>(route)].type != RouteType::PointToPoint) {
            text += texts.at(kTextRoundTypesUpper + static_cast<size_t>(kind));
        } else {
            text += texts.at(kTextPointToPointUpper);
        }
        s.addMenuItem("Race_Menu", text, static_cast<float>(r));
        const std::string tick = std::format("tick{}", r + 1);
        if (a.i32(race + Arcade::kRaceCompleted) == 0) {
            for (int e = 0; e < s.size(); ++e) {
                if (same(s.nameOf(e), tick.c_str())) s.setHidden(e, true);
            }
        } else {
            s.show(tick, false);
        }
        if (a.i32(race + Arcade::kRaceLocked) == 0) {
            if (r == 4) {
                for (int e = 0; e < s.size(); ++e) {
                    if (same(s.nameOf(e), "Cross")) s.setHidden(e, true);
                }
            }
        } else {
            s.setMenuItemEnabledAt("Race_Menu", static_cast<size_t>(r), false);
        }
        if (a.i32(race + Arcade::kRaceCompleted) == 0 && a.i32(race + Arcade::kRaceLocked) == 0 && !highlighted) {
            highlighted = true;
            s.selectMenuItem("Race_Menu", static_cast<size_t>(r));
        }
    }
    m_shownItem = 0;
    m_map.start(s);
    if (m_map.view3d(s) != nullptr) {
        m_map.look(host, s, 0, std::bit_cast<float>(0xBF4CCCCDu));
        for (int32_t r = 0; r < 5; ++r) {
            const auto route = static_cast<size_t>(a.i32(Arcade::race(c, r) + Arcade::kRaceRoute));
            if (route < data->tables.routes.size()) m_map.addRoute(host, s, data->tables.routes[route].name);
        }
    }
    m_view.reset();
    if (host.engine() != nullptr && s.find("CAR3D") >= 0) {
        scene::CarView::Setup setup;
        setup.element = "CAR3D";
        setup.logo = s.find("LOGO") >= 0 ? "LOGO" : "";
        setup.origin = s.find("ORIGIN") >= 0 ? "ORIGIN" : "";
        setup.zoom = 0.6f;
        setup.mirror = true;
        setup.keepFolder = true;
        setup.shadow = true;
        m_view = std::make_unique<scene::CarView>(*host.engine(), s, setup, data->catalogue, data->tables);
    }
    m_fade = s.find("FADEBOX") >= 0;
    data->makeArcadeCars(gameRandom());
    m_refresh = false;
    g_arcadeVideo = false;
}

// 0x0042CB00: an element Menu's highlighted item kept when it is of the kind 6 (+0x04; the window has none); the map's
// route models taken out and the map freed; the view emptied and freed; the arcade's cars freed (0x00489C30).
void ArcadeRace::leave(WindowHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    m_map.clear(s);
    m_map.release();
    if (m_view) m_view->clear();
    m_view.reset();
    if (data != nullptr) data->arcadeRaceCars.clear();
}

// 0x0042CE80: the highlighted race's details: LAP its laps for a circuit (kind 4) not on a route of type 4, else "NA"
// (432); TIME by its time of day (2 AFTERNOON, 3 EVENING, 4 NIGHT, else MORNING); WEATHER CLEAR for 1, else WET;
// OPPONENTS its cars less 1; WORLD_NAME its route's location (575 + location); ROUTE by the route's number in its
// track (+0x8C): 4 .. 7 "ROUTE <n - 3> Reversed", 8 Showoff, 9 Sprint, 10 Point-to-point, 11 Point-to-point (r), else
// "ROUTE <n + 1>", shown; CAR the race's car's display name and " (<rating> bhp)" (cut toward zero), its +0x128 1;
// CHALLENGE the championship's name (a developers' 1943 + c - 11, an extreme nitrous 1948 + c - 14, else 1888 + c),
// its +0x128 1. A challenge (kind 0) hides OPPONENTS and OPPONENTSTEXT and shows TARGET and TARGETVAL: a speed "<value>
// MPH" (or x 1.6093 "KPH" by the settings' units, to the nearest), a showoff its points with the thousands marked, a
// lap time as the HUD's times; another race hides TARGET and TARGETVAL and shows OPPONENTS and OPPONENTSTEXT. The car
// and the map then (+0x1C 1).
void ArcadeRace::details(WindowHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Arcade& a = data->arcade;
    const size_t race = a.current();
    const std::vector<ExeTables::Route>& routes = data->tables.routes;
    const int32_t route = a.i32(race + Arcade::kRaceRoute);
    const ExeTables::Route* r = route >= 0 && static_cast<size_t>(route) < routes.size() && route < 0x6D
                                    ? &routes[static_cast<size_t>(route)]
                                    : nullptr;
    const RouteType routeType = r != nullptr ? r->type : RouteType::Any;
    const EventKind kind = a.raceKind(race);
    if (kind == EventKind::Circuit && routeType != RouteType::PointToPoint) {
        s.setText("Lap", toWide(std::format("{}", a.i32(race + Arcade::kRaceLaps))));
    } else {
        s.setText("Lap", texts.at(kTextNotAvailable));
    }
    const int32_t time = a.i32(race + Arcade::kRaceTimeOfDay);
    const size_t timeLine = time == 2 ? 1 : time == 3 ? 2 : time == 4 ? 3 : 0;
    s.setText("Time", texts.at(kTextTimes + timeLine));
    s.setText("Weather", texts.at(kTextWeathers + (a.i32(race + Arcade::kRaceWeather) == 1 ? 0 : 1)));
    s.setText("Opponents", toWide(std::format("{}", a.i32(race + Arcade::kRaceCarCount) - 1)));
    if (r != nullptr) s.setText("World_Name", texts.at(kTextLocations + static_cast<size_t>(r->location)));
    const int32_t number = r != nullptr ? r->index : 0;
    std::wstring routeText;
    switch (number) {
    case 4:
    case 5:
    case 6:
    case 7:
        routeText = texts.at(kTextRoute) + toWide(std::format("{} ", number - 3)) + texts.at(kTextReversed);
        break;
    case 8:
    case 9:
    case 10:
        routeText = texts.at(kTextShowoff + static_cast<size_t>(number - 8));
        break;
    case 11:
        routeText = texts.at(kTextPointToPointReversed);
        break;
    default:
        routeText = texts.at(kTextRoute) + L" " + toWide(std::format("{}", number + 1));
        break;
    }
    s.setText("Route", routeText);
    s.setHidden("Route", false);
    const size_t place = static_cast<size_t>(a.i32(Arcade::kChampionshipIndex) * 5 + a.i32(Arcade::kRaceIndex));
    if (place < data->arcadeRaceCars.size()) {
        const Car& car = data->arcadeRaceCars[place];
        std::wstring text = car.def != nullptr ? latin(car.def->displayName) : std::wstring();
        text += toWide(std::format(" ({}", x87::chop(std::bit_cast<float>(car.rating)))) + L" " +
                texts.at(kTextBhp) + L")";
        s.setText("Car", text);
    }
    s.setMultiline("Car", true);
    const int32_t c = a.i32(Arcade::kChampionshipIndex);
    const size_t first = Arcade::race(c, 0);
    size_t line = kTextChampionships + static_cast<size_t>(c);
    if (a.i32(first + Arcade::kRaceDevelopers) != 0) {
        line = kTextDevelopers + static_cast<size_t>(c - 11);
    } else if (a.i32(first + Arcade::kRaceExtreme) != 0) {
        line = kTextExtreme + static_cast<size_t>(c - 14);
    }
    s.setText("Challenge", texts.at(line));
    s.setMultiline("Challenge", true);
    const auto hideAll = [&](const char* name) {
        for (int e = 0; e < s.size(); ++e) {
            if (same(s.nameOf(e), name)) s.setHidden(e, true);
        }
    };
    if (kind == EventKind::Challenge) {
        hideAll("Opponents");
        hideAll("OpponentsText");
        s.show("Target", false);
        s.show("TargetVal", false);
        const ChallengeType challenge = a.raceChallenge(race);
        const float value = a.f32(race + Arcade::kRaceChallengeValue);
        if (challenge == ChallengeType::Speed) {
            if (host.settingsValue(save::Settings::kMph) == 0) {
                s.setText("TargetVal", toWide(std::format("{} ", x87::chop(value * 1.6093f))) +
                                           texts.at(kTextKph));
            } else {
                s.setText("TargetVal", toWide(std::format("{} ", x87::chop(value))) + texts.at(kTextMph));
            }
        } else if (challenge == ChallengeType::Showoff) {
            s.setText("TargetVal", toWide(groupThousands(x87::chop(value), host.language().index)));
        } else {
            s.setText("TargetVal", toWide(hudTime(value)));
        }
    } else {
        hideAll("Target");
        hideAll("TargetVal");
        s.show("Opponents", false);
        s.show("OpponentsText", false);
    }
    m_refresh = true;
}

// 0x0042D6F0, with +0x1C and the view not loading: the map at the city place named as the race's route's location
// (0x0045BEC0, 0x004627D0: the place whose name of the nine at 0x00767DC0 is the location's text, line 575 +
// location, exactly; the first when none), the route's model shown and coming down (0x0045B720 with 0), the race's
// car shown turned 2.64, turning 0.4, fading (0x0045A080 on the arcade's cars, its place c x 5 + r); +0x1C 0.
void ArcadeRace::show(WindowHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (!m_refresh || (m_view && m_view->busy())) return;
    Arcade& a = data->arcade;
    const size_t race = a.current();
    const auto route = static_cast<size_t>(a.i32(race + Arcade::kRaceRoute));
    const std::vector<ExeTables::Route>& routes = data->tables.routes;
    if (route < routes.size()) {
        // Stock problem: the location's text of the language against nine English names, exactly; Super Speedway is
        // past the eight city places.
        m_map.lookAt(host, s, texts.at(kTextLocations + static_cast<size_t>(routes[route].location)));
        m_map.route(host, s, routes[route].name, false);
    }
    const size_t place = static_cast<size_t>(a.i32(Arcade::kChampionshipIndex) * 5 + a.i32(Arcade::kRaceIndex));
    if (m_view && place < data->arcadeRaceCars.size()) {
        m_view->show(data->arcadeRaceCars[place], nullptr, static_cast<uint32_t>(place),
                     std::bit_cast<float>(0x4028F5C3u), std::bit_cast<float>(0x3ECCCCCDu), true, host.viewLoading());
    }
    m_refresh = false;
}

// 0x0042D7D0: the race of the arcade's highlighted race: +0x5124, +0x512C and the four news cleared; a challenge
// (kind 0) by its +0x0C: 0 type 0 and 1 type 1 with the lap time (+0x74) its value, 2 type 2 with the speed (+0x78),
// each with +0x7C 1; 3 type 4 and its laps 9999; another kind: 2 type 4 with 9999 laps, 3 type 7, 4 type 5, 5 type 6
// (+0x08 0, +0x0C 3 off line); the laps, the track and route of the route's entry, one player, the time of day and the
// weather, +0x54 0 for a sprint else the automatic box; loading. [[0x00728C40] + 0x60] + 0x10 1; a showoff (kind 0,
// +0x0C 3) also has the race's +0x60 3.0 and the cruise set up for 3 minutes (0x004AA730, its +0x10 1).
void ArcadeRace::startRace(WindowHost& host) {
    GameData* data = host.gameData();
    GameData& d = *data;
    Arcade& a = d.arcade;
    RaceSetup& race = d.race;
    const size_t at = a.current();
    a.put(Arcade::kUnused, 0);
    a.put(Arcade::kOpenedFlag, 0);
    a.put(Arcade::kFinalUnlocked, 0);
    a.put(Arcade::kSeriesComplete, 0);
    a.put(Arcade::kNextSeries, 0);
    a.put(Arcade::kArcadeComplete, 0);
    const EventKind kind = a.raceKind(at);
    if (kind == EventKind::Challenge) {
        switch (a.raceChallenge(at)) {
        case ChallengeType::LapTime:
        case ChallengeType::PerfectLap:
            race.type = static_cast<RaceType>(a.raceChallenge(at));
            race.lapTime = a.f32(at + Arcade::kRaceChallengeValue);
            race.challenge = ChallengeOutcome::Met;
            break;
        case ChallengeType::Speed:
            race.type = RaceType::Speed;
            race.speed = a.f32(at + Arcade::kRaceChallengeValue);
            race.challenge = ChallengeOutcome::Met;
            break;
        case ChallengeType::Showoff:
            race.type = RaceType::Cruise;
            a.put(at + Arcade::kRaceLaps, 9999);
            break;
        default:
            break;
        }
    } else {
        switch (kind) {
        case EventKind::Showoff:
            race.type = RaceType::Cruise;
            a.put(at + Arcade::kRaceLaps, 9999);
            break;
        case EventKind::PointToPoint: race.type = RaceType::PointToPoint; break;
        case EventKind::Circuit: race.type = RaceType::Circuit; break;
        case EventKind::Sprint:
            race.type = RaceType::Sprint;
            race.heat = 0;
            race.heats = 3;
            break;
        default:
            break;
        }
    }
    race.laps = a.i32(at + Arcade::kRaceLaps);
    const auto route = static_cast<size_t>(a.i32(at + Arcade::kRaceRoute));
    if (route < d.tables.routes.size()) {
        race.track = d.tables.routes[route].track.substr(0, 31);
        race.route = d.tables.routes[route].name.substr(0, 31);
    }
    race.players = 1;
    race.timeOfDay = a.i32(at + Arcade::kRaceTimeOfDay);
    race.weather = a.i32(at + Arcade::kRaceWeather);
    race.gearbox =
        race.type == RaceType::Sprint ? 0 : static_cast<int32_t>(host.settingsValue(save::Settings::kAutomatic));
    host.request("loading", SwitchMode::Replace);
    if (kind == EventKind::Challenge && a.raceChallenge(at) == ChallengeType::Showoff) {
        race.difficulty = 3.0f;
        if (data->career) {
            data->career->cruise.setup(3, race.mode, race.type);
            data->career->cruise.started = 1;
        }
    }
}

// 0x0042CB90: a result waiting asks for the autosave as Arcade_Championship does. The showoff tutorial's "yes"
// (+0x20 1): vid_tutorial coming back here ([0x0072158C] "arcade_race", [0x0075ECF0] 1); its "no" (0) starts the race.
// The view's frame, FADEBOX's alpha 1 - the view's +0x4C, the map's frame, the car and the map (0x0042D6F0); a new
// highlighted item: the arcade's race its value (to the nearest) and its details (0x0042CE80). BACK (CLICK)
// Arcade_Championship; SELECT on a locked race NO, else CLICK and, for a showoff the profile's +0x634 has not marked,
// "VIEW SHOWOFF TUTORIAL?" (1911, type 3, into +0x20) and the mark, else the race (0x0042D7D0).
void ArcadeRace::update(WindowHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr) return;
    Arcade& a = data->arcade;
    if (a.i32(Arcade::kResultWaits) != 0) {
        a.put(Arcade::kResultWaits, 0);
        if (data->saving != 0) host.request("autosave", SwitchMode::Push);
    }
    if (m_tutorial == 1) {
        host.shared().tutorialReturn = "arcade_race";
        host.request("vid_tutorial", SwitchMode::Replace);
        g_arcadeVideo = true;
        return;
    }
    if (m_tutorial == 0) {
        startRace(host);
        return;
    }
    if (m_view) {
        m_view->frame(host.frameTime(), gameRandom());
        if (m_fade) {
            if (const int e = s.find("FADEBOX"); e >= 0) s.setAlpha(e, 1.0f - m_view->shown());
        }
    }
    m_map.frame(host, s);
    show(host, s);
    if (s.menuSize("Race_Menu") != 0) {
        const size_t item = s.selectedMenuItem("Race_Menu");
        if (m_shownItem != static_cast<int32_t>(item) + 1) {
            m_shownItem = static_cast<int32_t>(item) + 1;
            a.put(Arcade::kRaceIndex, x87::chop(s.menuItemValue("Race_Menu", item)));
            details(host, s);
        }
    }
    if (event == input::kBack) {
        host.sound("CLICK");
        host.request("Arcade_Championship", SwitchMode::Replace);
        return;
    }
    if (event != input::kSelect) return;
    const size_t at = a.current();
    if (a.i32(at + Arcade::kRaceLocked) != 0) {
        host.sound("NO");
        return;
    }
    host.sound("CLICK");
    int32_t asked = 1;
    if (data->career) asked = data->career->profile.showoffTutorialShown;
    if (a.raceKind(at) == EventKind::Challenge && asked == 0 && a.raceChallenge(at) == ChallengeType::Showoff) {
        m_tutorial = -1;
        host.message(MessageType::YesNo, texts.at(kTextShowoffTutorial), false, &m_tutorial, true);
        data->career->profile.showoffTutorialShown = 1;
        return;
    }
    startRace(host);
}

// Arcade_Results' enter (0x0042E150): Title2 the last of "UNLOCKED FINAL RACE" (1899), "SERIES COMPLETE" (1901), "NEW
// SERIES UNLOCKED" (1903), "ARCADE COMPLETE!" (1905) by the news set (in the order series, next series, final race,
// arcade); Comment over several lines (+0x128 1, 0x0042E273), the comments of the news in that order for a race of
// neither the developers nor the extreme nitrous, else emptied.
void ArcadeResults::enter(WindowHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr) return;
    const Arcade& a = data->arcade;
    const size_t news[4] = {Arcade::kSeriesComplete, Arcade::kNextSeries, Arcade::kFinalUnlocked,
                            Arcade::kArcadeComplete};
    const size_t order[4] = {1, 2, 0, 3};
    for (size_t k = 0; k < 4; ++k) {
        if (a.i32(news[k]) != 0) s.setText("Title2", texts.at(kTextResultTitles[order[k]]));
    }
    s.setMultiline("Comment", true);
    const size_t race = a.current();
    if (a.i32(race + Arcade::kRaceExtreme) == 0 && a.i32(race + Arcade::kRaceDevelopers) == 0) {
        for (size_t k = 0; k < 4; ++k) {
            if (a.i32(news[k]) != 0) s.setText("Comment", texts.at(kTextResultComments[order[k]]));
        }
    } else if (const int e = s.find("Comment"); e >= 0) {
        s.clearText(e);
    }
}

// 0x0042E330: SELECT or BACK (CLICK): Arcade_Championship after a series completed (+0x53CC), else Arcade_Race.
void ArcadeResults::update(WindowHost& host, uint32_t event) {
    GameData* data = host.gameData();
    if (data == nullptr || (event != input::kSelect && event != input::kBack)) return;
    host.sound("CLICK");
    host.request(data->arcade.i32(Arcade::kSeriesComplete) != 0 ? "Arcade_Championship" : "Arcade_Race",
                 SwitchMode::Replace);
}

}  // namespace oj::frontend
