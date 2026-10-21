// Colour charts: the hues and their shades, and the cursor over them.

#include "Game/UI/UiScreen.h"
#include "Game/UI/UiInternal.h"

#include "Core/Base/X87.h"
#include "Core/Input/Input.h"

namespace oj::ui {

using namespace detail;

namespace {

// 0x005B7270: a colour of the chart's row: a negative brightness b scales r, g and b by 1 + b, another adds b x b x
// 255 to each; each held within 0 .. 255 and cut; opaque.
uint32_t shadeColour(float b, float r, float g, float bl) {
    if (b < 0.0f) {
        const float s = 1.0f - -b;
        r = r * s;
        g = s * g;
        bl = s * bl;
    } else {
        const float add = b * b * 255.0f;
        r = add + r;
        g = add + g;
        bl = add + bl;
    }
    auto byte = [](float v) {
        if (v < 0.0f) return 0u;
        if (v > 255.0f) return 255u;
        return static_cast<uint32_t>(x87::chop(v)) & 0xFF;
    };
    return 0xFF000000u | (byte(r) << 16) | (byte(g) << 8) | byte(bl);
}

}  // namespace

uint32_t Screen::chartColour(const std::string& element) const {
    const int i = indexOf(element);
    return i >= 0 ? m_states[static_cast<size_t>(i)].chart.colour : 0;
}

void Screen::chartFind(const std::string& element, uint32_t argb) {
    const int i = indexOf(element);
    if (i < 0) return;
    m_states[static_cast<size_t>(i)].chart.finding = true;
    m_states[static_cast<size_t>(i)].chart.target = argb;
}

bool Screen::chartFinding(const std::string& element) const {
    const int i = indexOf(element);
    return i >= 0 && m_states[static_cast<size_t>(i)].chart.finding;
}

int Screen::chartColumn(const std::string& element) const {
    const int i = indexOf(element);
    return i >= 0 ? m_states[static_cast<size_t>(i)].chart.column : 0;
}

int Screen::chartRow(const std::string& element) const {
    const int i = indexOf(element);
    return i >= 0 ? m_states[static_cast<size_t>(i)].chart.row : 0;
}

// 0x005B6FE0: the wait (+0xD0) less the frame time, the blink (+0xE4) up by twice it and back to 0 past 1, the wait
// held within 0 .. 0.03. With no wait, shown: UP and DOWN move the cursor a row up or down (rows 0 .. (+0xC8 - 1) x 2,
// to the nearest), LEFT and RIGHT a column (columns 0 .. 7 x +0xCC - 1, without the greys 7 x (+0xCC - 1) - 1), and
// the wait 0.03.
void Screen::moveChart(State& s, float dt, uint32_t scroll) {
    if (s.chart.wait != 0.0f) s.chart.wait = s.chart.wait - dt;
    s.chart.blink = dt + dt + s.chart.blink;
    if (s.chart.blink > 1.0f) s.chart.blink = 0.0f;
    if (s.chart.wait < 0.0f) {
        s.chart.wait = 0.0f;
    } else if (s.chart.wait > 0.03f) {
        s.chart.wait = 0.03f;
    }
    if (s.chart.wait != 0.0f || s.hidden || scroll < input::kUp || scroll > input::kRight) return;
    switch (scroll) {
    case input::kUp:
        s.chart.row -= 1;
        if (s.chart.row < 0) s.chart.row = 0;
        break;
    case input::kDown: {
        s.chart.row += 1;
        const int last = static_cast<int>(std::nearbyint((s.chart.halfRows - 1.0f) + (s.chart.halfRows - 1.0f)));
        if (s.chart.row > last) s.chart.row = last;
        break;
    }
    case input::kLeft:
        s.chart.column -= 1;
        if (s.chart.column < 0) s.chart.column = 0;
        break;
    default: {
        s.chart.column += 1;
        const float steps = s.chart.greys ? s.chart.steps : s.chart.steps - 1.0f;
        const int last = -1 - x87::chop(steps * -7.0f);
        if (s.chart.column > last) s.chart.column = last;
        break;
    }
    }
    s.chart.wait = 0.03f;
}

// A ColourChart (0x005B7460) after its box: the cells of the chart the window's picture shows (ui_colourbar). Its
// width holds 7 x +0xCC cells (7 x (+0xCC - 1) without the greys), its height (+0xC8 - 0.5) x 2 rows; a row's
// brightness runs from 1 at the top to -1 at the bottom (row / (+0xC8 - 1)). Each row: red to yellow, yellow to green,
// green to cyan, cyan to blue, blue to magenta, magenta to red (+0xCC cells each, 255 / +0xCC a step) shaded by the
// brightness (0x005B7270), then the greys (255 / (+0xC8 - 1) x row x 0.5 + 128, the element colour's alpha). Looking
// for a colour (+0xD4) its brightness first goes up by a third (held within 0 .. 255, cut); the first cell within 8
// of it in red, green and blue takes the cursor, and the looking ends after the chart. The cell under the cursor gives
// +0xD8; the cursor's picture (ui_colourselect, white) covers the cell grown by blink x its width each side and blink
// x its height (10 at most) x 4 above and below. Then +0xD8's red, green and blue are taken to 0.75 (to the nearest).
void Screen::drawChart(const Element& e, State& s) {
    (void)e;
    const float sw = static_cast<float>(m_device.width());
    const float sh = static_cast<float>(m_device.height());
    float left = sw * s.x;
    float top = sh * s.y;
    float right = (s.w + s.x) * sw;
    float bottom = (s.h + s.y) * sh;
    if (s.hasTransition) {
        left += s.transition.offsetX;
        top += s.transition.offsetY;
        right += s.transition.offsetX;
        bottom += s.transition.offsetY;
    }
    const float width = right - left;
    const float height = bottom - top;
    const float hues = (s.chart.greys ? s.chart.steps : s.chart.steps - 1.0f) * 7.0f;
    const float cellW = width / hues;
    const float cellH = height / ((s.chart.halfRows - 0.5f) + (s.chart.halfRows - 0.5f));
    if (s.chart.finding) {
        auto up = [](uint32_t byte) {
            float v = static_cast<float>(byte) * 1.3333334f;
            if (v < 0.0f) v = 0.0f;
            if (v > 255.0f) v = 255.0f;
            return static_cast<uint32_t>(x87::chop(v)) & 0xFF;
        };
        const uint32_t t = s.chart.target;
        s.chart.target = (t & 0xFF000000u) | (up((t >> 16) & 0xFF) << 16) | (up((t >> 8) & 0xFF) << 8) | up(t & 0xFF);
    }
    const auto near = [&](uint32_t c) {
        const auto within = [](uint32_t target, uint32_t cell) {
            const int t = static_cast<int>(target), v = static_cast<int>(cell);
            return t > v - 8 && t < v + 8;
        };
        const uint32_t t = s.chart.target;
        return within((t >> 16) & 0xFF, (c >> 16) & 0xFF) && within((t >> 8) & 0xFF, (c >> 8) & 0xFF) &&
               within(t & 0xFF, c & 0xFF);
    };
    float cx0 = 0.0f, cy0 = 0.0f, cx1 = 0.0f, cy1 = 0.0f;
    const int halfRows = static_cast<int>(std::nearbyint(s.chart.halfRows));
    const int steps = x87::chop(s.chart.steps);
    float y0 = top;
    float y1 = top + cellH;
    int rowIndex = 0;
    for (int row = halfRows - 1; row > -halfRows; --row) {
        float x0 = left;
        float x1 = left + cellW;
        int column = 0;
        float grey = 0.0f;
        float bright = 0.0f;
        if (s.chart.halfRows != 1.0f) {
            const float span = s.chart.halfRows - 1.0f;
            grey = 255.0f / span * static_cast<float>(row);
            bright = static_cast<float>(row) / span;
        }
        const auto cell = [&](uint32_t colour) {
            if (s.chart.finding && near(colour)) {
                s.chart.column = column;
                s.chart.row = rowIndex;
                s.chart.colour = colour;
                s.chart.finding = false;
            }
            if (column == s.chart.column && rowIndex == s.chart.row) {
                cx0 = x0, cy0 = y0, cx1 = x1, cy1 = y1;
                s.chart.colour = colour;
            }
            ++column;
            x0 = x0 + cellW;
            x1 = x1 + cellW;
        };
        const float step = 255.0f / s.chart.steps;
        for (int i = 0; i < steps; ++i) cell(shadeColour(bright, 255.0f, step * static_cast<float>(i), 0.0f));
        for (int i = 0; i < steps; ++i) cell(shadeColour(bright, 255.0f - step * static_cast<float>(i), 255.0f, 0.0f));
        for (int i = 0; i < steps; ++i) cell(shadeColour(bright, 0.0f, 255.0f, step * static_cast<float>(i)));
        for (int i = 0; i < steps; ++i) cell(shadeColour(bright, 0.0f, 255.0f - step * static_cast<float>(i), 255.0f));
        for (int i = 0; i < steps; ++i) cell(shadeColour(bright, step * static_cast<float>(i), 0.0f, 255.0f));
        for (int i = 0; i < steps; ++i) cell(shadeColour(bright, 255.0f, 0.0f, 255.0f - step * static_cast<float>(i)));
        if (s.chart.greys) {
            for (int i = 0; i < steps; ++i) {
                float v = grey * 0.5f + 128.0f;
                if (v < 0.0f) v = 0.0f;
                if (v > 255.0f) v = 255.0f;
                const uint32_t g = static_cast<uint32_t>(x87::chop(v)) & 0xFF;
                cell((s.corners[0] & 0xFF000000u) | (g << 16) | (g << 8) | g);
            }
        }
        y0 = y0 + cellH;
        ++rowIndex;
        y1 = y1 + cellH;
    }
    s.chart.finding = false;
    const float cw = cx1 - cx0;
    float ch = cy1 - cy0;
    if (ch < 0.0f) {
        ch = 0.0f;
    } else if (ch > 10.0f) {
        ch = 10.0f;
    }
    const float dx = 1.0f * s.chart.blink * cw;
    const float dy = ch * s.chart.blink * 4.0f;
    const render::TextureId tex = texture("ui_colourselect");
    const render::QuadVertex corners[4] = {
        {cx0 - dx, cy0 - dy, 0.0f, 0.0f, 0xFFFFFFFF},
        {cx1 + dx, cy0 - dy, 1.0f, 0.0f, 0xFFFFFFFF},
        {cx0 - dx, cy1 + dy, 0.0f, 1.0f, 0xFFFFFFFF},
        {cx1 + dx, cy1 + dy, 1.0f, 1.0f, 0xFFFFFFFF},
    };
    drawPicture(m_device, corners, tex, render::Blend::Alpha);
    const auto threeQuarters = [](uint32_t byte) {
        return static_cast<uint32_t>(std::nearbyint(static_cast<float>(byte) * 0.75f)) & 0xFF;
    };
    const uint32_t c = s.chart.colour;
    s.chart.colour = (c & 0xFF000000u) | (threeQuarters((c >> 16) & 0xFF) << 16) |
                    (threeQuarters((c >> 8) & 0xFF) << 8) | threeQuarters(c & 0xFF);
}

}  // namespace oj::ui
