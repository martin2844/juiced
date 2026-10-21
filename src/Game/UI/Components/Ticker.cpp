// Tickers: the text moving across the box.

#include "Game/UI/UiScreen.h"
#include "Game/UI/UiInternal.h"

#include "Core/Base/X87.h"

namespace oj::ui {

using namespace detail;

// 0x005B52C0: a ticker's width: its text's (the font's +0x08, none without a font) and its box's; the text starts
// half of it in, at most 0.95 of the box and within 0 .. 550; the ramp and the fade from 0.
void Screen::measureTicker(int element) {
    const Element& e = m_window.elements[static_cast<size_t>(element)];
    State& s = m_states[static_cast<size_t>(element)];
    s.ticker.width = 0.0f;
    const std::wstring text = s.text.set ? s.text.value : caption(e);
    const Property* fp = e.find("Font");
    if (!text.empty() && fp != nullptr && !fp->values.empty()) {
        if (Font* f = font(s.text.fontName.empty() ? fp->values[0] : s.text.fontName); f != nullptr) {
            s.ticker.width = static_cast<float>(static_cast<uint32_t>(textWidth(*f, text)));
        }
    }
    const float box = static_cast<float>(m_device.width()) * s.w;
    s.ticker.width = box + s.ticker.width;
    s.ticker.scroll = s.ticker.width * 0.5f;
    if (box * 0.95f < s.ticker.scroll) s.ticker.scroll = box * 0.95f;
    s.ticker.scroll = s.ticker.scroll < 0.0f ? 0.0f : s.ticker.scroll > 550.0f ? 550.0f : s.ticker.scroll;
    s.ticker.ramp = 0.0f;
    s.ticker.fade = 0.0f;
}

void Screen::restartTickers() {
    // The menus' help tickers are not in the window's list.
    std::vector<bool> help(m_window.elements.size(), false);
    for (const State& s : m_states) {
        if (s.menu.helpTicker >= 0 && static_cast<size_t>(s.menu.helpTicker) < help.size()) {
            help[static_cast<size_t>(s.menu.helpTicker)] = true;
        }
    }
    for (size_t i = 0; i < m_window.elements.size(); ++i) {
        if (m_window.elements[i].kind == ElementKind::Ticker && !help[i]) measureTicker(static_cast<int>(i));
    }
}

// A ticker (0x005B53C0) after its box, shown with a text and a font: drawn within its box (held to the screen). It
// fades in over a second of the window's step (+0x164), then moves left (its speed's ramp +0x160 coming up over a
// second): 400 x the speed x the ramp pixels a second, back to the start past its width (+0x154); a width of 0 has
// the text set again (0x005B5280, the effect held) and measured. The text's right end starts 2 pixels in from the
// box's right side less what it moved, at the box's middle + 0.3 x the font's height; an unfinished transition
// restarts the fade and moves it by its offset; the font's colour with the fade as its alpha.
void Screen::drawTicker(const Element& e, State& s) {
    const std::wstring text = s.text.set ? s.text.value : caption(e);
    if (text.empty()) return;
    const Property* fp = e.find("Font");
    if (fp == nullptr || fp->values.empty()) return;
    Font* f = font(s.text.fontName.empty() ? fp->values[0] : s.text.fontName);
    if (f == nullptr) return;
    float x0, y0, x1, y1;
    elementRect(e, s, x0, y0, x1, y1);
    const float sw = static_cast<float>(m_device.width());
    const float sh = static_cast<float>(m_device.height());
    const auto hold = [](float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; };
    const render::ClipRect rect{hold(x0, 0.0f, sw - 1.0f), hold(y0, 0.0f, sh - 1.0f), hold(x1, 1.0f, sw),
                                hold(y1, 1.0f, sh)};
    m_device.setClip(&rect);
    float x = x1 - 2.0f - s.ticker.scroll;
    if (!(s.ticker.fade >= 1.0f)) {
        s.ticker.fade = hold(m_drawTime + s.ticker.fade, 0.0f, 1.0f);
    } else {
        if (s.ticker.ramp < 1.0f) s.ticker.ramp = m_drawTime + s.ticker.ramp;
        s.ticker.ramp = hold(s.ticker.ramp, 0.0f, 1.0f);
        s.ticker.scroll = m_drawTime * s.ticker.speed * s.ticker.ramp * 400.0f + s.ticker.scroll;
        if (s.ticker.scroll > s.ticker.width) s.ticker.scroll = 0.0f;
        if (s.ticker.width == 0.0f) {
            const int index = static_cast<int>(&s - m_states.data());
            setText(index, text);
            measureTicker(index);
        }
    }
    float y = (y0 + y1) * 0.5f + static_cast<float>(f->height()) * 0.3f;
    if (s.hasTransition) {
        if (!s.transition.done()) s.ticker.fade = 0.0f;
        x += s.transition.offsetX;
        y += s.transition.offsetY;
    }
    const uint32_t own = s.text.fontColours ? s.text.font : argbOf(fp->values, 1);
    drawString(f, withAlpha(own, static_cast<uint8_t>(x87::chop(s.ticker.fade * 255.0f))), text, x, y);
    m_device.setClip(nullptr);
}

}  // namespace oj::ui
