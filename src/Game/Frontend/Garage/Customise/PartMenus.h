#pragma once
// The windows of one part: the exhausts, the suspension's ride, the engine chip and the gearbox.

#include "Game/Frontend/Garage/Garage.h"
#include "Game/Frontend/Garage/GarageHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record 0x0071FF80.
class ExhaustMenu {
public:
    // 0x00440EF0
    void enter(GarageHost& host, ui::Screen& s);
    // 0x004411C0
    void update(GarageHost& host, ui::Screen& s, uint32_t event);

private:
    int32_t m_place = 0;     // +0x14: the highlighted item put on the model
    int32_t m_current = -1;  // +0x18: the car's exhaust
};

// The window of the record 0x0071FF38.
class SuspensionMenu {
public:
    // 0x00440560
    void enter(GarageHost& host, ui::Screen& s);
    // 0x00440620
    void update(GarageHost& host, ui::Screen& s, uint32_t event);

private:
    float m_original = 0.0f;  // +0x10: the car's ride
};

// The window of the record 0x0071FF4C.
class EngineChip {
public:
    // 0x004407B0
    void enter(GarageHost& host, ui::Screen& s);
    // 0x004408E0
    void update(GarageHost& host, ui::Screen& s, uint32_t event);

private:
    float m_original = 1.0f;  // +0x14: the car's chip
    float m_chosen = 0.0f;    // +0x18: the chip less 1
    float m_rating = 0.0f;    // +0x1C: the rating with the chip at 1
    int32_t m_bhp = 0;        // +0x20: the rating shown
    int32_t m_ownBhp = 0;     // +0x24: the car's rating (cut)
};

// The window of the record 0x0071FFCC.
class GearboxMenu {
public:
    // 0x004425B0
    void enter(GarageHost& host, ui::Screen& s);
    // 0x004426B0
    void update(GarageHost& host, ui::Screen& s, uint32_t event);

private:
    float m_original = 0.5f;  // +0x10: the car's gearing
    int32_t m_answer = -1;    // +0x14
    int32_t m_asked = 0;      // +0x18
};

}  // namespace oj::frontend
