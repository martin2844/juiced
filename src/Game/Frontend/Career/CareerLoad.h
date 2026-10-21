#pragma once
// careerload (and garageload, one record): three frames, then the career's window.

#include "Game/Frontend/Career/CareerHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The record 0x0071FC60.
class CareerLoad {
public:
    // 0x004358A0
    void enter(CareerHost& host, ui::Screen& s);
    void update(CareerHost& host, ui::Screen& s);

private:
    int m_frames = 0;  // +0x0C
};

}  // namespace oj::frontend
