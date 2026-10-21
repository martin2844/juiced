#pragma once
// The steering wheel's options (opts_wheel_dx9).

#include "Game/Frontend/Options/OptionsHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>
#include <string>

namespace oj::frontend {

// The window of the record 0x00721C10 (vtable 0x006F36B8).
class WheelOptions {
public:
    // 0x00483D10
    void enter(OptionsHost& host, ui::Screen& s);
    // 0x00483E80
    void update(OptionsHost& host, ui::Screen& s, uint32_t event);

private:
    std::string m_shownButton;   // +0x20: the highlighted button the meters were marked for
    uint32_t m_wheel = 0;        // +0x24: a steering wheel used (the settings' kSteeringWheel)
    uint32_t m_speed = 0;        // +0x28: speed sensitive steering (kWheelSpeed 0)
    uint32_t m_force = 0;        // +0x2C: the force feedback on (kWheelForce)
    float m_sensitivity = 0.0f;  // +0x30 (kSensitivity)
    float m_strength = 0.0f;     // +0x34 (kWheelStrength)
};

}  // namespace oj::frontend
