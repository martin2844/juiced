#pragma once
// The arcade championships' windows (docs/spec/09-arcade.md, "The windows").

#include "Game/Cars/CarView.h"
#include "Game/Frontend/Career/CityMap.h"
#include "Game/Frontend/WindowHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>
#include <memory>

namespace oj::frontend {

// The window of the record 0x0071FA48 (vtable 0x006EF918).
class ArcadeChampionship {
public:
    // 0x0042DA40
    void enter(WindowHost& host, ui::Screen& s);
    // 0x0042DD00
    void update(WindowHost& host, ui::Screen& s, uint32_t event);

private:
    int32_t m_shown = -1;  // +0x0C: the championship shown
    // +0x10: 1 by the enter (0x0042DA99), flipped when another championship shows (0x0042E119), read by nothing.
    int32_t m_showParity = 0;
};

// The window of the record 0x0071FA24 (vtable 0x006EF928).
class ArcadeRace {
public:
    // 0x0042C5B0
    void enter(WindowHost& host, ui::Screen& s);
    // 0x0042CB00
    void leave(WindowHost& host, ui::Screen& s);
    // 0x0042CB90
    void update(WindowHost& host, ui::Screen& s, uint32_t event);

private:
    // 0x0042CE80: the highlighted race's details: LAP its laps for a circuit (kind 4) not on a route of type 4, else
    // "NA" (432); TIME by its time of day (2 AFTERNOON, 3 EVENING, 4 NIGHT, else MORNING); WEATHER CLEAR for 1, else
    // WET; OPPONENTS its cars less 1; WORLD_NAME its route's location (575 + location); ROUTE by the route's number in
    // its track (+0x8C): 4 .. 7 "ROUTE <n - 3> Reversed", 8 Showoff, 9 Sprint, 10 Point-to-point, 11 Point-to-point
    // (r), else "ROUTE <n + 1>", shown; CAR the race's car's display name and " (<rating> bhp)" (cut toward zero), its
    // +0x128 1; CHALLENGE the championship's name (a developers' 1943 + c - 11, an extreme nitrous 1948 + c - 14, else
    // 1888 + c), its +0x128 1.
    void details(WindowHost& host, ui::Screen& s);
    // 0x0042D6F0, with +0x1C and the view not loading: the map at the city place named as the race's route's location
    // (0x0045BEC0, 0x004627D0: the place whose name of the nine at 0x00767DC0 is the location's text, line 575 +
    // location, exactly; the first when none), the route's model shown and coming down (0x0045B720 with 0), the race's
    // car shown turned 2.64, turning 0.4, fading (0x0045A080 on the arcade's cars, its place c x 5 + r); +0x1C 0.
    void show(WindowHost& host, ui::Screen& s);
    // 0x0042D7D0: the race of the arcade's highlighted race: +0x5124, +0x512C and the four news cleared; a challenge
    // (kind 0) by its +0x0C: 0 type 0 and 1 type 1 with the lap time (+0x74) its value, 2 type 2 with the speed
    // (+0x78), each with +0x7C 1; 3 type 4 and its laps 9999; another kind: 2 type 4 with 9999 laps, 3 type 7, 4 type
    // 5, 5 type 6 (+0x08 0, +0x0C 3 off line); the laps, the track and route of the route's entry, one player, the time
    // of day and the weather, +0x54 0 for a sprint else the automatic box; loading. [[0x00728C40] + 0x60] + 0x10 1; a
    // showoff (kind 0, +0x0C 3) also has the race's +0x60 3.0 and the cruise set up for 3 minutes (0x004AA730, its
    // +0x10 1).
    void startRace(WindowHost& host);

    int32_t m_shownItem = 0;  // +0x0C: the highlighted item (its place + 1 here; 0 none)
    CityMap m_map;            // +0x10
    std::unique_ptr<scene::CarView> m_view;  // +0x14
    bool m_fade = false;      // +0x18: FADEBOX
    bool m_refresh = false;   // +0x1C: the car and the map to show
    int m_tutorial = -1;      // +0x20: "VIEW SHOWOFF TUTORIAL?"
};

// The window of the record 0x0071FA6C; it keeps no state of its own.
class ArcadeLoading {
public:
    // 0x0042E3F0
    void enter(WindowHost& host, ui::Screen& s);
};

// The window keeps no state of its own.
class ArcadeResults {
public:
    // 0x0042E150
    void enter(WindowHost& host, ui::Screen& s);
    // 0x0042E330
    void update(WindowHost& host, uint32_t event);
};

}  // namespace oj::frontend
