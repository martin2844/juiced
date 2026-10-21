// Sliders: the knob along the bar.

#include "Game/UI/UiScreen.h"
#include "Game/UI/UiInternal.h"

namespace oj::ui {

using namespace detail;

float Screen::sliderValue(const std::string& element) const {
    const int i = indexOf(element);
    return i >= 0 ? m_states[static_cast<size_t>(i)].slider.value : 0.0f;
}

void Screen::setSliderValue(const std::string& element, float value) {
    const int i = indexOf(element);
    if (i >= 0) m_states[static_cast<size_t>(i)].slider.value = value;
}

void Screen::setSliderActive(const std::string& element, bool active) {
    const int i = indexOf(element);
    if (i >= 0) m_states[static_cast<size_t>(i)].slider.active = active;
}

// 0x005B6020: an active Slider takes the direction held: a horizontal one LEFT down and RIGHT up, a vertical one UP
// down and DOWN up, half its range a second while not at that end; then held within 0 .. 1.
void Screen::slide(State& s, float dt, uint32_t scroll) {
    const uint32_t down = s.slider.vertical ? 1 : 3;
    if (scroll == down) {
        if (s.slider.value > 0.0f) s.slider.value = s.slider.value - dt * 0.5f;
    } else if (scroll == down + 1) {
        if (!(s.slider.value >= 1.0f)) s.slider.value = dt * 0.5f + s.slider.value;
    }
    if (s.slider.value <= 0.0f) {
        s.slider.value = 0.0f;
    } else if (s.slider.value > 1.0f) {
        s.slider.value = 1.0f;
    }
}

// A Slider (0x005B60E0) after its box: the knob, a square of 0.8 of the box's height (a horizontal one; the width for
// a vertical one) whose centre runs from 0.08 of the box's width in from the start to as far in from the end by the
// value, across the middle of the box. The box's transition moves the run but not the middle; a FADEIN or GROW gives
// its alpha to the knob's colour. The knob's picture (KnobTexture) at the colour (SliderColour), dark grey
// (0xFF323232) when the slider is not active; without a picture a plain square.
void Screen::drawSlider(const Element& e, State& s) {
    (void)e;
    const float sw = static_cast<float>(m_device.width());
    const float sh = static_cast<float>(m_device.height());
    float x0, y0, x1, y1, half, middle;
    if (!s.slider.vertical) {
        const float pad = s.w * sw * 0.04f;
        x0 = sw * s.x + pad + pad;
        x1 = (s.w + s.x) * sw - (pad + pad);
        half = s.h * sh * 0.4f;
        middle = (s.h * 0.5f + s.y) * sh;
        y0 = middle - half;
        y1 = half + half + y0;
    } else {
        const float pad = s.h * sh * 0.04f;
        y0 = s.y * sh + pad + pad;
        y1 = (s.y + s.h) * sh - (pad + pad);
        half = s.w * sw * 0.4f;
        middle = (s.w * 0.5f + s.x) * sw;
        x0 = middle - half;
        x1 = half + half + x0;
    }
    uint32_t colour = 0xFF323232;
    if (s.hasTransition) {
        const Transition& t = s.transition;
        x0 += t.offsetX, x1 += t.offsetX;
        y0 += t.offsetY, y1 += t.offsetY;
        if (t.type == Transition::kFadeIn || t.type == Transition::kGrow) {
            s.slider.colour = withAlpha(s.slider.colour, alphaByte(t.alpha));
            colour = withAlpha(colour, alphaByte(t.alpha));
        }
    }
    if (!s.slider.vertical) {
        y0 = middle - half;
        y1 = middle + half;
        const float c = (x1 - x0) * s.slider.value + x0;
        x1 = c + half;
        x0 = c - half;
    } else {
        x0 = middle - half;
        x1 = middle + half;
        const float c = (y1 - y0) * s.slider.value + y0;
        y1 = c + half;
        y0 = c - half;
    }
    if (s.slider.active) colour = s.slider.colour;
    const render::TextureId tex = s.slider.knobTexture.empty() ? 0 : texture(s.slider.knobTexture);
    const render::QuadVertex corners[4] = {
        {x0, y0, 0.0f, 0.0f, colour},
        {x1, y0, 1.0f, 0.0f, colour},
        {x0, y1, 0.0f, 1.0f, colour},
        {x1, y1, 1.0f, 1.0f, colour},
    };
    drawPicture(m_device, corners, tex, render::Blend::Alpha);
}

}  // namespace oj::ui
