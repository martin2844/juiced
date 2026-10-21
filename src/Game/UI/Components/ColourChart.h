#pragma once
// A ColourChart (0x005B6E70): the hues and their shades, a cursor moving over them.

#include <cstdint>

namespace oj::ui {

struct ColourChartState {
    // Colour charts (0x005B6E70)
    int column = 0;           // +0xC0
    int row = 0;              // +0xC4
    float halfRows = 12.0f;   // +0xC8: the rows run from it - 1 down to 1 - it
    float steps = 7.0f;       // +0xCC: the cells of each hue
    float wait = 0.0f;        // +0xD0: the time before the cursor moves again
    bool finding = false;     // +0xD4
    uint32_t colour = 0xFFFFFFFF;  // +0xD8
    uint32_t target = 0;      // +0xDC
    float blink = 0.0f;       // +0xE4
    bool greys = true;        // +0xE8: a seventh hue of greys
};

}  // namespace oj::ui
