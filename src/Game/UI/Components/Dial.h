#pragma once
// A Dial (0x005B5960; NeedleTexture, NeedleData): a needle turning to an angle.

#include <cstdint>
#include <string>

namespace oj::ui {

struct DialState {
    // Dials (0x005B5960; NeedleTexture, NeedleData)
    std::string needleTexture;          // +0xD0
    uint32_t needleColour = 0xFFFFFFFF; // +0xD4
    float min = -2.3561945f;        // +0xC0
    float max = 2.3561945f;         // +0xC4
    float shown = 2.3561945f;       // +0xC8
    float target = -2.3561945f;     // +0xCC
    float needleLength = 0.45f;         // +0xD8: of the box's height
    float needleWidth = 0.04f;          // +0xDC: half the width
    float needleTail = 0.08f;           // +0xE0
    float pivotX = 0.5f;                // +0xE4: of the box
    float pivotY = 0.5f;                // +0xE8
    float ease = 0.25f;             // +0xEC
};

}  // namespace oj::ui
