#pragma once
// A Meter: a box filled to its value (MeterType, MeterTexture).

namespace oj::ui {

struct MeterState {
    // Meters
    float value = 0.0f;       // +0xF0, the value
    float shown = 0.0f;  // +0xEC
    bool active = false; // +0xF8
    bool slider = false; // +0xFC
};

}  // namespace oj::ui
