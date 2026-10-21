// Texts: an element's text, its font and how it is measured and drawn.

#include "Game/UI/UiScreen.h"
#include "Game/UI/UiInternal.h"

#include "Core/Base/GameFiles.h"
#include "Core/Base/Log.h"
#include "Core/Base/X87.h"

#include <cmath>
#include <filesystem>

namespace oj::ui {

using namespace detail;

void Screen::drawOverlayText(const std::wstring& text, float x, float y, float height,
                             uint32_t colour, float maxWidth) {
    Font* f = font("goodtime11");
    if (!f || f->height() <= 0) return;
    const float scale = height / static_cast<float>(f->height());
    float pen = x;
    const float baseline = y + f->ascent * scale;
    for (wchar_t c : text) {
        if (c == L' ') { pen += f->data.spaceAdvance * scale; continue; }
        const D8f::Glyph* g = f->data.find(c);
        if (!g) continue;
        if (pen + g->width * scale > x + maxWidth) break;
        if (g->page < f->pages.size()) {
            const float gx = pen + g->xOffset * scale, gy = baseline + g->yOffset * scale;
            const float right = gx + g->width * scale, bottom = gy + g->height * scale;
            const render::QuadVertex q[4] = {
                {gx, gy, g->u0, g->v0, colour}, {right, gy, g->u1, g->v0, colour},
                {gx, bottom, g->u0, g->v1, colour}, {right, bottom, g->u1, g->v1, colour}
            };
            render::applySampler(m_device, f->samplers[g->page], 0);
            m_device.drawQuad(q, f->pages[g->page], render::Blend::Alpha, true);
        }
        pen += g->advance * scale;
    }
}

void Screen::setText(int element, const std::wstring& text) {
    State& s = m_states[static_cast<size_t>(element)];
    s.text.set = true;
    s.text.value = text;
    s.modifier.restart();
}

void Screen::appendText(int element, const std::wstring& more) {
    State& s = m_states[static_cast<size_t>(element)];
    if (!s.text.set) {
        const Element& e = m_window.elements[static_cast<size_t>(element)];
        if (const Property* c = e.find("Caption"); c != nullptr && !c->values.empty()) {
            s.text.value = m_texts.at(static_cast<size_t>(std::atoi(c->values[0].c_str())));
        }
        s.text.set = true;
    }
    s.text.value += more;
}

// 0x005B1A00: the text emptied, the transition left as it is.
void Screen::clearText(int element) {
    State& s = m_states[static_cast<size_t>(element)];
    s.text.set = true;
    s.text.value.clear();
}

void Screen::setTextAlpha(int element, float alpha) {
    m_states[static_cast<size_t>(element)].text.alpha = alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha;
}

void Screen::setFontColour(int element, uint32_t argb) {
    State& s = m_states[static_cast<size_t>(element)];
    s.text.fontColours = true;
    s.text.font = argb;
    s.text.high = argb;
}

void Screen::setFixedAdvance(int element, int advance) {
    const Element& e = m_window.elements[static_cast<size_t>(element)];
    for (const char* line : {"Font", "HighFont"}) {
        const Property* fp = e.find(line);
        if (fp == nullptr || fp->values.empty()) continue;
        if (Font* f = font(fp->values[0])) {
            f->fixed = true;
            f->fixedAdvance = advance;
        }
    }
}

void Screen::setText(const std::string& element, const std::wstring& text) {
    const int i = indexOf(element);
    if (i >= 0) setText(i, text);
}

std::wstring Screen::textOf(const std::string& element) const {
    const int i = indexOf(element);
    if (i < 0) {
        return {};
    }
    const State& s = m_states[static_cast<size_t>(i)];
    if (s.text.set) {
        return s.text.value;
    }
    return caption(m_window.elements[static_cast<size_t>(i)]);
}

std::wstring Screen::caption(const Element& e) const {
    if (const Property* c = e.find("Caption"); c != nullptr && !c->values.empty()) {
        return m_texts.at(static_cast<size_t>(std::atoi(c->values[0].c_str())));
    }
    return {};
}

void Screen::setMultiline(const std::string& element, bool on) {
    const int i = indexOf(element);
    if (i >= 0) {
        m_states[static_cast<size_t>(i)].text.multiline = on;
    }
}

void Screen::setScrolling(const std::string& element, bool clip, bool scroll) {
    const int i = indexOf(element);
    if (i < 0) return;
    State& s = m_states[static_cast<size_t>(i)];
    s.text.clip = clip;
    s.text.autoScroll = scroll;
    s.text.scroll = 0.0f;
}

void Screen::setFontColours(const std::string& element, uint32_t font, uint32_t high) {
    const int i = indexOf(element);
    if (i >= 0) {
        State& s = m_states[static_cast<size_t>(i)];
        s.text.fontColours = true;
        s.text.font = font;
        s.text.high = high;
    }
}

void Screen::setFont(const std::string& element, const std::string& font) {
    const int i = indexOf(element);
    if (i >= 0) {
        m_states[static_cast<size_t>(i)].text.fontName = font;
    }
}

std::string Screen::fontOf(const std::string& element) const {
    const int i = indexOf(element);
    if (i < 0) {
        return {};
    }
    if (!m_states[static_cast<size_t>(i)].text.fontName.empty()) {
        return m_states[static_cast<size_t>(i)].text.fontName;
    }
    const Property* fp = m_window.elements[static_cast<size_t>(i)].find("Font");
    return fp != nullptr && !fp->values.empty() ? fp->values[0] : std::string();
}

Screen::Font* Screen::font(const std::string& name) {
    const uint32_t cls = resolutionClass(m_device.width(), m_device.height());
    const std::string key = name + "_" + std::to_string(cls);
    const auto found = m_fonts.find(key);
    if (found != m_fonts.end()) {
        return found->second.get();
    }
    auto f = std::make_unique<Font>();
    const GameFiles files(std::filesystem::current_path());
    const auto bytes = files.read(std::filesystem::path("ui") / "fonts" / (key + ".d8f"));
    if (!bytes || !f->data.parse(*bytes)) {
        log::warning("ui: font {} not loaded", key);
        m_fonts[key] = nullptr;
        return nullptr;
    }
    for (const D8f::Page& page : f->data.pages) {
        std::vector<uint32_t> pixels(page.indices.size());
        for (size_t i = 0; i < pixels.size(); ++i) {
            pixels[i] = f->data.palette[page.indices[i]];
        }
        render::TextureData data;
        data.format = render::PixelFormat::A8R8G8B8;
        data.width = page.width;
        data.height = page.height;
        data.levels = 1;
        data.pixels = reinterpret_cast<const uint8_t*>(pixels.data());
        data.size = pixels.size() * 4;
        f->pages.push_back(m_device.createTexture(data));
    }
    f->samplers.resize(f->pages.size());
    // The ascent and descent over all glyphs (from -1024) make the line height (0x0051E070).
    f->ascent = -1024;
    f->descent = -1024;
    for (const D8f::Glyph& g : f->data.glyphs) {
        f->ascent = std::max(f->ascent, -g.yOffset);
        f->descent = std::max(f->descent, static_cast<int>(g.height) + g.yOffset);
    }
    Font* raw = f.get();
    m_fonts[key] = std::move(f);
    return raw;
}

// The width of a text in pixels (0x0051E690): a space steps by the space advance, '_' by nothing, other characters
// by their glyph's advance (the font's last glyph for missing ones).
std::wstring Screen::fitTextWithin(const std::string& element, const std::wstring& text, int most) {
    const int i = indexOf(element);
    if (i < 0) return text;
    const Element& e = m_window.elements[static_cast<size_t>(i)];
    const Property* fp = e.find("Font");
    Font* f = fp != nullptr && !fp->values.empty() ? font(fp->values[0]) : nullptr;
    if (f == nullptr) return text;
    const auto wide = [&](const std::wstring& s) { return static_cast<uint32_t>(textWidth(*f, s)); };
    if (wide(text) <= static_cast<uint32_t>(most)) return text;
    std::wstring kept = text;
    std::wstring shown;
    do {
        if (kept.empty()) break;
        kept.pop_back();
        shown = kept + L"...";
    } while (wide(shown) > static_cast<uint32_t>(most));
    return shown;
}

std::wstring Screen::fitText(const std::string& element, const std::wstring& text) {
    const int i = indexOf(element);
    if (i < 0) return text;
    const Element& e = m_window.elements[static_cast<size_t>(i)];
    const Property* fp = e.find("Font");
    Font* f = fp != nullptr && !fp->values.empty() ? font(fp->values[0]) : nullptr;
    if (f == nullptr) return text;
    const State& s = m_states[static_cast<size_t>(i)];
    const auto most = x87::chop(s.w * static_cast<float>(m_device.width()));
    if (static_cast<int>(textWidth(*f, text)) <= most) return text;
    std::wstring kept = text;
    std::wstring shown;
    do {
        if (kept.empty()) break;
        kept.pop_back();
        shown = kept + L"...";
    } while (static_cast<int>(textWidth(*f, shown)) > most);
    return shown;
}

std::wstring Screen::fitTextEnd(const std::string& element, const std::wstring& text, int most) {
    const int i = indexOf(element);
    if (i < 0) return text;
    const Element& e = m_window.elements[static_cast<size_t>(i)];
    const Property* fp = e.find("Font");
    Font* f = fp != nullptr && !fp->values.empty() ? font(fp->values[0]) : nullptr;
    if (f == nullptr) return text;
    const auto wide = [&](const std::wstring& s) { return static_cast<uint32_t>(textWidth(*f, s)); };
    if (wide(text) <= static_cast<uint32_t>(most)) return text;
    std::wstring shown;
    for (size_t k = 1; k < text.size(); ++k) {
        shown = L"..." + text.substr(k);
        if (wide(shown) <= static_cast<uint32_t>(most)) break;
    }
    return shown;
}

int Screen::pixelWidth(const std::string& element) const {
    const int i = indexOf(element);
    if (i < 0) return -1;
    const float w = m_states[static_cast<size_t>(i)].w * static_cast<float>(m_device.width()) * 1.0f;
    return static_cast<int>(w);
}

int Screen::pixelWidthNearest(const std::string& element) const {
    const int i = indexOf(element);
    if (i < 0) return -1;
    const float w = m_states[static_cast<size_t>(i)].w * static_cast<float>(m_device.width()) * 1.0f;
    return static_cast<int>(std::nearbyint(w));
}

void Screen::drawFreeText(const std::string& fontName, const std::wstring& text, float x, float y, uint32_t colour) {
    Font* f = font(fontName);
    if (f == nullptr) return;
    drawString(f, colour, text, x, y + static_cast<float>(f->ascent));
}

float Screen::freeTextWidth(const std::string& fontName, const std::wstring& text) {
    const Font* f = font(fontName);
    return f != nullptr ? textWidth(*f, text) : 0.0f;
}

float Screen::textWidth(const Font& f, const std::wstring& text) const {
    float w = 0.0f;
    for (wchar_t c : text) {
        if (c == L' ') {
            w += static_cast<float>(f.data.spaceAdvance);
        } else if (c != L'_') {
            if (const D8f::Glyph* g = f.data.find(c)) {
                w += static_cast<float>(f.fixed ? f.fixedAdvance : static_cast<int>(g->advance));
            }
        }
    }
    return w;
}

// A line of text at a baseline (0x005186C0, the font's draw 0x0051E740): the pen starts at whole pixels; glyphs keep
// their pixel size; a space steps by the space advance, the last character by its width. With a modifier each
// character is taken at its place in the line (the spaces counted) against the modifier's place (+0x08): nearer than
// its reach (+0x14) the glyph grows by the modifier's growth (+0x0C) x the nearness (1 - distance / reach) on each
// side and its alpha is 1 - the nearness, no less than 0 and no more than the colour's (x 255 cut toward zero).
void Screen::drawString(Font* f, uint32_t colour, const std::wstring& text, float x, float baseline,
                        const TextModifier* modifier) {
    float penX = std::trunc(x);
    const float penY = std::trunc(baseline);
    const float own = static_cast<float>(colour >> 24) * 0.003921569f;  // [0x006FF918]
    float place = 0.0f;
    for (size_t i = 0; i < text.size(); ++i, place += 1.0f) {
        const wchar_t c = text[i];
        if (c == L' ') {
            penX += static_cast<float>(f->data.spaceAdvance);
            continue;
        }
        const D8f::Glyph* g = f->data.find(c);
        if (g == nullptr) {
            continue;
        }
        float near = 0.0f;
        uint32_t glyphColour = colour;
        if (modifier != nullptr) {
            const float distance = std::fabs(place - modifier->place);
            if (distance < modifier->reach) {
                near = 1.0f - distance / modifier->reach;
                float a = 1.0f - near;
                a = a < 0.0f ? 0.0f : (a > own ? own : a);
                glyphColour = withAlpha(colour, alphaByte(a));
            }
        }
        const float grow = (modifier != nullptr ? modifier->grow : 0.0f) * near;
        if (g->page < f->pages.size()) {
            const float gx = penX + static_cast<float>(g->xOffset) - grow;
            const float gy = penY + static_cast<float>(g->yOffset) - grow;
            const float gw = static_cast<float>(g->width) + grow + grow;
            const float gh = static_cast<float>(g->height) + grow + grow;
            const render::QuadVertex corners[4] = {
                {gx, gy, g->u0, g->v0, glyphColour},
                {gx + gw, gy, g->u1, g->v0, glyphColour},
                {gx, gy + gh, g->u0, g->v1, glyphColour},
                {gx + gw, gy + gh, g->u1, g->v1, glyphColour},
            };
            render::applySampler(m_device, f->samplers[g->page], 0);
            m_device.drawQuad(corners, f->pages[g->page], render::Blend::Alpha, true);
        }
        penX += static_cast<float>(f->fixed                ? f->fixedAdvance
                                   : i + 1 < text.size() ? static_cast<int>(g->advance)
                                                         : static_cast<int>(g->width));
    }
}

// A text or a button (0x005B1CB0): the box, then the caption in Font (in HighFont for the highlighted button). One
// line is centred on the box's middle (baseline 0.325 of the line height below it); a multiline text starts a line
// height below the top and breaks at line feeds and at spaces where the line gets wider than the box. Align Left /
// Right keep 2 pixels from the box's side, Centre (the default) centres. Shadowed adds a black copy 3 pixels down
// and right. The colour's alpha is the text's (+0x130, 1 unless the code sets it; a highlighted text's below 0.2
// becomes 1); with a transition, while a FADEIN or GROW runs, the transition's, and then no more than the colour's
// own.
void Screen::drawText(const Element& e, State& s, bool highlighted) {
    const std::wstring text = s.text.set ? s.text.value : caption(e);
    if (text.empty()) {
        return;
    }
    const Property* fp = e.find("Font");
    if (highlighted && e.has("HighFont")) {
        fp = e.find("HighFont");
    }
    if (fp == nullptr || fp->values.empty()) {
        return;
    }
    const uint32_t own = s.text.fontColours ? (highlighted ? s.text.high : s.text.font) : argbOf(fp->values, 1);
    Font* f = font(s.text.fontName.empty() ? fp->values[0] : s.text.fontName);
    if (f == nullptr) {
        return;
    }
    // 0x005B1E03: highlighted in a HighFont, while the manager's switch waits (mode 2) the alpha is +0x134, falling by
    // 3 x the frame time and back to 1 below 0.2; else +0x130, back to 1 below 0.2. Without the highlight or the
    // HighFont +0x130 as it is.
    float alpha = s.text.alpha;
    if (highlighted && e.has("HighFont")) {
        if (m_switchWaiting) {
            s.switchAlpha = s.switchAlpha - m_drawTime * 3.0f;
            if (s.switchAlpha < 0.2f) s.switchAlpha = 1.0f;
            alpha = s.switchAlpha;
        } else {
            if (s.text.alpha < 0.2f) s.text.alpha = 1.0f;
            alpha = s.text.alpha;
        }
    }
    // 0x005B1E79: highlighted in a HighFont the text's modifier steps by the frame time (the window manager's) with
    // the text's length and is given to the line's drawing (0x005B2390, below).
    const TextModifier* modifier = nullptr;
    if (highlighted && e.has("HighFont")) {
        s.modifier.step(m_drawTime, static_cast<float>(text.size()));
        modifier = &s.modifier;
    }
    uint32_t colour = withAlpha(own, alphaByte(alpha));
    if (s.hasTransition) {
        const Transition& t = s.transition;
        if (!t.done() && (t.type == Transition::kFadeIn || t.type == Transition::kGrow)) {
            colour = withAlpha(colour, alphaByte(t.alpha));
        }
        if ((colour >> 24) > (own >> 24)) {
            colour = withAlpha(colour, static_cast<uint8_t>(own >> 24));
        }
    }
    if ((colour >> 24) == 0) {
        return;
    }
    float x0, y0, x1, y1;
    elementRect(e, s, x0, y0, x1, y1);
    const float offsetX = s.hasTransition ? s.transition.offsetX : 0.0f;
    const float offsetY = s.hasTransition ? s.transition.offsetY : 0.0f;
    const int align = alignmentOf(e);
    const bool shadow = flag(e, "Shadowed");
    const float lineHeight = static_cast<float>(f->height());

    auto line = [&](const std::wstring& l, float baseline) {
        const float w = textWidth(*f, l);
        float x = align == kAlignLeft ? x0 + 2.0f : align == kAlignRight ? x1 - 2.0f - w : (x0 + x1) * 0.5f - w * 0.5f;
        x += offsetX;
        baseline += offsetY;
        if (baseline <= -50.0f || baseline >= static_cast<float>(m_device.height()) + 50.0f) {
            return;
        }
        // 0x005B2390: Shadowed, the black copy and the text, neither through the modifier. Else with the modifier
        // (highlighted in a HighFont) two copies the modifier's spread (+0x1C) to the right and to the left, their
        // alpha no more than its +0x20, after its start and place are set to 0, its growth to (1 - its time) x 10 and
        // its reach to 200; then the text itself, not through it, a button's at the least of its alpha and |pulse| +
        // 0.25 (0.25 .. 1): it blinks (a text's not).
        if (shadow) {
            drawString(f, colour & 0xFF000000, l, x + 3.0f, baseline + 3.0f, nullptr);
            drawString(f, colour, l, x, baseline, nullptr);
            return;
        }
        uint32_t main = colour;
        if (modifier != nullptr) {
            TextModifier& m = s.modifier;
            const float a = static_cast<float>(colour >> 24) * 0.003921569f;  // [0x006FF918]
            const uint32_t echo = a > m.alpha ? withAlpha(colour, alphaByte(m.alpha)) : colour;
            m.start = 0.0f;
            m.place = 0.0f;
            m.grow = (1.0f - m.time) * 10.0f;
            m.reach = 200.0f;
            const float right = x + m.spread;
            drawString(f, echo, l, right, baseline, &m);
            drawString(f, echo, l, right - (m.spread + m.spread), baseline, &m);
            float pulse = std::fabs(m.pulse) + 0.25f;  // [0x006FFBEC]
            if (pulse < 0.25f) {
                pulse = 0.25f;
            } else if (!(pulse <= 1.0f)) {
                pulse = 1.0f;
            }
            if (a > pulse && e.kind != ElementKind::Text) main = withAlpha(colour, alphaByte(pulse));
        }
        drawString(f, main, l, x, baseline, nullptr);
    };

    if (!s.text.multiline) {
        line(text, (y0 + y1) * 0.5f + lineHeight * 0.325f);
        return;
    }
    // Characters are added to the line; CR is dropped, LF ends the line, and once the line is wider than the box it
    // is cut at its last space.
    const int boxWidth = static_cast<int>(e.w * static_cast<float>(m_device.width()));
    float baseline = y0 + lineHeight;
    if (s.text.autoScroll) {
        baseline = y1 + lineHeight;
        s.text.scroll = s.text.scroll + m_drawTime * s.text.scrollSpeed;
        baseline -= s.text.scroll;
    }
    const float first = baseline;
    if (s.text.clip) {
        const render::ClipRect rect{x0, y0, x1, y1};
        m_device.setClip(&rect);
    }
    std::wstring current;
    int lastSpace = -1;
    for (wchar_t c : text) {
        if (c == L'\r') {
            continue;
        }
        if (c == L'\n') {
            line(current, baseline);
            baseline += lineHeight;
            current.clear();
            lastSpace = -1;
            continue;
        }
        current.push_back(c);
        if (c == L' ') {
            lastSpace = static_cast<int>(current.size()) - 1;
        }
        if (static_cast<int>(textWidth(*f, current)) > boxWidth && lastSpace > -1) {
            const std::wstring head = current.substr(0, static_cast<size_t>(lastSpace));
            const std::wstring rest = current.substr(static_cast<size_t>(lastSpace) + 1);
            line(head, baseline);
            baseline += lineHeight;
            current = rest;
            lastSpace = -1;
        }
    }
    line(current, baseline);
    if (s.text.clip) m_device.setClip(nullptr);
    if (s.text.autoScroll) {
        s.text.scrollEnd = (y1 - y0) + (baseline + lineHeight - first);
        if (s.text.scrollEnd < s.text.scroll) s.text.scroll = 0.0f;
    }
}

}  // namespace oj::ui
