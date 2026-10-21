#pragma once
// carbuy: buying a car of the dealer's (or selling one of the garage).

#include "Game/Frontend/Garage/Garage.h"
#include "Game/Frontend/Garage/GarageHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record 0x0071FCB0.
class CarBuy {
public:
    // 0x00438150
    void enter(GarageHost& host, ui::Screen& s);
    // 0x00438BB0
    void update(GarageHost& host, ui::Screen& s, uint32_t event);
    // 0x00438A70
    void leave(GarageHost& host);

private:
    int32_t m_leaving = 0;  // +0x28
    int32_t m_answer = -1;  // +0x2C
    int32_t m_asked = 0;    // +0x30
    int32_t m_note = 1;     // +0x34: the note's turn
    int32_t m_done = 0;     // +0x38: the car bought or sold
};

}  // namespace oj::frontend
