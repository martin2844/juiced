#pragma once
// CMSCrew: the player's crew.

#include "Game/Frontend/Career/CareerHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record 0x00722094 (vtable 0x006F545C).
class CrewWindow {
public:
    // 0x004C5F60
    void enter(CareerHost& host, ui::Screen& s);
    // 0x004C63D0
    void update(CareerHost& host, ui::Screen& s, uint32_t event);

private:
    // The crewmate of a member place (1 .. 4; the profile's +0x500 + 4 x (place - 1)); -1 past the members.
    int mateOfPlace(CareerHost& host, int place);
    // 0x004C6070: Cash and Date; the buttons 1 .. 4 "-" and crewpic1 .. 4 hidden; for each member the button of its
    // place its crewmate's short name (over several lines) and crewpic<n> shown with its ui_crewmember_<number>; every
    // Selection2, Selection3 and Selection4 hidden.
    void list(CareerHost& host, ui::Screen& s);
    // 0x004C64C0: every element with SELECTION in its name hidden, then selection<n> of the highlighted button shown;
    // Skill1 .. 4Meter and skill1 .. 4 hidden.
    void showStats(CareerHost& host, ui::Screen& s);

    int m_mate = -1;          // +0x0C: the highlighted member's crewmate
    int32_t m_unused = -1;    // +0x10: set by the enter (0x004C5FCE), not read by the window
};

}  // namespace oj::frontend
