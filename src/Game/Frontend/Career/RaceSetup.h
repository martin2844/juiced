#pragma once
// CMSRaceSetup: the cars and drivers of a race.

#include "Game/Cars/CarView.h"
#include "Game/Frontend/Career/CareerHost.h"
#include "Game/UI/UiScreen.h"

#include <array>
#include <cstdint>
#include <memory>

namespace oj::frontend {

// The window of the record 0x00722640 (vtable 0x006F5DBC).
class RaceSetupWindow {
public:
    // 0x004D69F0
    void enter(CareerHost& host, ui::Screen& s);
    // 0x004D7840
    void update(CareerHost& host, ui::Screen& s, uint32_t event);
    // 0x004D77E0
    void leave();

private:
    // 0x004D7470: carmenu emptied (its +0x100 set) and an item for each car listed for the meeting (the profile's
    // +0x514, count +0x5F0), its value the garage index: the model's display name, " (D)" when damaged, " (P)" with
    // more than three prototype parts (0x004A8F50); then "GO" (value 999) disabled and the cars' items enabled.
    void cars(CareerHost& host, ui::Screen& s);
    // 0x004D8990: selection1 .. 3 hidden; the first free slot of the team shows its selection<n>.
    void markers(CareerHost& host, ui::Screen& s);
    // 0x004D7660: with GO highlighted nothing; else bhp and drive shown, STATS shown, bhp the highlighted car's rating
    // (cut, thousands marked) and drive its drive's line (1830 + 0x0048CE20).
    void stats(CareerHost& host, ui::Screen& s, bool go);
    // 0x004D8410: the car taken into the team: its items disabled; in the first free slot (+0x38, the count +0x44 up)
    // Car<n>icon and Car<n> shown with the model's display name; with every slot taken the last slot's car is given
    // back (its items enabled) and the car takes that slot.
    void take(CareerHost& host, ui::Screen& s, int32_t index);
    // 0x004D8270: the car given back: its slot freed (the count down), Car<n>icon and Car<n> hidden and emptied; its
    // items enabled; the markers again.
    void giveBack(CareerHost& host, ui::Screen& s, int32_t index);
    // 0x004D8810: GO: the profile's race cars (+0x600, count +0x60C) the team's when it is complete; a round the
    // player's crew is not in yet takes it (+0x0D): a race or a sprint with room (crews x team under 6, a sprint 4) one
    // crew more (+0x16) and the player's drivers (+0x18), else the first crew 1 .. 8 racing gives its place.
    void go(CareerHost& host);

    int32_t m_respectAnswer = 0;   // +0x0C: "Leave event and lose respect?"
    int32_t m_leaving = 0;         // +0x10
    int32_t m_leaveAnswer = 0;     // +0x14
    int32_t m_repairAnswer = -1;   // +0x18: "Repair this car for .. ARE YOU SURE?"
    int32_t m_shownCar = -1;       // +0x1C: the car in the view
    int32_t m_tutorialAnswer = -1;  // +0x20: "VIEW SHOWOFF TUTORIAL?"
    std::unique_ptr<scene::CarView> m_view;  // +0x30: the car view
    std::array<int32_t, 3> m_cars{-1, -1, -1};  // +0x38: the team's cars (garage indices)
    int32_t m_chosen = 0;          // +0x44
};

// The first menu item of the value enabled or not (0x005AFA60 and the item's +0x00).
void enableCarItems(ui::Screen& s, float value, bool enabled);

}  // namespace oj::frontend
