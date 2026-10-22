#pragma once
// The accessories: the windows' tint, the neon, the dials' colours, the ICE and the horn.

#include "Game/Frontend/Garage/Garage.h"
#include "Game/Frontend/Garage/GarageHost.h"
#include "Game/UI/UiScreen.h"

#include <array>
#include <cstdint>

namespace oj::frontend {

// The window of the record 0x0071FF24.
class TintsMenu {
public:
    // 0x00440250
    void enter(GarageHost& host, ui::Screen& s);
    // 0x004403B0
    void update(GarageHost& host, ui::Screen& s, uint32_t event);

private:
    uint32_t m_own = 0;  // +0x10: the car's tint (float)
};

// The window of the record 0x0071FE88.
class NeonMenu {
public:
    // 0x0043E690
    void enter(GarageHost& host, ui::Screen& s);
    // 0x0043E9E0
    void update(GarageHost& host, ui::Screen& s, uint32_t event);

private:
    bool m_custom = false;
    uint32_t m_own = 0;  // +0x0C: the car's neon colour
    int32_t m_had = 0;   // +0x10: the car had a neon
};

// The window of the record 0x0071FEA8.
class HudColourMenu {
public:
    // 0x0043ED70
    void enter(GarageHost& host, ui::Screen& s);
    // 0x0043F120
    void update(GarageHost& host, ui::Screen& s, uint32_t event);

private:
    std::array<uint32_t, 2> m_own{};  // +0x10, +0x14: the car's dial colours
};

// The window of the record 0x0071FEE8.
class IceMenu {
public:
    // 0x0043F7F0
    void enter(GarageHost& host, ui::Screen& s);
    // 0x0043FB40
    void update(GarageHost& host, ui::Screen& s, uint32_t event);

private:
    int32_t m_own = -1;    // +0x18: the car's ICE
    int32_t m_shown = -1;  // +0x1C
};

// The window of the record 0x0071FF08.
class HornMenu {
public:
    // 0x0043FEC0
    void enter(GarageHost& host, ui::Screen& s);
    // 0x00440040
    void update(GarageHost& host, ui::Screen& s, uint32_t event);
    // 0x0043FF90: the leave (0x00440320's) stops the horn playing (+0x18, not -1).
    void stopHorn(GarageHost& host);

private:
    uint32_t m_own = 0;              // +0x10: the car's horn (+0xB0)
    int32_t m_shown = -1;            // +0x14
    uint32_t m_handle = 0xFFFFFFFF;  // +0x18: the horn playing (-1 none)
};

}  // namespace oj::frontend
