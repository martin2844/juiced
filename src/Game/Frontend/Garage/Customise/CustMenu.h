#pragma once
// custmenu: the customising menu over carsel (tuning, styling, paint).

#include "Game/Frontend/Garage/Garage.h"
#include "Game/Frontend/Garage/GarageHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record 0x0071FCEC.
class CustMenu {
public:
    // 0x00439050
    void enter(GarageHost& host, ui::Screen& s);
    // 0x004390A0
    void update(GarageHost& host, ui::Screen& s, uint32_t event);

private:
    int32_t m_returning = 0;  // +0x14: back from a window
};

}  // namespace oj::frontend
