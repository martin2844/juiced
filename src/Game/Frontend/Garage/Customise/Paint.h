#pragma once
// The paint windows: colourmenu, colourchoose, colourfocus and wheelcolourmenu.

#include "Core/Cars/Car.h"
#include "Game/Frontend/Garage/Garage.h"
#include "Game/Frontend/Garage/GarageHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record 0x0071FDB8.
class ColourMenu {
public:
    // 0x0043D270
    void enter(GarageHost& host, ui::Screen& s);
    // 0x0043D390
    void update(GarageHost& host, ui::Screen& s, uint32_t event);

private:
    int32_t m_first = 0;          // +0x0C
    int32_t m_answer = -1;        // +0x14
    int32_t m_askedBuy = 0;       // +0x18
    int32_t m_askedLose = 0;      // +0x1C
    int32_t m_showBuy = 0;        // +0x20
    const Car* m_car = nullptr;   // +0x24
};

// The window of the record 0x0071FE48.
class ColourChoose {
public:
    // 0x0043DB90
    void enter(GarageHost& host, ui::Screen& s);
    // 0x0043DD60
    void update(GarageHost& host, ui::Screen& s, uint32_t event);

private:
    uint32_t m_original = 0;  // +0x10
    int32_t m_answer = -1;    // +0x1C
    int32_t m_asked = 0;      // +0x20
};

// The window of the record 0x0071FE6C.
class ColourFocus {
public:
    // 0x0043E2D0
    void enter(GarageHost& host, ui::Screen& s);
    // 0x0043E410
    void update(GarageHost& host, ui::Screen& s, uint32_t event);

private:
    int32_t m_answer = -1;  // +0x10
    int32_t m_asked = 0;    // +0x14
};

// The window of the record 0x0071FED4.
class WheelColourMenu {
public:
    // 0x0043F3F0
    void enter(GarageHost& host, ui::Screen& s);
    // 0x0043F600
    void update(GarageHost& host, ui::Screen& s, uint32_t event);

private:
    int32_t m_own = 2;  // +0x10: the place of the car's wheel colour
    // The fixed build's: the car's wheel colour (+0x3C, +0x40) as it entered, put back by BACK when it is none of
    // the list (m_own -1).
    uint32_t m_ownColour = 0;
    uint32_t m_ownColoured = 0;
};

}  // namespace oj::frontend
