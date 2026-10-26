#pragma once
// Hosting a meeting: its setup, its race and its summary.

#include "Game/Frontend/Career/CareerHost.h"
#include "Game/Frontend/Career/CityMap.h"
#include "Game/UI/UiScreen.h"

#include <array>
#include <cstdint>
#include <vector>
#include <optional>

namespace oj::frontend {

// The rounds set up ([0x00728C74 ..], 0x18 bytes each; their count [0x00728C70]).
struct HostRound {
    EventKind type = EventKind::Challenge;
    int32_t route = 0;
    int32_t laps = 0;
    int32_t crew = 0;
    int32_t prize = 0;
    int32_t cost = 0;               // +0x14: set 0 when the round is added (0x004CB7FC), summed by 0x004CC0AC
};

// The hosted meeting's three windows (CMSHostMeetingSetup, CMSHostMeetingSetupRace,
// CMSHostMeetingSummary): their records (the setup's read by the other two) and what they
// share.
class HostMeeting {
public:
    // 0x004CA9A0
    void enterSetup(CareerHost& host, ui::Screen& s);
    // 0x004CAA90
    void leaveSetup();
    // 0x004CAAE0
    void updateSetup(CareerHost& host, ui::Screen& s, uint32_t event);
    // 0x004CB550
    void enterRace(CareerHost& host, ui::Screen& s);
    // 0x004CB690
    void updateRace(CareerHost& host, ui::Screen& s, uint32_t event);
    // 0x004CC3A0
    void enterSummary(CareerHost& host, ui::Screen& s);
    // 0x004CC500
    void updateSummary(CareerHost& host, ui::Screen& s, uint32_t event);
    // 0x004CB640: the race window's map freed.
    void leaveRace() { m_race.map = CityMap{}; }
    // 0x004CC4B0: the summary's map freed.
    void leaveSummary() { m_summary.map = CityMap{}; }
    // The setup's defaults (+0x38) taken when it opens next (the calendar's, the summary's).
    void askDefaults() { m_setup.defaults = 1; }

private:
    // 0x004CD580: an event of the setup (+0x0D the location, +0x10 the type, +0x14 5000, +0x6C the class; a region
    // (+0x60) or a make (+0x64) by the criteria; its prize the rounds' and CMSHostMeetingSetupRace's from 0x00718FD8,
    // not read further) and for crews 1 .. 8 whether each comes (0x004A9480) into +0x40 ..; the crews coming.
    int32_t attendance(CareerHost& host);
    // 0x004CAE60: [0x0075EF70] 0; TypeText the type's line (140 + type); LocationText the location's crew (108 + it)
    // and " (<the track of its first circuit route>)" (575 + the route's location; the map looks at the location by its
    // English name, 0x0045BEC0), white with the crew's fourth step (+0x60) and respect (+0x70) of 1000 or more, else
    // red and reason 1; CriteriaText "None" (164) in 0xFF646432, or the region (188 + it) or the make (193 + it) with
    // the opponents' cars of it, the class and the type (0x004A9820 on crew 1) none: reason 5; with reason 5
    // CriteriaText, CriteriaTypeText (184 + the type) and BHPText red; BHPText the class (604 + it) and " (<low> -
    // <high>" (0x0047FCA0) "bhp)"; CostValue the location's cost (0x00718FB4) into +0x34 and [0x00728C6C]; CashValue
    // the money, red and reason 2 under the cost; Attendance the crews coming (0x004CD580), red under 2.
    void setupTexts(CareerHost& host, ui::Screen& s);
    // 0x004CBA80: [0x0075EF70] 0.
    void raceTexts(CareerHost& host, ui::Screen& s);
    // 0x004CC820: [0x0075EF70] 0.
    void summaryTexts(CareerHost& host, ui::Screen& s);
    // 0x004CD6F0: the hosted event.
    void makeEvent(CareerHost& host);
    std::vector<int> pinkRivals(const Career& career) const;
    void stepPinkRival(const Career& career, int step);
    bool pinkReady(const Career& career) const;

    // The record of CMSHostMeetingSetup (0x007221E0, vtable 0x006F56C0; [0x00719070]).
    struct Setup {
        int32_t answer = 0;        // +0x0C: "Cancel event setup?"
        CityMap map;               // +0x10: the meeting's map (0x0045B6B0)
        EventKind type = EventKind::Challenge;  // +0x14: PointToPoint .. Sprint
        int32_t location = 0;      // +0x18: the crew whose territory (0 .. 8)
        CarCriteria criteriaType = CarCriteria::None;  // +0x1C
        int32_t region = 0;        // +0x20: 1 .. 4
        int32_t make = 0;          // +0x24: 1 .. 17
        int32_t carClass = 0;      // +0x2C: 0 .. 7
        int32_t unused = 0;        // +0x30: set by the enter (0x004CA9D9), not read by the window
        int32_t cost = 0;          // +0x34
        int32_t defaults = 0;      // +0x38: set by the calendar
        std::array<int32_t, 8> attend{};  // +0x40: crews 1 .. 8 come
    };
    // The record of CMSHostMeetingSetupRace (0x00722240, vtable 0x006F56B0; [0x00719074]).
    struct Race {
        int32_t answer = 0;   // +0x0C: "Cancel event setup?"
        EventKind type = EventKind::Challenge;  // +0x10
        int32_t route = 0;    // +0x14
        int32_t laps = 0;     // +0x18: the place in the laps' table 0x00719044
        int32_t crew = 0;     // +0x1C: the team's size 1 .. 3
        int32_t prize = 0;    // +0x20: the place in the prizes' table 0x00718FD8
        CityMap map;          // +0x28: the meeting's map
    };
    // The record of CMSHostMeetingSummary (0x0072226C, vtable 0x006F56D0).
    struct Summary {
        int32_t answer = 0;    // +0x0C: "Cancel event setup?"
        int32_t confirm = 0;   // +0x10: "Create event. Cost: .."
        CityMap map;           // +0x14: the meeting's map
    };

    Setup m_setup;
    Race m_race;
    Summary m_summary;
    int32_t m_reason = 0;  // [0x0075EF70]: why it cannot be hosted (0 it can)
    int32_t m_cost = 0;    // [0x00728C6C]
    int32_t m_rounds = 0;  // [0x00728C70]: the rounds set up (m_roundList's size)
    std::vector<HostRound> m_roundList;
    bool m_pink = false;
    std::optional<std::wstring> m_locationCaption, m_crewCaption, m_prizeCaption;
};

}  // namespace oj::frontend
