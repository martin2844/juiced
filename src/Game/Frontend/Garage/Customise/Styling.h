#pragma once
// The styling windows: stylingmenu, the body parts (modmenu), the wheels, the decals and the sponsors' decals.

#include "Core/Cars/Car.h"
#include "Game/Frontend/Garage/Garage.h"
#include "Game/Frontend/Garage/GarageHost.h"
#include "Game/UI/UiScreen.h"

#include <array>
#include <cstdint>

namespace oj::frontend {

// The window of the record 0x0071FD7C.
class StylingMenu {
public:
    // 0x0043B060
    void enter(GarageHost& host, ui::Screen& s);
    // 0x0043B4A0
    void update(GarageHost& host, ui::Screen& s, uint32_t event);

private:
    int32_t m_answer = -1;        // +0x10
    int32_t m_asked = 0;          // +0x14
    const Car* m_car = nullptr;   // +0x18
};

// The window of the record 0x0071FD98: the body parts of a category.
class ModMenu {
public:
    // 0x0043BDE0
    void enter(GarageHost& host, ui::Screen& s);
    // 0x0043C1B0
    void update(GarageHost& host, ui::Screen& s, uint32_t event);

private:
    int32_t m_shown = -1;  // +0x0C: the item last put on the model
    int32_t m_own = -1;    // +0x10: the car's part of the category
};

// The window of the record 0x0071FD04.
class WheelsMenu {
public:
    // 0x00439300
    void enter(GarageHost& host, ui::Screen& s);
    // 0x00439730
    void update(GarageHost& host, ui::Screen& s, uint32_t event);
    // 0x00439630
    void leave(GarageHost& host, ui::Screen& s);

private:
    int32_t m_shown = -1;  // +0x1C
    int32_t m_own = -1;    // +0x20: the car's wheel
};

// The window of the record 0x0071FD28.
class DecalMenu {
public:
    // 0x00439AD0
    void enter(GarageHost& host, ui::Screen& s);
    // 0x00439E50
    void update(GarageHost& host, ui::Screen& s, uint32_t event);

private:
    int32_t m_shown = -1;                // +0x0C: the item last put on the model
    int32_t m_own = -1;                  // +0x10: the car's decal (+0x0C)
    std::array<int32_t, 2> m_colours{};  // +0x14, +0x18: the place of the car's decal colour in [DecalColour]
    int32_t m_menuShown = 1;             // +0x1C: the decal list shown (0 the colour chart)
    uint32_t m_colourOwn = 0;            // +0x2C: the car's decal colour (+0x38)
};

// sponsordecalmenu: the sponsors' decals on or off.
class SponsorDecalMenu {
public:
    // 0x0043A520
    void enter(GarageHost& host, ui::Screen& s);
    // 0x0043A590
    void update(GarageHost& host, ui::Screen& s, uint32_t event);
};

}  // namespace oj::frontend
