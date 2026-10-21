// Meters: a box filled to its value, and a meter used as a slider.

#include "Game/UI/UiScreen.h"
#include "Game/UI/UiInternal.h"

#include "Core/Base/X87.h"

namespace oj::ui {

using namespace detail;

void Screen::setMeter(int element, float value) {
    m_states[static_cast<size_t>(element)].meter.value = value;
}

void Screen::setMeter(const std::string& element, float value, bool jump) {
    const int i = indexOf(element);
    if (i >= 0) {
        State& s = m_states[static_cast<size_t>(i)];
        s.meter.value = value;
        if (jump) s.meter.shown = value;
    }
}

void Screen::setMeterSlider(const std::string& element, bool slider) {
    const int i = indexOf(element);
    if (i >= 0) m_states[static_cast<size_t>(i)].meter.slider = slider;
}

void Screen::setMeterActive(const std::string& element, bool active) {
    const int i = indexOf(element);
    if (i >= 0) m_states[static_cast<size_t>(i)].meter.active = active;
}

float Screen::meterShown(const std::string& element) const {
    const int i = indexOf(element);
    return i >= 0 ? m_states[static_cast<size_t>(i)].meter.shown : 0.0f;
}

void Screen::setMeterShown(const std::string& element, float value) {
    const int i = indexOf(element);
    if (i >= 0) m_states[static_cast<size_t>(i)].meter.shown = value;
}

// 0x005B6580: an active slider Meter takes the direction held: a horizontal one LEFT down and RIGHT up, a vertical
// one UP down and DOWN up, half its range a second while the shown value is not at the end; the shown value then
// within 0 .. 1 and the value the same.
void Screen::slideMeter(const Element& e, State& s, float dt, uint32_t scroll) {
    const Property* type = e.find("MeterType");
    const bool vertical =
        type == nullptr || type->values.empty() || _stricmp(type->values[0].c_str(), "HORIZONTAL") != 0;
    const uint32_t down = vertical ? 1 : 3;
    if (scroll == down + 1) {
        if (!(s.meter.shown >= 1.0f)) s.meter.shown = dt * 0.5f + s.meter.shown;
    } else if (scroll == down) {
        if (s.meter.shown > 0.0f) s.meter.shown = s.meter.shown - dt * 0.5f;
    }
    s.meter.shown = std::clamp(s.meter.shown, 0.0f, 1.0f);
    s.meter.value = s.meter.shown;
}

// A Meter (0x005B6650) after its box: the shown value moves 0.15 of the way to the value at each draw; nothing is
// drawn within 0.01 of 0. MeterType VERTICAL (the default) fills from the bottom, HORIZONTAL from the left,
// MeterCentered from the middle either way. The MeterTexture covers that part of the box with the same part of the
// picture; the far corners' colours move from the near corners' to their own by the value, all under the box's
// fading transition.
void Screen::drawMeter(const Element& e, State& s) {
    const Property* tex = e.find("MeterTexture");
    if (tex == nullptr || tex->values.empty()) {
        return;
    }
    s.meter.shown = (s.meter.value - s.meter.shown) * 0.15f + s.meter.shown;
    const float v = s.meter.shown;
    if (v < 0.01f && v > -0.01f) {
        return;
    }
    const Property* type = e.find("MeterType");
    const bool vertical =
        type == nullptr || type->values.empty() || _stricmp(type->values[0].c_str(), "HORIZONTAL") != 0;
    float fx0 = 0.0f, fx1 = 1.0f, fy0 = 0.0f, fy1 = 1.0f;
    if (vertical) {
        fy0 = 1.0f - v;
    } else if (e.has("MeterCentered")) {
        if (v <= 0.0f) {
            fx0 = 0.5f - v * -0.5f;
            fx1 = 0.5f;
        } else {
            fx0 = 0.5f;
            fx1 = v * 0.5f + 0.5f;
        }
    } else {
        fx1 = v;
    }
    uint32_t corner[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
    for (const Property& p : e.properties) {
        if (p.name != "MeterVertexColour" || p.values.empty()) {
            continue;
        }
        const std::string& which = p.values[0];
        const int i = _stricmp(which.c_str(), "TOPRIGHT") == 0   ? 1
                      : _stricmp(which.c_str(), "BOTLEFT") == 0  ? 2
                      : _stricmp(which.c_str(), "BOTRIGHT") == 0 ? 3
                                                                 : 0;
        corner[i] = argbOf(p.values, 1);
    }
    const Transition* t = s.hasTransition ? &s.transition : nullptr;
    for (uint32_t& c : corner) {
        c = fadeAlpha(c, t);
    }
    // Each byte from `from` towards `to` by v, rounded, kept to a byte.
    auto towards = [v](uint32_t from, uint32_t to) {
        uint32_t out = 0;
        for (int shift = 0; shift < 32; shift += 8) {
            const int a = static_cast<int>((from >> shift) & 0xFF);
            const int b = static_cast<int>((to >> shift) & 0xFF);
            const int step = x87::chop(static_cast<float>(b - a) * v);
            out |= (static_cast<uint32_t>(a + step) & 0xFF) << shift;
        }
        return out;
    };
    uint32_t colours[4];
    if (vertical) {
        colours[0] = towards(corner[2], corner[0]);
        colours[1] = towards(corner[3], corner[1]);
        colours[2] = corner[2];
        colours[3] = corner[3];
    } else {
        colours[0] = corner[0];
        colours[1] = towards(corner[0], corner[1]);
        colours[2] = corner[2];
        colours[3] = towards(corner[2], corner[3]);
    }
    float x0, y0, x1, y1;
    elementRect(e, s, x0, y0, x1, y1);
    if (t != nullptr) {
        x0 += t->offsetX, x1 += t->offsetX;
        y0 += t->offsetY, y1 += t->offsetY;
    }
    const float w = x1 - x0;
    const float h = y1 - y0;
    quad(x0 + fx0 * w, y0 + fy0 * h, x0 + fx1 * w, y0 + fy1 * h, colours, texture(tex->values[0]), fx0, fx1, fy0,
         fy1, false);
}

}  // namespace oj::ui
