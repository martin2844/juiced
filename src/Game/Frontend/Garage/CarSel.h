#pragma once
// carsel: choosing a car of the garage.

#include "Game/Frontend/Garage/Garage.h"
#include "Game/Frontend/Garage/GarageHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record 0x0071FC70.
class CarSel {
public:
    // 0x00436380
    void enter(GarageHost& host, ui::Screen& s);
    // 0x004371C0
    void update(GarageHost& host, ui::Screen& s, uint32_t event);
    // 0x00436B90
    void leave(GarageHost& host);
    // 0x00436C60 (on: the front end's music to 0; with the settings' music volume not 0 the game's music, unless
    // playing, plays its next song (0x00408EC0 with 1, 1) and goes to that volume; the record's +0x3C 1) and
    // 0x00436CF0 (off: the game's music to 0, the front end's started again (0x0046BDD0) and to the settings'
    // volume; +0x3C 0). The front end's way back from the network calls the first again (0x0046C420).
    void music(GarageHost& host, bool on);
    // +0x3C: the garage's music on.
    bool musicOn() const { return m_music != 0; }

private:
    // 0x00436D40: the garage's menu hidden for a window over it, or shown again.
    void setMenuHidden(GarageHost& host, ui::Screen& s, bool hidden);
    // 0x00437000: the garage left (after the autosave when the money changed).
    void leaveGarage(GarageHost& host);
    int32_t m_menuHidden = 0;     // +0x18: the menu hidden for a window over it
    int32_t m_music = 0;          // +0x3C: the garage's music on
    int32_t m_repairShown = 0;    // +0x1C: the repair's price shown
    int32_t m_leave = 0;          // +0x20: leaving after the autosave
    int32_t m_repairAnswer = -1;  // +0x24
    int32_t m_rollingAnswer = -1; // +0x28: read (0x004379AA), no question answers into it; -1 by the enter (0x00436A04)
    int32_t m_sellAnswer = -1;    // +0x2C
    int32_t m_unused = -1;        // +0x30: -1 by the enter (0x00436A0A), not read by the window
    int32_t m_repairAsked = 0;    // +0x34
};

}  // namespace oj::frontend
