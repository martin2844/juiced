#pragma once
// The career's menu (CMSMainMenu).

#include "Core/Career/Career.h"
#include "Game/Frontend/Career/CareerHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record 0x0072229C.
class CareerMenu {
public:
    // 0x004CE160
    void enter(CareerHost& host, ui::Screen& s);
    // 0x004CE1F0
    void update(CareerHost& host, ui::Screen& s, uint32_t event);

private:
    // 0x004CE870: a career without cars and with less than 30000 gets the rest of 30000 (the money counter +0x04, none
    // below 0) and one of four notes (lines 1955 + rand() & 3).
    void topUp(CareerHost& host, Career& career);
    // 0x004CE910: when every garage car needs a repair (its grid damaged, its +0x74 or +0x7C over 0.1, or its nitro not
    // full with a nitro part) that costs at least the money, one of four notes (lines 1960 + rand() & 3) and every car
    // repaired (0x0048EB20).
    void freeRepair(CareerHost& host, Career& career);

    int32_t m_leaving = 0;  // +0x0C
    int32_t m_answer = -1;  // +0x10: the answer to leaving the career
};

}  // namespace oj::frontend
