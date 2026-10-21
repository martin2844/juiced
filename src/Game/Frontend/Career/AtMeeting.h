#pragma once
// CMSAtMeeting: a meeting's events.

#include "Core/Career/Career.h"
#include "Game/Frontend/Career/CareerHost.h"
#include "Game/Frontend/Career/CityMap.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record of CMSAtMeeting (vtable 0x006F4F38).
class AtMeeting {
public:
    // 0x004C01E0
    void enter(CareerHost& host, ui::Screen& s);
    // 0x004C06A0
    void update(CareerHost& host, ui::Screen& s, uint32_t event);
    // 0x004C0650
    void leave();

private:
    // 0x004C0B60: the event's details: every element named extra1, laps, type, prize, sponsored, route, routetxt,
    // lapstitle, typetitle and prizetitle hidden; lapstitle, typetitle and prizetitle shown; Prize the round's first
    // prize (" NA" for none); route "ROUTE:"; routetxt the route: for a circuit or a challenge its number (the route
    // slot + 1, past 3 the slot - 3 and "Reversed"), "Showoff", "Point-to-point" (with "Reversed" past the first slot)
    // or "Sprint".
    void details(CareerHost& host, ui::Screen& s);
    // 0x004C1060: the race's set up: the current event's crew with its +0x2D0 over 0 lowers it by one (not below 0)
    // when rand() / 32767 is above 0.75; a crew of more than one member in a round not a cruise nor a challenge, and
    // not a pink slip, goes to CMSCrewRaceSetup; else the first racer is member 0 (the profile's +0x5F4), the profile's
    // +0x614 is set and CMSRaceSetup follows.
    void toRaceSetup(CareerHost& host);
    // The race entered (0x004C0810 and the menu's ENTER item): the session to be set up again (+0x264), the profile's
    // +0x614 0, +0x618 1 and +0x61C 0, the bets cleared (0x004B1620), the event attended and its fee paid (0x004B7F00).
    void enterRace(CareerHost& host);

    CityMap m_map;                     // +0x0C: the route's map (0x0045B6B0)
    CalendarEvent* m_event = nullptr;  // +0x10
    int32_t m_respectAnswer = 0;       // +0x14: "Leave event and lose respect?"
    int32_t m_leaving = 0;             // +0x18: the respect taken, the event ends
    int32_t m_leaveAnswer = 0;         // +0x1C: "Are you sure you want to leave the race event?"
    int32_t m_feeAnswer = 0;           // +0x20: "Enter race event? Fee:"
};

}  // namespace oj::frontend
