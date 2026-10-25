// Dials: the needle turning to its angle.

#include "Game/UI/UiScreen.h"
#include "Game/UI/UiInternal.h"

#include <cmath>

namespace oj::ui {

using namespace detail;

namespace {

// 0x005B5AC0: a point turned by an angle about the origin (x87 fcos / fsin, the products rounded to floats).
void turn(float& x, float& y, float angle) {
    const double c = std::cos(static_cast<double>(angle));
    const double s = std::sin(static_cast<double>(angle));
    const float xc = static_cast<float>(x * c);
    const float ys = static_cast<float>(y * s);
    const float xs = static_cast<float>(x * s);
    const float yc = static_cast<float>(y * c);
    x = -ys + xc;
    y = xs + yc;
}

}  // namespace

void Screen::setDial(int element, float angle, bool jump) {
    State& s = m_states[static_cast<size_t>(element)];
    s.dial.target = angle;
    if (jump) s.dial.shown = angle;
}

float Screen::dialMin(int element) const {
    return m_states[static_cast<size_t>(element)].dial.min;
}

float Screen::dialMax(int element) const {
    return m_states[static_cast<size_t>(element)].dial.max;
}

void Screen::setNeedleColour(int element, uint32_t argb) {
    m_states[static_cast<size_t>(element)].dial.needleColour = argb;
}

// A Dial (0x005B5B10) after its box: the shown angle moves a quarter (+0xEC) of the way to the angle at each draw;
// the needle is a quad of the needle texture about the pivot (+0xE4, +0xE8 of the box), the length (+0xD8) up from it,
// the tail (+0xE0) down and the half width (+0xDC) each side, all of the box's height in pixels, turned by the angle;
// first a black copy 2 pixels right and up, then the needle in its colour (0x005181B0: the first corner's x and y and
// the second's y and x, in that order, moved onto the screen's edges within 2 pixels as x, y, x and y). The fixed
// build moves the second corner's x as an x and its y as a y.
void Screen::drawDial(const Element& e, State& s) {
    if (s.dial.needleTexture.empty()) {
        return;
    }
    const render::TextureId tex = texture(s.dial.needleTexture);
    if (tex == 0) {
        return;
    }
    s.dial.shown = (s.dial.target - s.dial.shown) * s.dial.ease + s.dial.shown;
    const float angle = s.dial.shown;
    const float sw = static_cast<float>(m_device.width());
    const float sh = static_cast<float>(m_device.height());
    const float px = (s.dial.pivotX * s.w + s.x) * sw;
    const float py = (s.dial.pivotY * s.h + s.y) * sh;
    const float height = sh * s.h;
    const float half = height * s.dial.needleWidth;
    // The tip's corners, then the tail's (texture coordinates (0, 0), (1, 0), (0, 1), (1, 1)).
    float x[4] = {-half, half, -half, half};
    float y[4] = {-(height * s.dial.needleLength), -(height * s.dial.needleLength), height * s.dial.needleTail,
                  height * s.dial.needleTail};
    for (int i = 0; i < 4; ++i) {
        turn(x[i], y[i], angle);
        x[i] += px;
        y[i] += py;
        if (s.hasTransition) {
            x[i] += s.transition.offsetX;
            y[i] += s.transition.offsetY;
        }
    }
    (void)e;
    auto needle = [&](float dx, float dy, uint32_t colour) {
        float vx[4], vy[4];
        for (int i = 0; i < 4; ++i) vx[i] = x[i] + dx, vy[i] = y[i] + dy;
        vx[0] = snapEdge(vx[0], sw);
        vy[0] = snapEdge(vy[0], sh);
        vx[1] = snapEdge(vx[1], sw);
        vy[1] = snapEdge(vy[1], sh);
        const render::QuadVertex corners[4] = {
            {vx[0], vy[0], 0.0f, 0.0f, colour},
            {vx[1], vy[1], 1.0f, 0.0f, colour},
            {vx[2], vy[2], 0.0f, 1.0f, colour},
            {vx[3], vy[3], 1.0f, 1.0f, colour},
        };
        drawPicture(m_device, corners, tex, render::Blend::Alpha);
    };
    needle(2.0f, -2.0f, 0xFF000000);
    needle(0.0f, 0.0f, s.dial.needleColour);
}

}  // namespace oj::ui
