#pragma once
// The session windows of the matchmaking (docs/spec/28-network.md, "The session windows").

#include "Core/Network/NetTransport.h"
#include "Core/Network/OnlineSessions.h"
#include "Game/Frontend/Network/NetworkHost.h"
#include "Game/UI/UiScreen.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace oj::frontend {

// online_bg (0x00721820): the window the online windows open over; no state of its own.
class OnlineBg {
public:
    // 0x004739C0
    void enter(WindowHost& host);
};

// online_mode (0x0072184C): quick, search or create; no state of its own.
class OnlineMode {
public:
    void enter();
    // 0x00473A30
    void update(WindowHost& host, ui::Screen& s, uint32_t event);
};

// The window of the record of online_arcade_create (0x00721880, 0x54 bytes, vtable 0x006F2EAC).
class OnlineArcadeCreate {
public:
    // 0x00474840
    void enter(NetworkHost& host, ui::Screen& s);
    // 0x00474A30
    void update(NetworkHost& host, ui::Screen& s, uint32_t event);

private:
    // 0x00474770: ACCESS Public / Private; LEVEL Any or GROUP G .. A, ALL CARS; MODS NONE / STANDARD / PROTOTYPE;
    // COLLISION NO / YES.
    void show(NetworkHost& host, ui::Screen& s);

    net::ArcadeSession m_settings;  // +0x0C .. +0x43
    int32_t m_creating = 0;         // +0x44
    int32_t m_hostStarted = 0;      // +0x48
    int32_t m_carMenuHidden = 1;    // +0x4C: CARMENU hidden
    int32_t m_carMenuLevel = -1;    // +0x50: the level CARMENU lists
};

// The window of the record of online_arcade_search (0x007218D4, 0x34 bytes, vtable 0x006F2E8C).
class OnlineArcadeSearch {
public:
    // 0x00475250
    void enter(NetworkHost& host, ui::Screen& s);
    // 0x00475360
    void update(NetworkHost& host, ui::Screen& s, uint32_t event);

private:
    // 0x00475120: MIN_PLAYERS "%d"; STATUS Any / Waiting / Racing; LEVEL Any or GROUP ..; MODS Any / NONE / STANDARD /
    // PROTOTYPE; COLLISION Any / NO / YES.
    void show(NetworkHost& host, ui::Screen& s);

    net::ArcadeSearch m_settings;  // +0x0C .. +0x1F
    int32_t m_quick = 0;           // +0x20: the quick match searching
    int32_t m_joining = 0;         // +0x24
    float m_timer = 0.0f;          // +0x28
    int32_t m_createAnswer = -1;   // +0x2C: "create one?"
};

// The window of the record of online_career_create (0x00721908, 0x78 bytes, vtable 0x006F2E6C).
class OnlineCareerCreate {
public:
    // 0x00475F20
    void enter(NetworkHost& host, ui::Screen& s);
    // 0x004761F0
    void update(NetworkHost& host, ui::Screen& s, uint32_t event);

private:
    // 0x00475A50: ACCESS; UPPER_CLASS the upper class's most bhp and LOWER_CLASS the lower's least ("%d bhp");
    // TEAM_RACE; CRITERIA_TYPE; CRITERIA (by its type); PINKSLIP and COLLISION NO / YES. Red: TEAM_RACE with fewer crew
    // mates than the team (a team over 100 counts 1), CRITERIA and CRITERIA_TYPE without the team's cars of it
    // (0x004A48D0), the classes without the team's cars in the range (0x004A4830), PINKSLIP set with 55 cars in the
    // garage.
    void show(NetworkHost& host, ui::Screen& s);

    net::CareerSession m_settings;  // +0x0C .. +0x53
    int32_t m_creating = 0;         // +0x54
    int32_t m_hostStarted = 0;      // +0x58
};

// The window of the record of online_career_search (0x00721980, 0x5C bytes, vtable 0x006F2E2C).
class OnlineCareerSearch {
public:
    // 0x00476DD0
    void enter(NetworkHost& host, ui::Screen& s);
    // 0x00476F90
    void update(NetworkHost& host, ui::Screen& s, uint32_t event);

private:
    // 0x004768C0: MIN_PLAYERS "%d"; STATUS Any / Waiting / Racing; TEAM_RACE Any or its text; UPPER_CLASS the upper's
    // most bhp, LOWER_CLASS the lower's least; CRITERIA_TYPE Any (CRITERIA Any with it) or its text, CRITERIA Any or by
    // the type; PINKSLIP and COLLISION Any / NO / YES. Red: TEAM_RACE (not Any) with fewer crew mates; the criteria
    // when neither is Any and the garage lacks them; the classes without the cars; PINKSLIP (any but 0, Any too) with
    // 55 cars.
    void show(NetworkHost& host, ui::Screen& s);

    net::CareerSearch m_settings;  // +0x0C .. +0x2F
    int32_t m_quick = 0;           // +0x30
    int32_t m_joining = 0;         // +0x34
    float m_timer = 0.0f;          // +0x38
    int32_t m_createAnswer = -1;   // +0x3C
};

// The window of the record of online_arcade_results and online_career_results (one, 0x007219DC, 0x20
// bytes, vtable 0x006F2F0C).
class OnlineResults {
public:
    // 0x00478AD0
    void enter(NetworkHost& host, ui::Screen& s);
    // 0x00478B30
    void leave(ui::Screen& s);
    // 0x00478C00
    void update(NetworkHost& host, ui::Screen& s, uint32_t event);

private:
    // 0x00478950: the sorted position whose record's address is the highlighted row's (+0x14); -1 none.
    int32_t position() const;
    // 0x00478A50: the search again (not quick) with the profile's settings: the arcade's (+0x2A8, 0x004E27D0) or the
    // career's (+0x304, 0x004E3300).
    void refresh(WindowHost& host);
    // 0x00477970: the four lists (M_HOST_NAME, M_USED, M_OPEN, M_QUALITY) emptied; a row for each sorted position whose
    // record is valid: the session's name (its item's data the record's address), "%d" of its players (key50) and of
    // its open slots (key51), its quality's '+'s (0x004E6650).
    void fillRows(NetworkHost& host, ui::Screen& s);
    // 0x00478570: the highlighted session's settings (0x00478950; none: the texts emptied, M_STATUS "").
    void details(NetworkHost& host, ui::Screen& s);

    size_t m_menu = 0;                         // +0x04: "Menu"'s highlight kept by the leave
    float m_sortTimer = 0.0f;                  // +0x0C
    float m_listTimer = 0.0f;                  // +0x10
    std::optional<net::Address> m_highlighted; // +0x14: the address of M_HOST_NAME's highlighted row (-1 none)
    int32_t m_requerying = 0;                  // +0x18
    int32_t m_joining = 0;                     // +0x1C
    std::vector<std::optional<net::Address>> m_rows;  // M_HOST_NAME's items' data (item +0x24)
};

// The text of an option and its colours (yellow, red when it cannot be had).
void onlineOption(ui::Screen& s, const std::string& element, const std::wstring& text, bool invalid = false);
// The search windows' wait of a join: true while it waits.
bool onlineJoinWait(NetworkHost& host, int32_t& joining);
// The search windows' quick match: its search, its wait and its question to create one.
void onlineQuickWait(NetworkHost& host, int32_t& quick, int32_t& joining, float& timer, int32_t& answer,
                     bool career);
// The create windows' leave.
void leaveOnlineCreate(WindowHost& host);

}  // namespace oj::frontend
