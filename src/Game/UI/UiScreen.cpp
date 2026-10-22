// A window of the interface: its file loaded, its elements found, shown, hidden and highlighted, and its frame and draw
// over the elements' kinds.

#include "Game/UI/UiScreen.h"
#include "Game/UI/UiInternal.h"

#include "Core/Base/Log.h"

#include <filesystem>

namespace oj::ui {

using namespace detail;

namespace {

// The corner of a VertexColour or MeterVertexColour line: TOPLEFT (also any other word) 0, TOPRIGHT 1, BOTLEFT 2,
// BOTRIGHT 3.
int cornerOf(const std::string& word) {
    return _stricmp(word.c_str(), "TOPRIGHT") == 0   ? 1
           : _stricmp(word.c_str(), "BOTLEFT") == 0  ? 2
           : _stricmp(word.c_str(), "BOTRIGHT") == 0 ? 3
                                                     : 0;
}

int transitionType(const std::string& name) {
    static const std::pair<const char*, int> kTypes[] = {
        {"FADEIN", Transition::kFadeIn},        {"SLIDE_LR", Transition::kSlideLR},
        {"SLIDE_RL", Transition::kSlideRL},     {"SLIDE_TB", Transition::kSlideTB},
        {"SLIDE_BT", Transition::kSlideBT},     {"SLIDE_LRBT", Transition::kSlideLRBT},
        {"SLIDE_RLBT", Transition::kSlideRLBT}, {"SLIDE_LRTB", Transition::kSlideLRTB},
        {"SLIDE_RLTB", Transition::kSlideRLTB}, {"FALSH", Transition::kFlash},
        {"GROW", Transition::kGrow},            {"GROW_HORI", Transition::kGrowHori},
        {"GROW_VERT", Transition::kGrowVert},   {"GROW_UP", Transition::kGrowUp},
        {"GROW_DOWN", Transition::kGrowDown},   {"GROW_LEFT", Transition::kGrowLeft},
        {"GROW_RIGHT", Transition::kGrowRight},
    };
    for (const auto& [n, t] : kTypes) {
        if (_stricmp(name.c_str(), n) == 0) return t;
    }
    return Transition::kFadeIn;
}

}  // namespace

uint32_t resolutionClass(uint32_t width, uint32_t height) {
    uint32_t c = 640;
    if (width >= 800 && height >= 600) c = 800;
    if (width >= 1024 && height >= 768) c = 1024;
    if (width >= 1280 && height >= 960) c = 1280;
    if (width >= 1600 && height >= 1200) c = 1600;
    return c;
}

Screen::Screen(render::IRenderDevice& device, const Language& language)
    : sound(defaultSound), m_device(device), m_language(language) {}

Screen::~Screen() {
    for (const auto& [name, id] : m_textures) {
        pictureSamplers().erase(id);
        m_device.destroyTexture(id);
    }
    for (const auto& [name, font] : m_fonts) {
        if (font) {
            for (render::TextureId id : font->pages) {
                m_device.destroyTexture(id);
            }
        }
    }
}

bool Screen::load(const std::string& name, bool fromDisk) {
    m_loaded = false;
    m_name = name;
    m_current.clear();
    const std::filesystem::path windows = std::filesystem::path("ui") / "windows";
    if (!m_window.load(windows / (name + ".ui"))) {
        log::error("ui: {}.ui not found", name);
        m_states.clear();
        return false;
    }
    // The window's texts in the game's language, else the English ones (0x005AA660).
    const auto texts =
        windows / m_language.name / (name + "." + (fromDisk ? m_language.looseExtension : m_language.extension));
    if (!m_texts.load(texts) && !m_texts.load(windows / "english" / (name + ".eng"))) {
        log::warning("ui: no texts {}", texts.string());
    }
    m_states.assign(m_window.elements.size(), State{});
    m_order.resize(m_window.elements.size());
    for (size_t i = 0; i < m_order.size(); ++i) m_order[i] = i;
    m_views.clear();
    m_views.resize(m_window.elements.size());
    const float sw = static_cast<float>(m_device.width());
    const float sh = static_cast<float>(m_device.height());
    for (size_t i = 0; i < m_window.elements.size(); ++i) {
        const Element& e = m_window.elements[i];
        State& s = m_states[i];
        if (e.kind == ElementKind::Button && e.has("Highlight") && m_current.empty()) {
            m_current = e.name;
            s.highlighted = true;
        }
        // HighlightButton <name> (0x005AE150; no window has one): the window's button of the name (case aside, of
        // those made so far; none is hidden yet) becomes the highlighted one (0x005A9A40, 0x005A99F0), or none.
        if (const Property* p = e.find("HighlightButton"); p != nullptr && !p->values.empty()) {
            std::string found;
            for (size_t k = 0; k <= i; ++k) {
                const Element& b = m_window.elements[k];
                if (b.kind == ElementKind::Button && _stricmp(b.name.c_str(), p->values[0].c_str()) == 0) {
                    found = b.name;
                    break;
                }
            }
            m_current = found;
            for (size_t k = 0; k <= i; ++k) {
                if (m_window.elements[k].kind == ElementKind::Button) {
                    m_states[k].highlighted = _stricmp(m_window.elements[k].name.c_str(), found.c_str()) == 0;
                }
            }
        }
        // FontModifier <ONCE|LOOP|BOUNCE> <start> <growth> <speed> <reach> (0x005AC8C0; no window has one): a Text's
        // modifier (0x005B1B20) or another element's as a menu's (0x005AF0B0): the type 0, 1 or 2 (another word 0),
        // the start and the place the start, the growth, the speed and the reach (0 for a missing one), forward,
        // no spread, alpha 0, time and pulse 1. (Stock problem: an element neither Text nor Menu takes the menu's
        // record at its +0x10C; here only those two take it.)
        if (const Property* p = e.find("FontModifier"); p != nullptr && (e.kind == ElementKind::Text || e.kind == ElementKind::Menu)) {
            const auto word = [&](size_t k) { return k < p->values.size() ? p->values[k] : std::string(); };
            const auto value = [&](size_t k) { return static_cast<float>(std::atof(word(k).c_str())); };
            TextModifier m;
            m.type = _stricmp(word(0).c_str(), "LOOP") == 0     ? ModifierType::Loop
                     : _stricmp(word(0).c_str(), "BOUNCE") == 0 ? ModifierType::Bounce
                                                                : ModifierType::None;
            m.start = value(1);
            m.place = m.start;
            m.grow = value(2);
            m.speed = value(3);
            m.reach = value(4);
            s.modifier = m;
        }
        // Colour a r g b (0x005AC100) sets the four corners, VertexColour <corner> a r g b (0x005AC7A0) one;
        // Multiline (0x005ACB20) +0x128, Autoscroll (0x005ACB40) +0x140 and +0x13C with +0x144 0.
        for (const Property& p : e.properties) {
            if (_stricmp(p.name.c_str(), "Multiline") == 0) {
                s.text.multiline = true;
            } else if (_stricmp(p.name.c_str(), "Autoscroll") == 0) {
                s.text.autoScroll = true;
                s.text.clip = true;
                s.text.scroll = 0.0f;
            } else if (_stricmp(p.name.c_str(), "TickerSpeed") == 0) {
                // 0x005ACB70: the number (atof), 0 without one.
                s.ticker.speed = p.values.empty() ? 0.0f : static_cast<float>(std::atof(p.values[0].c_str()));
            } else if (p.name == "Colour") {
                s.corners.fill(argbOf(p.values, 0));
            } else if (p.name == "VertexColour" && !p.values.empty()) {
                s.corners[static_cast<size_t>(cornerOf(p.values[0]))] = argbOf(p.values, 1);
            } else if (e.kind == ElementKind::Movie && _stricmp(p.name.c_str(), "Movie") == 0) {
                // 0x005ADBB0: with a name the interface's movie is given back, the name kept, +0x100 1 and the
                // volume 1; then +0x100 0 in any case.
                if (!p.values.empty()) {
                    movie::System* m = movie::System::get();
                    if (m != nullptr) m->closeCurrent();
                    s.movie.name = p.values[0];
                    s.movie.volume = 1.0f;
                }
                s.movie.looped = false;
            } else if (e.kind == ElementKind::Movie && _stricmp(p.name.c_str(), "Looped") == 0) {
                s.movie.looped = true;  // 0x005ADC10
            } else if (e.kind == ElementKind::Movie && _stricmp(p.name.c_str(), "PlayFast") == 0) {
                s.movie.playFast = true;  // 0x005ADC30
            }
        }
        s.x = e.x, s.y = e.y, s.w = e.w, s.h = e.h;
        if (e.kind == ElementKind::Slider) {
            // SliderType HORIZONTAL | VERTICAL (0x005ADA10), SliderColour a r g b (0x005ADA70), KnobTexture <name>
            // (the knob's picture; SliderTexture is kept at +0xD8 and not drawn).
            if (const Property* t = e.find("SliderType"); t != nullptr && !t->values.empty()) {
                if (_stricmp(t->values[0].c_str(), "HORIZONTAL") == 0) {
                    s.slider.vertical = false;
                } else if (_stricmp(t->values[0].c_str(), "VERTICAL") == 0) {
                    s.slider.vertical = true;
                }
            }
            if (const Property* c = e.find("SliderColour")) s.slider.colour = argbOf(c->values, 0);
            if (const Property* k = e.find("KnobTexture"); k != nullptr && !k->values.empty()) {
                s.slider.knobTexture = k->values[0];
            }
        }
        if (e.kind == ElementKind::Dial) {
            // The Dial's defaults (0x005B5960), then NeedleTexture <name> a r g b (0x005ADC50) and NeedleData min max
            // pivotX pivotY length width tail (0x005ADD30).
            s.dial.needleTexture = "ui_gauge_needle";
            if (const Property* n = e.find("NeedleTexture"); n != nullptr && !n->values.empty()) {
                s.dial.needleTexture = n->values[0];
                s.dial.needleColour = argbOf(n->values, 1);
            }
            if (const Property* d = e.find("NeedleData")) {
                s.dial.min = number(d, 0, 0.0f);
                s.dial.max = number(d, 1, 0.0f);
                s.dial.pivotX = number(d, 2, 0.0f);
                s.dial.pivotY = number(d, 3, 0.0f);
                s.dial.needleLength = number(d, 4, 0.0f);
                s.dial.needleWidth = number(d, 5, 0.0f);
                s.dial.needleTail = number(d, 6, 0.0f);
            }
        }
        // Transition <type> <duration> <delay> [LOOP]
        if (const Property* t = e.find("Transition"); t != nullptr && !t->values.empty()) {
            s.hasTransition = true;
            Transition& tr = s.transition;
            tr.type = transitionType(t->values[0]);
            tr.duration = tr.remaining = number(t, 1, 0.0f);
            tr.delay = tr.delayLeft = number(t, 2, 0.0f);
            tr.loop = t->values.size() > 3 && _stricmp(t->values[3].c_str(), "LOOP") == 0;
            tr.width = sw;
            tr.height = sh;
            tr.step(0.001f);
        }
        if (e.kind == ElementKind::Menu) {
            buildMenu(i);
        }
        if (e.kind == ElementKind::View3d && m_viewFactory) {
            m_views[i] = m_viewFactory(e);
            // 0x005AB3C4: the window's name in capitals holding "HUD_" makes the view round (0x005B36D0's last word).
            std::string upper = name;
            for (char& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            if (m_views[i] && upper.find("HUD_") != std::string::npos) m_views[i]->setRound(true);
        }
    }
    // HelpText x y w h <font> a r g b on a menu (0x005AD8A0, 0x005AF6A0): its help ticker "help" (+0x184), not in the
    // window's list (no find reaches it); its box takes the menu's corner colours and outline, its font the colour,
    // the menu's transition made again for it without its loop (0x005B2970). The menu draws it.
    const size_t own = m_window.elements.size();
    for (size_t i = 0; i < own; ++i) {
        if (m_window.elements[i].kind != ElementKind::Menu) continue;
        const Property* h = m_window.elements[i].find("HelpText");
        if (h == nullptr) continue;
        Element t;
        t.kindName = "Ticker";
        t.kind = ElementKind::Ticker;
        t.name = "help";
        t.x = number(h, 0, 0.0f);
        t.y = number(h, 1, 0.0f);
        t.w = number(h, 2, 0.0f);
        t.h = number(h, 3, 0.0f);
        Property f{"Font", {}};
        for (size_t k = 4; k < h->values.size() && k < 9; ++k) f.values.push_back(h->values[k]);
        t.properties.push_back(std::move(f));
        if (const Property* o = m_window.elements[i].find("Outline")) t.properties.push_back(*o);
        State ts;
        ts.corners = m_states[i].corners;
        ts.x = t.x, ts.y = t.y, ts.w = t.w, ts.h = t.h;
        if (m_states[i].hasTransition) {
            const Transition& from = m_states[i].transition;
            ts.hasTransition = true;
            Transition& tr = ts.transition;
            tr.type = from.type;
            tr.duration = tr.remaining = from.duration;
            tr.delay = tr.delayLeft = from.delay;
            tr.loop = false;
            tr.width = sw;
            tr.height = sh;
            tr.step(0.001f);
        }
        m_states[i].menu.helpTicker = static_cast<int>(m_window.elements.size());
        m_window.elements.push_back(std::move(t));
        m_states.push_back(std::move(ts));
        m_views.emplace_back();
    }
    // The windows' enter (0x005A5F20): the menu named Menu takes the help of its item, every ticker of the window is
    // measured.
    for (size_t i = 0; i < own; ++i) {
        const Element& e = m_window.elements[i];
        if (e.kind == ElementKind::Menu && _stricmp(e.name.c_str(), "Menu") == 0) menuHelp(i);
        if (e.kind == ElementKind::Ticker) measureTicker(static_cast<int>(i));
    }
    log::info("ui: {} ({} elements, {} texts)", name, m_window.elements.size(), m_texts.size());
    m_loaded = true;
    return true;
}

const Element* Screen::element(const std::string& name) const {
    const int i = indexOf(name);
    return i < 0 ? nullptr : &m_window.elements[static_cast<size_t>(i)];
}

int Screen::indexOf(const std::string& name) const {
    for (size_t i : m_order) {
        if (_stricmp(m_window.elements[i].name.c_str(), name.c_str()) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int Screen::find(const std::string& name) const {
    return indexOf(name);
}

const std::string& Screen::nameOf(int element) const {
    return m_window.elements[static_cast<size_t>(element)].name;
}

void Screen::setHidden(int element, bool hidden) {
    m_states[static_cast<size_t>(element)].hidden = hidden;
}

bool Screen::hidden(int element) const {
    return m_states[static_cast<size_t>(element)].hidden;
}

// 0x005B2930: the highlight (+0x120), the alpha back to 1 (+0x130) and the text modifier started again (+0x108).
void Screen::setHighlighted(int element, bool on) {
    State& s = m_states[static_cast<size_t>(element)];
    s.highlighted = on;
    s.text.alpha = 1.0f;
    s.modifier.restart();
}

void Screen::setPlace(int element, float x, float y, float w, float h) {
    State& s = m_states[static_cast<size_t>(element)];
    s.placed = true;
    s.x = x, s.y = y, s.w = w, s.h = h;
}

void Screen::placeOf(int element, float& x, float& y, float& w, float& h) const {
    const State& s = m_states[static_cast<size_t>(element)];
    x = s.x, y = s.y, w = s.w, h = s.h;
}

void Screen::moveToEnd(int element) {
    const auto at = std::find(m_order.begin(), m_order.end(), static_cast<size_t>(element));
    if (at == m_order.end()) return;
    m_order.erase(at);
    m_order.push_back(static_cast<size_t>(element));
}

int Screen::addBox(const std::string& name, float x, float y, float w, float h) {
    Element e;
    e.kindName = "Box";
    e.kind = ElementKind::Box;
    e.name = name;
    e.x = x, e.y = y, e.w = w, e.h = h;
    m_window.elements.push_back(std::move(e));
    State s;
    s.x = x, s.y = y, s.w = w, s.h = h;
    m_states.push_back(std::move(s));
    m_views.emplace_back();
    m_order.push_back(m_window.elements.size() - 1);
    return static_cast<int>(m_window.elements.size() - 1);
}

void Screen::hideContaining(const std::string& part, bool animate) {
    for (size_t i = 0; i < m_window.elements.size(); ++i) {
        if (m_window.elements[i].name.find(part) == std::string::npos) {
            continue;
        }
        State& s = m_states[i];
        if (!animate || !s.hasTransition) {
            s.hidden = true;
        } else if (!s.transition.out) {
            Transition& t = s.transition;
            t.out = true;
            t.remaining = t.duration;
            t.progress = 0.0f;
            t.delayLeft = 0.0f;
        }
    }
}

bool Screen::hidden(const std::string& element) const {
    const int i = indexOf(element);
    return i < 0 || m_states[static_cast<size_t>(i)].hidden;
}

// A hidden element shown: its transition starts again (animate) or rests at its end.
void Screen::showState(State& s, bool animate) {
    if (!s.hidden) {
        return;
    }
    s.hidden = false;
    if (!s.hasTransition) {
        return;
    }
    Transition& t = s.transition;
    if (animate) {
        t.remaining = t.duration;
        t.delayLeft = t.delay;
        t.progress = 0.0f;
        t.alpha = 0.0f;
    } else {
        t.remaining = 0.0f;
        t.progress = 1.0f;
    }
    t.out = false;
}

// 0x005A9BB0: unlike 0x005A9C20 (showContaining) an element shown without its transition started again keeps the
// transition as it is: one still coming in with the window goes on fading in.
void Screen::show(const std::string& element, bool animate) {
    for (size_t i = 0; i < m_window.elements.size(); ++i) {
        if (_stricmp(m_window.elements[i].name.c_str(), element.c_str()) != 0) continue;
        if (animate) {
            showState(m_states[i], true);
        } else {
            m_states[i].hidden = false;
        }
    }
}

void Screen::showContaining(const std::string& part, bool animate) {
    for (size_t i = 0; i < m_window.elements.size(); ++i) {
        if (m_window.elements[i].name.find(part) != std::string::npos) {
            showState(m_states[i], animate);
        }
    }
}

void Screen::setHidden(const std::string& element, bool hidden) {
    const int i = indexOf(element);
    if (i >= 0) {
        m_states[static_cast<size_t>(i)].hidden = hidden;
    }
}

std::vector<std::string> Screen::buttons() const {
    std::vector<std::string> names;
    for (const Element& e : m_window.elements) {
        if (e.kind == ElementKind::Button) {
            names.push_back(e.name);
        }
    }
    return names;
}

bool Screen::moveHighlight(uint32_t direction) {
    static const char* const kNeighbour[] = {nullptr, "UpButton", "DownButton", "LeftButton", "RightButton"};
    const Element* current = element(m_current);
    if (current == nullptr || direction == 0 || direction > 4) {
        return false;
    }
    const std::string& set = m_states[static_cast<size_t>(indexOf(m_current))].neighbours[direction];
    const Property* p = current->find(kNeighbour[direction]);
    if (set.empty() && (p == nullptr || p->values.empty())) {
        return false;
    }
    const int next = indexOf(set.empty() ? p->values[0] : set);
    if (next < 0 || m_states[static_cast<size_t>(next)].hidden) {
        return false;
    }
    setCurrentButton(m_window.elements[static_cast<size_t>(next)].name);
    if (sound) sound("SELECT");
    return true;
}

void Screen::setCurrentButton(const std::string& name) {
    for (size_t i = 0; i < m_window.elements.size(); ++i) {
        if (m_window.elements[i].kind == ElementKind::Button) setHighlighted(static_cast<int>(i), false);
    }
    m_current = name;
    if (const int i = indexOf(name); i >= 0 && m_window.elements[static_cast<size_t>(i)].kind == ElementKind::Button) {
        setHighlighted(i, true);
    }
}

void Screen::setNeighbour(const std::string& button, uint32_t direction, const std::string& name) {
    const int i = indexOf(button);
    if (i < 0 || direction == 0 || direction > 4) return;
    m_states[static_cast<size_t>(i)].neighbours[direction] = name.substr(0, 31);
}

void Screen::update(float dt, uint32_t scroll) {
    for (size_t i = 0; i < m_window.elements.size(); ++i) {
        State& s = m_states[i];
        if (!drawn(m_window.elements[i])) continue;
        if (m_window.elements[i].kind == ElementKind::Meter && s.meter.active && s.meter.slider) {
            slideMeter(m_window.elements[i], s, dt, scroll);
        } else if (m_window.elements[i].kind == ElementKind::Slider && s.slider.active) {
            slide(s, dt, scroll);
        } else if (m_window.elements[i].kind == ElementKind::ColourChart) {
            moveChart(s, dt, scroll);
        }
        if (s.hasTransition) {
            s.transition.step(dt);
            // An element that went out is hidden (the box draw 0x005B2BE0).
            if (s.transition.out && s.transition.progress >= 1.0f) {
                s.hidden = true;
            }
        }
        if (m_window.elements[i].kind == ElementKind::Menu) {
            // The highlight follows the selection by 10 x the distance per second (0x005AFB80); the items come in
            // at 8 per second.
            s.menu.position += (static_cast<float>(s.menu.selected) - s.menu.position) * dt * 10.0f;
            s.menu.reveal = std::min(s.menu.reveal + 8.0f * dt, static_cast<float>(s.menu.items.size()));
        }
    }
}

// The element's box in pixels (the box draw 0x005B2BE0). Its place is +0xA4, +0xA8 as the element keeps it: the
// draw eases from there to +0x9C, +0xA0 by a factor that never leaves 0, and InactivePos is read and dropped.
void Screen::elementRect(const Element&, const State& s, float& x0, float& y0, float& x1, float& y1) const {
    const float sw = static_cast<float>(m_device.width());
    const float sh = static_cast<float>(m_device.height());
    x0 = s.x * sw;
    y0 = s.y * sh;
    x1 = (s.x + s.w) * sw;
    y1 = (s.y + s.h) * sh;
}

View3d* Screen::view(const std::string& element) {
    const int i = indexOf(element);
    return i >= 0 && static_cast<size_t>(i) < m_views.size() ? m_views[static_cast<size_t>(i)].get() : nullptr;
}

void Screen::draw(float seconds, float dt) {
    m_drawTime = dt;
    m_drawSeconds = seconds;
    for (size_t k = 0; k < m_order.size(); ++k) {
        const size_t i = m_order[k];
        const Element& e = m_window.elements[i];
        State& s = m_states[i];
        if (s.hidden || !drawn(e)) {
            continue;
        }
        if (e.kind == ElementKind::Line) {
            drawLineElement(e, s);
            continue;
        }
        drawBox(e, s, seconds);
        if (i < m_views.size() && m_views[i]) {
            float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
            elementRect(e, s, x0, y0, x1, y1);
            m_views[i]->draw(x0, y0, x1, y1, seconds, dt);
        }
        if (e.kind == ElementKind::Text) {
            drawText(e, s, s.highlighted);
        } else if (e.kind == ElementKind::Ticker) {
            drawTicker(e, s);
        } else if (e.kind == ElementKind::Button) {
            drawText(e, s, s.highlighted);
        } else if (e.kind == ElementKind::Menu) {
            drawMenu(e, s);
        } else if (e.kind == ElementKind::Meter) {
            drawMeter(e, s);
        } else if (e.kind == ElementKind::Dial) {
            drawDial(e, s);
        } else if (e.kind == ElementKind::Slider) {
            drawSlider(e, s);
        } else if (e.kind == ElementKind::ColourChart) {
            drawChart(e, s);
        } else if (e.kind == ElementKind::Movie) {
            drawMovie(i, s);
        }
    }
}

}  // namespace oj::ui

oj::ui::Screen::MovieMessages oj::ui::Screen::s_movieMessages{};
