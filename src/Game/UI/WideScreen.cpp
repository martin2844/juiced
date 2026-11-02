#include "Game/UI/WideScreen.h"

#include "Game/UI/UiInternal.h"
#include "Game/UI/UiScreen.h"
#include "Render/Display.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace oj::ui {

namespace {

bool is(const std::string& a, const char* b) {
    return _stricmp(a.c_str(), b) == 0;
}

bool fullWidth(const Element& e) {
    return e.x <= 0.01f && e.x + e.w >= 0.99f;
}

bool fullHeight(const Element& e) {
    return e.y <= 0.01f && e.y + e.h >= 0.99f;
}

// A side of a box at the window's edge (within 0.01 of it), the box on the window (not parked past it) and at least
// 0.1 across that way (a bar, a bar's filler piece, a banner's half, a pattern; not a button's picture or an icon).
constexpr float kEdge = 0.01f;
constexpr float kEdgeBox = 0.1f;

// A full screen picture or movie played once covers the screen no wider than a 16:9 one of its height (its middle
// kept on the widest): the 4:3 frame scaled by 4/3 at most.
constexpr float kMostCover = (16.0f / 9.0f) / (4.0f / 3.0f);

// One axis of a placement: the box from lo to lo + size of the window's fractions, its low side to the screen's start
// and its high side to the screen's end where asked, else where the layout puts them (scale and offset: pixels = lo x
// scale + offset).
void edgeAxis(float lo, float size, float layOffset, float laySize, float screen, bool low, bool high, float& scale,
              float& offset) {
    scale = laySize;
    offset = layOffset;
    if ((!low && !high) || size <= 0.0f) return;
    const float hi = lo + size;
    const float from = low ? 0.0f : layOffset + lo * laySize;
    const float to = high ? screen : layOffset + hi * laySize;
    scale = (to - from) / size;
    offset = from - lo * scale;
}

// The elements that may reach the screen's edges: the pictures, the movies (the menus' looped backgrounds: hub_01,
// career_02, customrace_01; the logos and the outro played once, no wider than 16:9) and the 3D views. A movie less
// than the whole window (the tutorials, the characters') keeps its proportions, as a text, a button or a menu stays
// with the hints and texts it goes with.
bool reachesEdges(const Element& e) {
    return e.kind == ElementKind::Box || e.kind == ElementKind::View3d ||
           (e.kind == ElementKind::Movie && (e.has("Looped") || (fullWidth(e) && fullHeight(e))));
}

// The pictures that stretch across with a menu's layout: the bars, panels and patterns (the banners' halves, the
// hints' sweep and lines, the menus' bars, the silver lines, the tribal swirls, the meters' and sliders' tracks, the
// colour bar, the graphs' grids, the calendar's day cursor over its stretched days).
bool patternTexture(const std::string& name) {
    static constexpr const char* kPatterns[] = {
        "ui_test_pan",   "ui_topsweep",     "don_ui_",          "ui_blue_yellow",     "ui_tribal_",
        "ui_arcade_red", "ui_green_pan",    "ui_hud_pan",       "ui_hud_box",         "ui_msgbox_select",
        "ui_colourbar",  "ui_stats_tile",   "ui_gridwalk_select", "ui_plainbox",      "ui_graph",
        "ui_pan5_",      "ui_progress_bar", "ui_replaypos",     "ui_modchange_base",  "ui_slider_bar",
        "ui_respect_bar", "ui_opts_slider",  "ui_calendar_select",
    };
    for (const char* pattern : kPatterns) {
        if (_strnicmp(name.c_str(), pattern, std::strlen(pattern)) == 0) return true;
    }
    return false;
}

}  // namespace

bool framedWindow(const std::string& window) {
    for (const char* name : {"hudcolourmenu", "cmsmobilephone", "cmsmobilephonemenu"}) {
        if (is(window, name)) return true;
    }
    return false;
}

bool hudWindow(const std::string& window) {
    for (const char* name : {"hud_race", "hud_solo", "hud_sprint", "hud_cruise", "hud_split", "hud_spectator"}) {
        if (is(window, name)) return true;
    }
    return false;
}

Anchor hudAnchor(const std::string& window, const Element& e, bool down) {
    if (!down) {
        static constexpr struct {
            const char* window;
            const char* element;
            Anchor anchor;
        } kNamed[] = {
            {"hud_cruise", "DIGITAL_BG_BONUS", Anchor::Left}, {"hud_cruise", "REGIONMESSAGE", Anchor::Left},
            {"hud_cruise", "BONUS_TEXT", Anchor::Left},       {"hud_spectator", "RP_NEXTCARTEXT", Anchor::Left},
            {"hud_race", "DPADCONTROL01", Anchor::Centre},    {"hud_race", "DPADCONTROL02", Anchor::Centre},
            {"hud_race", "DPADCONTROL03", Anchor::Centre},    {"hud_sprint", "DPADCONTROL01", Anchor::Centre},
            {"hud_sprint", "DPADCONTROL02", Anchor::Centre},  {"hud_sprint", "DPADCONTROL03", Anchor::Centre},
        };
        for (const auto& n : kNamed) {
            if (is(window, n.window) && is(e.name, n.element)) return n.anchor;
        }
    }
    const float middle = down ? e.y + e.h * 0.5f : e.x + e.w * 0.5f;
    return middle < 0.3f ? Anchor::Left : middle > 0.7f ? Anchor::Right : Anchor::Centre;
}

bool pictureElement(const std::string&, const Element& e) {
    if (e.kind != ElementKind::Box) return false;
    const Property* t = e.find("Texture");
    if (t == nullptr || t->values.empty()) return false;
    for (const char* name : {"UI_SPLASH", "ui_background_01", "ui_background_02", "UI_ARCADE_LOADING", "garagedoor"}) {
        if (is(t->values[0], name)) return true;
    }
    return false;
}

bool keepsShape(const Element& e, const std::string& texture) {
    switch (e.kind) {
    case ElementKind::Movie:
    case ElementKind::Dial: return true;
    case ElementKind::Box: return !texture.empty() && !patternTexture(texture);
    default: return false;
    }
}

std::vector<int> pictureHolders(const Window& window) {
    const std::vector<Element>& all = window.elements;
    std::vector<int> holders(all.size(), -1);
    constexpr float kGrown = 0.05f;
    for (size_t c = 0; c < all.size(); ++c) {
        const Element& h = all[c];
        const Property* t = h.find("Texture");
        const std::string texture = t != nullptr && !t->values.empty() ? t->values[0] : std::string();
        if (!keepsShape(h, texture) || h.w <= 0.0f || h.h <= 0.0f || h.w > 0.6f || h.h > 0.8f) continue;
        for (size_t i = 0; i < all.size(); ++i) {
            const Element& e = all[i];
            // Of two of the same size the first holds the second (a ticked box over its cross, the hint's arrows).
            if (i == c || e.w * e.h > h.w * h.h || (e.w * e.h == h.w * h.h && i < c)) continue;
            const float mx = e.x + e.w * 0.5f, my = e.y + e.h * 0.5f;
            if (e.x < h.x - kGrown || e.x + e.w > h.x + h.w + kGrown || e.y < h.y - kGrown ||
                e.y + e.h > h.y + h.h + kGrown || mx < h.x || mx > h.x + h.w || my < h.y || my > h.y + h.h) {
                continue;
            }
            const int now = holders[i];
            if (now < 0 || all[static_cast<size_t>(now)].w * all[static_cast<size_t>(now)].h < h.w * h.h) {
                holders[i] = static_cast<int>(c);
            }
        }
    }
    return holders;
}

std::vector<bool> columnHeads(const Window& window) {
    const std::vector<Element>& all = window.elements;
    std::vector<bool> heads(all.size(), false);
    for (size_t i = 0; i < all.size(); ++i) {
        const Element& e = all[i];
        if (e.kind != ElementKind::Text || detail::alignmentOf(e) != detail::kAlignRight) continue;
        for (const Element& m : all) {
            if (m.kind == ElementKind::Menu && std::fabs(m.x - e.x) < 0.01f && std::fabs(m.w - e.w) < 0.01f &&
                detail::alignmentOf(m) == detail::kAlignLeft) {
                heads[i] = true;
                break;
            }
        }
    }
    return heads;
}

void Screen::movieUnderlay(size_t index) {
    const render::Display d = render::display(m_device.width(), m_device.height());
    if (d.stock || index >= m_window.elements.size()) return;
    const Element& e = m_window.elements[index];
    if (e.kind != ElementKind::Movie || e.has("Looped") || !fullWidth(e) || !fullHeight(e)) return;
    const uint32_t black[4] = {0xFF000000u, 0xFF000000u, 0xFF000000u, 0xFF000000u};
    quad(0.0f, 0.0f, d.width, d.height, black, 0, 0.0f, 1.0f, 0.0f, 1.0f, false);
}

uint32_t Screen::frameWidth() const {
    return static_cast<uint32_t>(render::display(m_device.width(), m_device.height()).frameW + 0.5f);
}

bool Screen::pixelRect(const std::string& element, float& x0, float& y0, float& x1, float& y1) const {
    const int i = indexOf(element);
    if (i < 0) return false;
    const size_t k = static_cast<size_t>(i);
    elementRect(m_window.elements[k], m_states[k], x0, y0, x1, y1);
    return true;
}

// The box of an element (drawBox) in pixels: a text's, a button's, a menu's or a ticker's box with a side at the
// window's edge out to the screen's (the city map's districts' bar), its contents where placement() puts them.
void Screen::boxRect(const Element& e, const State& s, float& x0, float& y0, float& x1, float& y1) const {
    const bool contents = e.kind == ElementKind::Text || e.kind == ElementKind::Button ||
                          e.kind == ElementKind::Menu || e.kind == ElementKind::Ticker;
    if (!contents) {
        elementRect(e, s, x0, y0, x1, y1);
        return;
    }
    const Placement p = edgePlacement(static_cast<size_t>(&s - m_states.data()));
    x0 = s.x * p.sx + p.ox;
    y0 = s.y * p.sy + p.oy;
    x1 = (s.x + s.w) * p.sx + p.ox;
    y1 = (s.y + s.h) * p.sy + p.oy;
}

// The placement of a box reaching the screen's edges (Screen::edgePlacement): a side at the window's edge taken to
// the screen's, the HUD's only across the whole width or height; the other where the layout puts it.
Screen::Placement Screen::edgePlacement(size_t index) const {
    const render::Display d = render::display(m_device.width(), m_device.height());
    Placement p{d.width, d.height, 0.0f, 0.0f};
    if (d.stock || index >= m_window.elements.size()) return p;
    const Element& e = m_window.elements[index];
    // A plain bar across the whole window (no picture: a colour's fill) reaches the screen's top or bottom however thin
    // (the main menu's black strip over its top, 0.042 high: on 5:4 the background movie showed above it).
    const bool plainBar = e.kind == ElementKind::Box && fullWidth(e) && !e.has("Texture") && !e.has("Animated");
    const bool across = e.w >= kEdgeBox && e.x < 1.0f - kEdge && e.x + e.w > kEdge;
    const bool down = (e.h >= kEdgeBox || plainBar) && e.y < 1.0f - kEdge && e.y + e.h > kEdge;
    bool left = across && e.x <= kEdge, right = across && e.x + e.w >= 1.0f - kEdge;
    bool top = down && e.y <= kEdge, bottom = down && e.y + e.h >= 1.0f - kEdge;
    if (m_hud) {
        left = right = left && right;
        top = bottom = top && bottom;
    }
    const bool framed = m_hud || m_framed;
    edgeAxis(e.x, e.w, framed ? d.frameX : d.menuX, framed ? d.frameW : d.menuW, d.width, left, right, p.sx, p.ox);
    edgeAxis(e.y, e.h, d.frameY, d.frameH, d.height, top, bottom, p.sy, p.oy);
    return p;
}

// The element's placement (Screen::Placement): the stock's whole screen; else a menu's laid out across its zone (at
// most 16:9) and down the 4:3 frame, a picture keeping its shape (an icon, a logo, a portrait, a movie, a dial: the
// frame's size, its middle where the layout puts the box's) with the elements it holds (pictureHolders), the framed
// windows' in the frame (framedWindow); a picture, a looped movie or a 3D view filling the whole
// window stretched over the screen without a texture, a texture or movie scaled to cover the screen in its 4:3 shape
// and cut at two edges, a splash or loading picture and a movie played once no wider than 16:9; one at an edge of a
// menu's window (a bar, a banner's half: its sides at the window's edges) out to the screen's edges (edgePlacement);
// a HUD element in the frame moved to its anchors (one across the whole width stretched over it).
Screen::Placement Screen::placement(size_t index) const {
    const render::Display d = render::display(m_device.width(), m_device.height());
    Placement p{d.width, d.height, 0.0f, 0.0f};
    if (d.stock || index >= m_window.elements.size()) return p;
    const Element& e = m_window.elements[index];
    const float layX = m_hud || m_framed ? d.frameX : d.menuX;
    const float layW = m_hud || m_framed ? d.frameW : d.menuW;
    p = {layW, d.frameH, layX, d.frameY};
    const bool wide = fullWidth(e), high = fullHeight(e);
    if (reachesEdges(e)) {
        if (wide && high) {
            const bool textured = e.kind == ElementKind::Movie || e.has("Texture") || e.has("Animated");
            if (e.kind == ElementKind::View3d || !textured) return {d.width, d.height, 0.0f, 0.0f};
            // Cover: the frame scaled until both of its sides reach the screen's.
            float k = d.width / d.frameW > d.height / d.frameH ? d.width / d.frameW : d.height / d.frameH;
            if (pictureElement(m_name, e) || (e.kind == ElementKind::Movie && !e.has("Looped"))) {
                k = std::min(k, kMostCover);
            }
            p.sx = d.frameW * k;
            p.sy = d.frameH * k;
            p.ox = (d.width - p.sx) * 0.5f;
            p.oy = (d.height - p.sy) * 0.5f;
            return p;
        }
        const Placement edges = edgePlacement(index);
        if (edges.sx != p.sx || edges.ox != p.ox || edges.sy != p.sy || edges.oy != p.oy) return edges;
    }
    if (m_hud) {
        switch (hudAnchor(m_name, e, false)) {
        case Anchor::Left: p.ox = d.zoneX; break;
        case Anchor::Right: p.ox = d.zoneX + d.zoneW - d.frameW; break;
        case Anchor::Centre: break;
        }
        if (d.frameH < d.height) {
            switch (hudAnchor(m_name, e, true)) {
            case Anchor::Left: p.oy = 0.0f; break;
            case Anchor::Right: p.oy = d.height - d.frameH; break;
            case Anchor::Centre: break;
            }
        }
        return p;
    }
    if (index < m_heads.size() && m_heads[index] && index < m_states.size()) {
        // A column's header: from the column's start, its 4:3 width.
        p.ox = layX + m_states[index].x * (layW - d.frameW);
        p.sx = d.frameW;
        return p;
    }
    // The picture keeping its shape: the element's own or the one holding it.
    const int holder = index < m_holders.size() ? m_holders[index] : -1;
    const size_t shaped = holder >= 0 ? static_cast<size_t>(holder) : index;
    if (shaped < m_states.size()) {
        const State& s = m_states[shaped];
        const Element& picture = m_window.elements[shaped];
        const Property* t = picture.find("Texture");
        static const std::string kNone;
        const std::string& texture = s.hasTexture                           ? s.texture
                                     : t != nullptr && !t->values.empty() ? t->values[0]
                                                                          : kNone;
        if (holder >= 0 || keepsShape(picture, texture)) {
            // Its middle where the layout puts the box's (its place now: the code moves some), the frame's size.
            const float middle = s.x + s.w * 0.5f;
            p.ox = layX + middle * layW - middle * d.frameW;
            p.sx = d.frameW;
        }
    }
    return p;
}

}  // namespace oj::ui
