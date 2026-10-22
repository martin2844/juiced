#pragma once
// The custom race's window instant_setup (docs/spec/02-frontend.md, "The custom race").

#include "Game/Frontend/Career/CityMap.h"
#include "Game/Frontend/CustomRace/CustomRaceHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The custom race window's page (its +0x28): what its menu chooses.
enum class SetupPage : int32_t {
    Location = 0,
    Route = 1,
    Laps = 2,
    Opponents = 3,
    Difficulty = 4,
    TimeOfDay = 5,
    Weather = 6,
};

// The window of the record of instant_setup (0x00720AD0, vtable 0x006F19B0).
class InstantSetup {
public:
    // 0x0045BFE0
    void enter(CustomRaceHost& host, ui::Screen& s);
    // 0x0045C420
    void leave();
    // 0x0045DC60
    void update(CustomRaceHost& host, ui::Screen& s, uint32_t event);

private:
    // The pages' common start: the menu shown and emptied, its transition started again.
    void menuCleared(ui::Screen& s);
    // 0x0045C450, the location page (+0x138 1).
    void showLocations(CustomRaceHost& host, ui::Screen& s);
    // 0x0045C6C0, the route page (+0x13C 1).
    void showRoutes(CustomRaceHost& host, ui::Screen& s);
    // 0x0045CD10, the laps: items "1 LAP", "2 LAPS" .. "6 LAPS" (1940, 175) valued 1 .. 6; the settings' laps less 1
    // highlighted (0 past the list); +0x18 -1; the host's stage 5.
    void showLaps(CustomRaceHost& host, ui::Screen& s);
    // 0x0045D000, the opponents: at most 6 cars (4 in a sprint, at night or in the wet) less the players; items "<n +
    // 1> " "CARS" (65) valued n from 1 (0 with more players); highlighted the settings' opponents less 1, or the most
    // less 1 when the settings ask for more (0 past the list); +0x18 -1.
    void showOpponents(CustomRaceHost& host, ui::Screen& s);
    // 0x0045D2E0, the time limit (the settings' and the race's difficulty; no page leads to it): items "1 MINUTE", "3
    // MINUTES", "5 MINUTES" (1941, 66) valued 1, 3, 5; highlighted the first for the settings' 1.0, the third for 5.0,
    // else the second; +0x18 -1; the host's stage 3.
    void showDifficulty(CustomRaceHost& host, ui::Screen& s);
    // 0x0045D5C0, the time of day: MORNING, AFTERNOON, EVENING, NIGHT (67 ..) valued 1 .. 4; the settings' +0xDC less 1
    // highlighted (0 past the list); +0x18 -1.
    void showTimeOfDay(CustomRaceHost& host, ui::Screen& s);
    // 0x0045D870, the weather: CLEAR and WET (71, 72) valued 1, 2; the settings' +0xE0 less 1 highlighted (0 past the
    // list); +0x18 -1.
    void showWeather(CustomRaceHost& host, ui::Screen& s);
    // 0x0045DB00: the first route of the track's .tdf whose name (lower case) holds the kind: the race's route (less
    // ".wld", 31 characters) and its place; -1 when none.
    int32_t findRoute(CustomRaceHost& host, const char* kind);

    bool m_fill = true;        // +0x0C: the page's menu to fill
    CityMap m_map;             // +0x10
    int32_t m_shown = -1;      // +0x18: the item the page took
    int32_t m_locked = -1;     // +0x1C
    int m_tutorial = -1;       // +0x20: "VIEW SHOWOFF TUTORIAL?"
    SetupPage m_page = SetupPage::Location;  // +0x28
    bool m_locationPage = false;  // +0x138
    bool m_routePage = false;     // +0x13C
};

}  // namespace oj::frontend
