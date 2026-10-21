#pragma once
// The game options (opts_game_pc; from a race's pause opts_ingame_game and the second player's opts_ingame_game_ply2).

#include "Game/Frontend/Options/OptionsHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>
#include <string>

namespace oj::frontend {

// The window of the record 0x00721B00 (vtable 0x006F3638): the settings' values at enter (the save system's +0x78 ..),
// for BACK.
class GameOptions {
public:
    // 0x004813E0
    void enter(OptionsHost& host, ui::Screen& s);
    // 0x00481770
    void update(OptionsHost& host, ui::Screen& s, uint32_t event);

private:
    std::string m_shownButton;   // +0x2C: the highlighted button the texts were written for
    uint32_t m_wheel = 0;        // +0x34 (kSteeringWheel; the window's VIB)
    uint32_t m_map = 0;          // +0x3C (kMap)
    uint32_t m_automatic = 0;    // +0x40 (kAutomatic)
    uint32_t m_automatic2 = 0;   // +0x44 (the second player's)
    uint32_t m_units = 0;        // +0x48 (kMph)
    uint32_t m_aids = 0;         // +0x4C (kAids)
    uint32_t m_difficulty = 0;   // +0x50 (kDifficulty)
    float m_rumble = 0.0f;       // +0x54: the controller's rumble left (s)
    float m_sensitivity = 0.0f;  // +0x58, +0x5C (kSensitivity)
};

}  // namespace oj::frontend
