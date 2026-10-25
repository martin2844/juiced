#pragma once
// The car's stats over a window (Car_Stats2, Car_Stats2purple) and the makers' logos over the windows
// (car_StatsLogos).

#include "Game/Frontend/WindowHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>
#include <vector>

namespace oj::frontend {

// The windows of the records Car_Stats2 (0x00721DD8) and Car_Stats2purple (0x00721F20), vtable 0x006F5258, on the
// car [0x00728C60].
class CarStats {
public:
    // The record's +0x10 ([0x00721DE8], [0x00721F30]), set by the windows that open it: a car of the player's garage.
    void setGarage(int32_t garage) { m_garage = garage; }
    // 0x004C42D0
    void enter(WindowHost& host, ui::Screen& s);
    // 0x004C4770
    void update(WindowHost& host, ui::Screen& s, uint32_t event);
    // 0x004C4710: the blinking elements' alpha back to 1 (the Menu's item is kept by the windows' leave).
    void leave(ui::Screen& s);
    // 0x004C5260: car_StatsLogos over the windows.
    static void enterLogos(WindowHost& host, ui::Screen& s);

private:
    // 0x004C4920: the parts' icons.
    void parts(WindowHost& host, ui::Screen& s);

    int32_t m_model = 0;       // +0x0C: the car's model id
    int32_t m_garage = 0;      // +0x10: a car of the player's garage
    std::vector<int> m_blink;  // +0x14 .., the count +0x140: the elements that blink
    static inline int32_t m_logosShown = 0;    // [0x00728C64]: car_StatsLogos shown over the window
    static inline float m_blinkPhase = 0.0f;   // [0x007A2D40]: the blink's phase
};

}  // namespace oj::frontend
