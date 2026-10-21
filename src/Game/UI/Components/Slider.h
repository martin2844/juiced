#pragma once
// A Slider (0x005B5EE0): a knob along a bar.

#include <cstdint>
#include <string>

namespace oj::ui {

struct SliderState {
    // Sliders (0x005B5EE0)
    float value = 0.5f;                    // +0xC0
    bool vertical = false;            // +0xCC, SliderType VERTICAL
    uint32_t colour = 0xFFFFFFFF;     // +0xC8, SliderColour a r g b
    bool active = true;               // +0xD4
    std::string knobTexture = "ui_sliderhandle";  // +0xDC, KnobTexture
};

}  // namespace oj::ui
