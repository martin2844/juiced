#pragma once
// The repair window (vtable 0x006F0404, table 0x007203C4; its .ui, with newmenu, Condition1 .. 3 and CostText1 .. 3,
// is not among the game's files; never registered: its factory entry is never handed to 0x005A8E30; opened by
// "unregistered:repair").

#include "Game/Frontend/Garage/GarageHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

class Repair {
public:
    // 0x00445630
    void enter(GarageHost& host, ui::Screen& s);
    // 0x00445A70
    void update(GarageHost& host, ui::Screen& s, uint32_t event);

private:
    // The costs of the brakes' (0x00446030), the tyres' (0x00445FA0) and the nitro's (0x00445F10) repairs.
    float brakesCost(GarageHost& host);
    float tyresCost(GarageHost& host);
    float nitroCost(GarageHost& host);
    // 0x00445730: the conditions and costs shown; 0x00445660: newmenu's items.
    void showTexts(GarageHost& host, ui::Screen& s);
    void fillItems(GarageHost& host, ui::Screen& s);
    // The repairs: the brakes (0x004463D0), the tyres (0x00446270), the nitro (0x004460C0) and all (0x00446530).
    void repairBrakes(GarageHost& host);
    void repairTyres(GarageHost& host);
    void repairNitro(GarageHost& host);
    void repairAll(GarageHost& host);

    int32_t m_answer = -1;  // +0x0C
    int32_t m_asked = 0;    // +0x10
};

}  // namespace oj::frontend
