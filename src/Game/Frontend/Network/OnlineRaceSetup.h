#pragma once
// CMSOnlineRaceSetup: the online career's race's cars.

#include "Game/Cars/CarView.h"
#include "Game/Frontend/Network/NetworkHost.h"
#include "Game/UI/UiScreen.h"

#include <array>
#include <cstdint>
#include <memory>

namespace oj::frontend {

// The window of the record of CMSOnlineRaceSetup (0x007225E4, vtable 0x006F5D3C).
class OnlineRaceSetup {
public:
    // 0x004D4720
    void enter(NetworkHost& host, ui::Screen& s);
    // 0x004D52D0
    void update(NetworkHost& host, ui::Screen& s, uint32_t event);
    // 0x004D5270
    void leave();

private:
    // 0x004D4E50: CarMenu emptied (its +0x100 set) and an item for each garage car whose class (0x0048E9E0) lies within
    // the session's upper .. lower class and that meets its criteria (1 its territory +8, 2 its make +4, 3 its model
    // +0), its value the garage index: the display name, " (D)" damaged, " (P)" with more than three prototype parts;
    // after each item bhp the car's rating with its thousands marked (0x004A3AF0, so the last car's stays); then "GO"
    // (999); the cars' items enabled.
    void cars(NetworkHost& host, ui::Screen& s);
    // 0x004D5F90: selection1 .. 3 hidden; the first free slot of the team shows its selection<n>.
    void markers(ui::Screen& s);
    // 0x004D5100: the car's stats (999: none): bhp its rating with its thousands marked (0x004A3AF0), drive its drive's
    // text.
    void stats(NetworkHost& host, ui::Screen& s, int32_t index);
    // 0x004D5980: the car's items disabled; in the first free slot (+0x38 up) Car<n> shown with its display name ("
    // (P)" heavily tuned); with every slot taken the last slot's car given back (its items enabled) and the car takes
    // it.
    void take(NetworkHost& host, ui::Screen& s, int32_t index);
    // 0x004D5820: the car given back: its slot -1 (+0x38 down) and Car<n> emptied; its items enabled; the markers.
    void giveBack(ui::Screen& s, int32_t index);
    // 0x004D5ED0: GO: the profile's race cars (+0x600, count +0x60C) the team's when it is complete; careerload and the
    // career's screen (+0xA4) 8.
    void go(NetworkHost& host);

    int32_t m_shown = 0;                      // +0x0C: the garage index shown
    float m_shownValue = -1.0f;               // +0x10: CarMenu's value shown
    std::unique_ptr<scene::CarView> m_view;   // +0x20 (FADEBOX +0x24)
    int32_t m_repairAnswer = -1;              // +0x28: "Repair this car for .. ARE YOU SURE?"
    std::array<int32_t, 3> m_cars{-1, -1, -1};  // +0x2C: the team's cars (garage indices)
    int32_t m_chosen = 0;                     // +0x38
};

}  // namespace oj::frontend
