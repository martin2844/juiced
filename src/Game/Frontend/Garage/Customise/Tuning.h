#pragma once
// The performance parts: tuningmenu, performtype, performmod, buymod, the performance meters (performance, over the
// windows) and the suppliers.

#include "Core/Cars/CarDefs.h"
#include "Game/Frontend/Garage/Garage.h"
#include "Game/Frontend/Garage/GarageHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record 0x0071FD68.
class TuningMenu {
public:
    // 0x0043A780
    void enter(GarageHost& host, ui::Screen& s);
    // 0x0043A8B0
    void update(GarageHost& host, ui::Screen& s, uint32_t event);
    // 0x0043A860
    void leave(GarageHost& host, ui::Screen& s);

private:
    int32_t m_returning = 0;      // +0x0C: back from a window
    const Car* m_car = nullptr;   // +0x10: the car the menu was last on
};

// The window of the record 0x0071FF9C: a kind's level, or its part taken off.
class PerformType {
public:
    // 0x00441560
    void enter(GarageHost& host, ui::Screen& s);
    // 0x00441800
    void update(GarageHost& host, ui::Screen& s, uint32_t event);

private:
    int32_t m_answer = -1;  // +0x0C: the removal's answer
    int32_t m_asked = 0;    // +0x10
};

// The window of the record 0x0071FFB4: a supplier's part of the kind and level.
class PerformMod {
public:
    // 0x00441D20
    void enter(GarageHost& host, ui::Screen& s);
    // 0x00442020
    void update(GarageHost& host, ui::Screen& s, uint32_t event);
    // 0x00441F90
    void leave(GarageHost& host, ui::Screen& s);

private:
    int32_t m_current = -1;              // +0x10: the car's part of the kind
    const CarPart* m_chosen = nullptr;   // +0x14
};

// The window of the record 0x007203D8: the purchase's question over a part's window.
class BuyMod {
public:
    // 0x00446750
    void enter(GarageHost& host, ui::Screen& s);
    // 0x00446B80
    void update(GarageHost& host);

private:
    int32_t m_answer = -1;  // +0x0C
};

// performance over the windows: the part's meters.
class Performance {
public:
    // 0x004423C0
    void enter(GarageHost& host, ui::Screen& s);
    // 0x00442420
    void update(GarageHost& host, ui::Screen& s);

private:
    int32_t m_part = -1;   // +0x1C: the part shown ([0x0075ED08])
    int32_t m_brand = -1;  // +0x20: its supplier ([0x00715C30])
};

// suppliers: the suppliers of the kind's parts of the catalogue's level.
class Suppliers {
public:
    // 0x00440B80
    void enter(GarageHost& host, ui::Screen& s);
    // 0x00440CC0
    void update(GarageHost& host, ui::Screen& s, uint32_t event);
};

}  // namespace oj::frontend
