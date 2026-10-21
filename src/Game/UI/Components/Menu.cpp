// Menus: their items, the selection, the events and how they are drawn.

#include "Game/UI/UiScreen.h"
#include "Game/UI/UiInternal.h"

#include "Core/Base/X87.h"

#include <cmath>

namespace oj::ui {

using namespace detail;

namespace {

// The window events the menus take (input::UiEvent).
constexpr uint32_t kEventUp = 1;
constexpr uint32_t kEventDown = 2;
constexpr uint32_t kEventLeft = 3;
constexpr uint32_t kEventRight = 4;

constexpr uint32_t kDisabledItem = 0xFF294552;

bool horizontal(const Element& e) {
    const Property* t = e.find("MenuType");
    return t != nullptr && !t->values.empty() && _stricmp(t->values[0].c_str(), "HORIZONTAL") == 0;
}

}  // namespace

// The items of a menu's .ui lines: AddItem <caption> <value> <command> [<help>], AddTexturedItem <texture>
// <highlight texture> <caption> <value> <command> [<help>] [ADDITIVE]; caption -1 is no text.
void Screen::buildMenu(size_t index) {
    const Element& e = m_window.elements[index];
    State& s = m_states[index];
    for (const Property& p : e.properties) {
        MenuItem item;
        size_t caption = 0;
        size_t help = 3;
        bool textured = false;
        if (p.name == "AddItem" && !p.values.empty()) {
            caption = 0;
            item.value = number(&p, 1, 0.0f);
            if (p.values.size() > 2) item.command = p.values[2];
        } else if (p.name == "AddTexturedItem" && p.values.size() >= 3) {
            auto unquote = [](const std::string& v) { return v == "\"\"" ? std::string() : v; };
            item.texture = unquote(p.values[0]);
            item.highTexture = unquote(p.values[1]);
            caption = 2;
            item.value = number(&p, 3, 0.0f);
            if (p.values.size() > 4) item.command = p.values[4];
            help = 5;
            textured = true;
            item.additive = _stricmp(p.values.back().c_str(), "ADDITIVE") == 0;
        } else if (p.name == "SelectItem") {
            // SelectItem n (0x005AD6C0; no window has one): the item n of those made so far highlighted (0x005AF8A0:
            // past the last the first), the highlight there at once.
            const auto n = static_cast<uint32_t>(p.values.empty() ? 0 : std::atoi(p.values[0].c_str()));
            s.menu.selected = static_cast<uint32_t>(s.menu.items.size()) - 1u < n ? 0 : n;
            s.menu.position = static_cast<float>(s.menu.selected);
            continue;
        } else {
            continue;
        }
        const int line = std::atoi(p.values[caption].c_str());
        if (line >= 0) {
            item.text = m_texts.at(static_cast<size_t>(line));
        }
        if (p.values.size() > help) {
            const int helpLine = std::atoi(p.values[help].c_str());
            if (helpLine >= 0 && static_cast<size_t>(helpLine) < m_texts.size()) {
                item.help = m_texts.at(static_cast<size_t>(helpLine));
            }
        }
        widenMenu(index, item, textured);
        auto shared = std::make_shared<MenuItem>(std::move(item));
        s.menu.items.push_back(shared);
        s.menu.all.push_back(std::move(shared));
    }
}

// +0x150 as an item is added, in the highlight font (+0x108) or else the normal one (+0x104): AddItem's and the
// code's text items (0x005AF230) with a text its width + 20 when wider; textured items (0x005AF480) with a text and a
// font its width when wider, else the item height (the screen's height x ItemHeight, +0x140).
void Screen::widenMenu(size_t index, const MenuItem& item, bool textured) {
    const Element& e = m_window.elements[index];
    State& s = m_states[index];
    const Property* fp = e.has("HighFont") ? e.find("HighFont") : e.find("Font");
    Font* f = fp != nullptr && !fp->values.empty() ? font(fp->values[0]) : nullptr;
    if (!textured) {
        if (f != nullptr && !item.text.empty()) s.menu.widest = std::max(s.menu.widest, textWidth(*f, item.text) + 20.0f);
    } else if (f != nullptr && !item.text.empty()) {
        s.menu.widest = std::max(s.menu.widest, textWidth(*f, item.text));
    } else {
        s.menu.widest = static_cast<float>(m_device.height()) * number(e.find("ItemHeight"), 0, 0.5f);
    }
}

void Screen::setMenuItemColour(const std::string& menu, size_t index, uint32_t argb) {
    const int i = indexOf(menu);
    if (i < 0 || index >= m_states[static_cast<size_t>(i)].menu.items.size()) return;
    m_states[static_cast<size_t>(i)].menu.items[index]->colour = argb;
}

void Screen::clearMenu(const std::string& menu) {
    const int i = indexOf(menu);
    if (i >= 0) {
        State& s = m_states[static_cast<size_t>(i)];
        s.menu.all.clear();
        s.menu.items.clear();
        s.menu.selected = 0;
        s.menu.widest = 0.0f;
        s.menu.top = 0;
    }
}

void Screen::restoreMenu(const std::string& menu) {
    const int i = indexOf(menu);
    if (i >= 0) {
        State& s = m_states[static_cast<size_t>(i)];
        s.menu.items = s.menu.all;
        s.menu.reveal = 0.0f;
        s.menu.revealSound = 1.0f;
    }
}

void Screen::addMenuItem(const std::string& menu, const std::wstring& text, float value, const std::string& texture,
                         const std::string& highTexture) {
    const int i = indexOf(menu);
    if (i < 0) {
        return;
    }
    auto item = std::make_shared<MenuItem>();
    item->text = text;
    item->value = value;
    item->texture = texture;
    item->highTexture = highTexture;
    // An item with a picture is a textured one (0x005AF480), else a text one (0x005AF230).
    widenMenu(static_cast<size_t>(i), *item, !texture.empty() || !highTexture.empty());
    State& s = m_states[static_cast<size_t>(i)];
    s.menu.items.push_back(item);
    s.menu.all.push_back(std::move(item));
}

void Screen::insertCommandItem(const std::string& menu, int place, const std::wstring& text, float value,
                               const std::string& command) {
    const int i = indexOf(menu);
    if (i < 0) return;
    auto item = std::make_shared<MenuItem>();
    item->text = text;
    item->value = value;
    item->command = command;
    widenMenu(static_cast<size_t>(i), *item, false);
    State& s = m_states[static_cast<size_t>(i)];
    if (place < 0) {
        s.menu.items.push_back(item);
        s.menu.all.push_back(std::move(item));
    } else if (static_cast<size_t>(place) >= s.menu.items.size()) {
        s.menu.items.push_back(std::move(item));
    } else {
        s.menu.items.insert(s.menu.items.begin() + place, std::move(item));
    }
}

void Screen::addCommandItem(const std::string& menu, const std::wstring& text, float value,
                            const std::string& command) {
    insertCommandItem(menu, -1, text, value, command);
}

bool Screen::removeMenuItem(const std::string& menu, const std::string& command) {
    const int i = indexOf(menu);
    if (i < 0) {
        return false;
    }
    std::vector<std::shared_ptr<MenuItem>>& items = m_states[static_cast<size_t>(i)].menu.items;
    for (auto it = items.begin(); it != items.end(); ++it) {
        if (_stricmp(command.c_str(), (*it)->command.c_str()) == 0) {
            items.erase(it);
            return true;
        }
    }
    return false;
}

void Screen::removeMenuItemAt(const std::string& menu, size_t index) {
    const int i = indexOf(menu);
    if (i < 0) return;
    std::vector<std::shared_ptr<MenuItem>>& items = m_states[static_cast<size_t>(i)].menu.items;
    if (index < items.size()) items.erase(items.begin() + static_cast<ptrdiff_t>(index));
}

void Screen::setMenuItemAdditive(const std::string& menu, size_t index, bool additive) {
    const int i = indexOf(menu);
    if (i < 0 || index >= m_states[static_cast<size_t>(i)].menu.items.size()) return;
    m_states[static_cast<size_t>(i)].menu.items[index]->additive = additive;
}

void Screen::setMenuItemEnabled(const std::string& menu, const std::string& command, bool enabled) {
    const int i = indexOf(menu);
    if (i < 0) return;
    for (const auto& item : m_states[static_cast<size_t>(i)].menu.items) {
        if (_stricmp(command.c_str(), item->command.c_str()) == 0) {
            item->enabled = enabled;
            return;
        }
    }
}

void Screen::setMenuItemEnabledAt(const std::string& menu, size_t index, bool enabled) {
    const int i = indexOf(menu);
    if (i < 0 || index >= m_states[static_cast<size_t>(i)].menu.items.size()) return;
    m_states[static_cast<size_t>(i)].menu.items[index]->enabled = enabled;
}

bool Screen::menuItemEnabled(const std::string& menu, size_t index) const {
    const int i = indexOf(menu);
    if (i < 0 || index >= m_states[static_cast<size_t>(i)].menu.items.size()) return false;
    return m_states[static_cast<size_t>(i)].menu.items[index]->enabled;
}

void Screen::setMenuItemText(const std::string& menu, size_t index, const std::wstring& text) {
    const int i = indexOf(menu);
    if (i < 0 || index >= m_states[static_cast<size_t>(i)].menu.items.size()) return;
    m_states[static_cast<size_t>(i)].menu.items[index]->text = text;
}

void Screen::setMenuItemHelp(const std::string& menu, size_t index, const std::wstring& help) {
    const int i = indexOf(menu);
    if (i < 0 || index >= m_states[static_cast<size_t>(i)].menu.items.size()) return;
    m_states[static_cast<size_t>(i)].menu.items[index]->help = help;
}

void Screen::setMenuItemValue(const std::string& menu, size_t index, float value) {
    const int i = indexOf(menu);
    if (i < 0 || index >= m_states[static_cast<size_t>(i)].menu.items.size()) return;
    m_states[static_cast<size_t>(i)].menu.items[index]->value = value;
}

void Screen::setMenuItemTextures(const std::string& menu, size_t index, const std::string& texture,
                                 const std::string& highTexture) {
    const int i = indexOf(menu);
    if (i < 0 || index >= m_states[static_cast<size_t>(i)].menu.items.size()) return;
    MenuItem& item = *m_states[static_cast<size_t>(i)].menu.items[index];
    item.texture = texture;
    item.highTexture = highTexture;
}

void Screen::setMenuItemCommand(const std::string& menu, size_t index, const std::string& command) {
    const int i = indexOf(menu);
    if (i < 0 || index >= m_states[static_cast<size_t>(i)].menu.items.size()) return;
    m_states[static_cast<size_t>(i)].menu.items[index]->command = command;
}

std::wstring Screen::menuItemText(const std::string& menu, size_t index) const {
    const int i = indexOf(menu);
    if (i < 0 || index >= m_states[static_cast<size_t>(i)].menu.items.size()) {
        return {};
    }
    return m_states[static_cast<size_t>(i)].menu.items[index]->text;
}

size_t Screen::menuSize(const std::string& menu) const {
    const int i = indexOf(menu);
    return i < 0 ? 0 : m_states[static_cast<size_t>(i)].menu.items.size();
}

void Screen::setMenuActive(const std::string& menu, bool active) {
    if (const int i = indexOf(menu); i >= 0) m_states[static_cast<size_t>(i)].menu.active = active;
}

void Screen::setMenuSecondPlayer(const std::string& menu, bool on) {
    if (const int i = indexOf(menu); i >= 0) m_states[static_cast<size_t>(i)].menu.secondPlayer = on;
}

void Screen::setMenuAlign(const std::string& menu, int align) {
    if (const int i = indexOf(menu); i >= 0) m_states[static_cast<size_t>(i)].menu.align = align;
}

void Screen::setMenuClipped(const std::string& menu, bool clipped) {
    if (const int i = indexOf(menu); i >= 0) m_states[static_cast<size_t>(i)].menu.clipped = clipped;
}

void Screen::removeMenuModifier(const std::string& menu) {
    if (const int i = indexOf(menu); i >= 0) m_states[static_cast<size_t>(i)].menu.useModifier = false;
}

void Screen::selectMenuItem(const std::string& menu, size_t index, bool keepPosition) {
    const int i = indexOf(menu);
    if (i < 0) {
        return;
    }
    State& s = m_states[static_cast<size_t>(i)];
    if (s.menu.items.empty() || index > s.menu.items.size() - 1) {
        index = 0;
    }
    s.menu.selected = index;
    if (!keepPosition) {
        s.menu.position = static_cast<float>(index);
    }
    menuHelp(static_cast<size_t>(i));
}

// 0x005AF860 / 0x005B5240: a menu's help ticker takes the highlighted item's help (the effect held) and is measured.
void Screen::menuHelp(size_t menu) {
    const State& s = m_states[menu];
    if (s.menu.helpTicker < 0 || s.menu.selected >= s.menu.items.size()) return;
    setText(s.menu.helpTicker, s.menu.items[s.menu.selected]->help);
    measureTicker(s.menu.helpTicker);
}

size_t Screen::selectedMenuItem(const std::string& menu) const {
    const int i = indexOf(menu);
    return i < 0 ? 0 : m_states[static_cast<size_t>(i)].menu.selected;
}

float Screen::menuPosition(const std::string& menu) const {
    const int i = indexOf(menu);
    return i < 0 ? 0.0f : m_states[static_cast<size_t>(i)].menu.position;
}

float Screen::menuItemValue(const std::string& menu, size_t index) const {
    const int i = indexOf(menu);
    if (i < 0 || index >= m_states[static_cast<size_t>(i)].menu.items.size()) {
        return 0.0f;
    }
    return m_states[static_cast<size_t>(i)].menu.items[index]->value;
}

std::string Screen::menuItemCommand(const std::string& menu, size_t index) const {
    const int i = indexOf(menu);
    if (i < 0 || index >= m_states[static_cast<size_t>(i)].menu.items.size()) {
        return {};
    }
    return m_states[static_cast<size_t>(i)].menu.items[index]->command;
}

// 0x005B11A0 for every menu of the window: no wrap at the ends; a vertical list skips disabled items and scrolls so
// the selection stays among the items that fit.
bool Screen::menuEvent(uint32_t firstEvent, uint32_t secondEvent) {
    bool moved = false;
    const float sh = static_cast<float>(m_device.height());
    for (size_t i = 0; i < m_window.elements.size(); ++i) {
        const Element& e = m_window.elements[i];
        State& s = m_states[i];
        if (e.kind != ElementKind::Menu || !s.menu.active || s.hidden || s.menu.items.empty()) {
            continue;
        }
        const float itemHeight = number(e.find("ItemHeight"), 0, 0.5f);
        const int fit = std::max(1, x87::chop(e.h * sh / (itemHeight * sh + 3.0f) - 1.0f));
        // 0x005B1207: the first player's event, or with none the second's for a menu of +0x1A0.
        const uint32_t event = firstEvent == 0 && s.menu.secondPlayer ? secondEvent : firstEvent;
        const size_t before = s.menu.selected;
        const auto count = static_cast<int>(s.menu.items.size());
        if (!horizontal(e)) {
            if (event == kEventUp) {
                int k = static_cast<int>(s.menu.selected) - 1;
                while (k > -1 && !s.menu.items[static_cast<size_t>(k)]->enabled) --k;
                if (k > -1) {
                    s.menu.selected = static_cast<size_t>(k);
                    s.menu.scrolled = k < s.menu.top;
                    if (s.menu.scrolled) s.menu.top = k;
                }
            } else if (event == kEventDown) {
                int k = static_cast<int>(s.menu.selected) + 1;
                while (k < count && !s.menu.items[static_cast<size_t>(k)]->enabled) ++k;
                if (k < count) {
                    s.menu.selected = static_cast<size_t>(k);
                    s.menu.scrolled = s.menu.top + fit < k;
                    if (s.menu.scrolled) s.menu.top = k - fit;
                }
            }
        } else {
            if (event == kEventLeft && s.menu.selected > 0) {
                --s.menu.selected;
            } else if (event == kEventRight && static_cast<int>(s.menu.selected) < count - 1) {
                ++s.menu.selected;
            }
        }
        if (s.menu.selected != before) {
            // 0x005B13C9: SELECT, +0x170 1, +0x118 1 (the highlighted picture's alpha, written after it is drawn), the
            // help ticker (0x005AF860) and the modifier started again (0x005B14A0).
            moved = true;
            if (sound) sound("SELECT");
            menuHelp(i);
            s.modifier.restart();
        }
    }
    return moved;
}

bool Screen::isMenu(const std::string& element) const {
    const int i = indexOf(element);
    return i >= 0 && m_window.elements[static_cast<size_t>(i)].kind == ElementKind::Menu;
}

void Screen::refreshMenuHelp(const std::string& menu) {
    if (const int i = indexOf(menu); i >= 0) menuHelp(static_cast<size_t>(i));
}

// A menu (0x005AFB80): items one under another (VERTICAL, the default) or side by side (HORIZONTAL). The selected
// item uses HighFont and its highlight texture and goes through the menu's text modifier; the others their texture
// inset by 15% at no more than half alpha. AutoArrows after the items.
void Screen::drawMenu(const Element& e, State& s) {
    const float sw = static_cast<float>(m_device.width());
    const float sh = static_cast<float>(m_device.height());
    float x0, y0, x1, y1;
    elementRect(e, s, x0, y0, x1, y1);
    const bool vertical = !horizontal(e);
    const bool centred = flag(e, "OptionCentered");
    const bool clip = s.menu.clipped || flag(e, "ClipMenu");
    const float itemHeight = number(e.find("ItemHeight"), 0, 0.5f);
    const float highHeight = number(e.find("HighlightItemHeight"), 0, 0.0f);
    const float unselectedOffset = number(e.find("UnselectedOffset"), 0, 0.0f);
    const int textOffset = static_cast<int>(number(e.find("TextXOffset"), 0, 0.0f));
    const int align = s.menu.align >= 0 ? s.menu.align : alignmentOf(e);
    const bool shadow = flag(e, "Shadowed");
    const Transition* t = s.hasTransition ? &s.transition : nullptr;
    const bool fading = t != nullptr && (t->type == Transition::kFadeIn || t->type == Transition::kGrow);
    const Property* normalFont = e.find("Font");
    const Property* highFont = e.has("HighFont") ? e.find("HighFont") : normalFont;
    const auto count = static_cast<int>(s.menu.items.size());
    if (clip) {
        const render::ClipRect rect{x0, y0, x1, y1};
        m_device.setClip(&rect);
    }
    const float sel = static_cast<float>(s.menu.selected);
    const float away = std::fabs(sel - s.menu.position);
    // 0x005B0358: a vertical list's next item once it has come in plays SWOOSH while it is above the menu's bottom.
    if (vertical && !centred && s.menu.revealSound <= s.menu.reveal && count > 0) {
        const float step = itemHeight * sh + 3.0f;
        const float base = y0 - static_cast<float>(s.menu.top) * step + step * 0.5f;
        if (step * s.menu.revealSound - 1.0f + base < y1 && sound) sound("SWOOSH");
        s.menu.revealSound += 1.0f;
    }

    for (int i = 0; i < count; ++i) {
        const MenuItem& item = *s.menu.items[static_cast<size_t>(i)];
        const bool selected = i == static_cast<int>(s.menu.selected);
        // 0x005B0031: the highlighted item steps the menu's modifier by the frame time (the window manager's, held
        // within 0 .. 0.1 by the step) with its text's length.
        if (selected && s.menu.useModifier) s.modifier.step(m_drawTime, static_cast<float>(item.text.size()));
        const Property* fp = selected ? highFont : normalFont;
        Font* f = fp != nullptr && !fp->values.empty() ? font(fp->values[0]) : nullptr;
        uint32_t textColour = fp != nullptr ? argbOf(fp->values, 1) : 0xFFFFFFFF;
        const float w = f != nullptr && !item.text.empty() ? textWidth(*f, item.text) : 0.0f;

        float x = align == kAlignLeft    ? x0 + 10.0f
                  : align == kAlignRight ? x1 - 10.0f - w - static_cast<float>(count * textOffset)
                                         : (x0 + x1) * 0.5f - w * 0.5f;
        float y = (y0 + y1) * 0.5f;
        const float fi = static_cast<float>(i);
        if (vertical) {
            if (centred) {
                const float step = itemHeight * sh + 2.0f;
                y += (fi - s.menu.position) * step;
                if (!selected) {
                    x += unselectedOffset;
                }
                if (i < static_cast<int>(s.menu.selected)) y -= highHeight * sh + 2.0f;
                if (i > static_cast<int>(s.menu.selected)) y += highHeight * sh + 2.0f;
            } else {
                const float step = itemHeight * sh + 3.0f;
                float base = y0 - static_cast<float>(s.menu.top) * step;
                if (s.menu.scrolled) {
                    base -= (s.menu.position - sel) * step;
                }
                y = base + fi * step + step * 0.5f;
                const float d = s.menu.reveal - fi;
                if (d < 1.0f) {
                    x -= (1.0f - d) * s.menu.widest;
                }
            }
        } else if (centred) {
            const float plain = sw * itemHeight;
            const float step = s.menu.widest != 0.0f && plain < s.menu.widest ? s.menu.widest * 1.2f : plain + 10.0f;
            x += (fi - s.menu.position) * step;
            if (!selected) {
                y += unselectedOffset;
            }
        } else {
            x -= static_cast<float>(count) * 0.5f * sw * itemHeight;
            x += fi * (sw * itemHeight + 3.0f) + itemHeight * 0.5f * sw;
        }
        if (t != nullptr) {
            x += t->offsetX;
            y += t->offsetY;
        }

        // The item's picture: across the menu (vertical) or down it (horizontal), the item height wide. The pictures
        // are those that loaded (the item's +0x48 and +0x4C, 0x005AF480): with neither none is drawn (0x005B06D2), the
        // highlighted item takes its high picture, else its own (0x005B0964), another only its own (0x005B0AE0).
        const render::TextureId own = item.texture.empty() ? 0 : texture(item.texture);
        const render::TextureId high = item.highTexture.empty() ? 0 : texture(item.highTexture);
        if (own != 0 || high != 0) {
            float ix0, iy0, ix1, iy1;
            if (vertical) {
                ix0 = x0, ix1 = x1;
                iy0 = y - itemHeight * 0.5f * sh;
                iy1 = y + itemHeight * 0.5f * sh;
            } else {
                ix0 = x - itemHeight * 0.5f * sw;
                ix1 = x + itemHeight * 0.5f * sw;
                iy0 = y0, iy1 = y1;
            }
            if (t != nullptr) {
                ix0 += t->offsetX, ix1 += t->offsetX;
                iy0 += t->offsetY, iy1 += t->offsetY;
            }
            uint32_t colour = item.colour;
            if (fading) {
                colour = withAlpha(colour, alphaByte(t->alpha));
            }
            if (selected) {
                const float dx = (ix1 - ix0) * away * 0.15f;
                const float dy = (iy1 - iy0) * away * 0.15f;
                ix0 += dx, iy0 += dy, ix1 -= dx, iy1 -= dy;
                // The driver window of the Russian, Polish and Czech games lifts its orange bars (a high picture
                // ui_pan5_*, 0x005B090A) by 4 pixels.
                if (m_language.index >= 5 && m_language.index <= 7 && high != 0 &&
                    item.highTexture.find("ui_pan5_") != std::string::npos &&
                    _stricmp(m_name.c_str(), "driver") == 0) {
                    iy0 -= 4.0f, iy1 -= 4.0f;
                }
                const uint32_t c[4] = {colour, colour, colour, colour};
                quad(ix0, iy0, ix1, iy1, c, high != 0 ? high : own, 0.0f, 1.0f, 0.0f, 1.0f, item.additive);
            } else if (own != 0) {
                const float dx = (ix1 - ix0) * 0.15f;
                const float dy = (iy1 - iy0) * 0.15f;
                ix0 += dx, iy0 += dy, ix1 -= dx, iy1 -= dy;
                const float shift = (1.0f - away) * unselectedOffset;
                if (vertical) {
                    ix0 += shift, ix1 += shift;
                } else {
                    iy0 += shift, iy1 += shift;
                }
                if ((colour >> 24) > 127) {
                    colour = withAlpha(colour, 127);
                }
                const uint32_t c[4] = {colour, colour, colour, colour};
                quad(ix0, iy0, ix1, iy1, c, own, 0.0f, 1.0f, 0.0f, 1.0f, item.additive);
            }
        }

        if (f != nullptr && !item.text.empty()) {
            if (fading) {
                textColour = withAlpha(textColour, std::min(alphaByte(t->alpha), uint8_t(textColour >> 24)));
            }
            uint32_t colour = item.enabled ? textColour : withAlpha(kDisabledItem, uint8_t(textColour >> 24));
            const float baseline = y + static_cast<float>(f->height()) * 0.3f;
            // The highlighted item's text goes through the menu's modifier (0x005B0BE8): its shadow too; two copies
            // the modifier's spread (+0x1C) to the right and to the left, their alpha no more than the modifier's
            // +0x20, after the modifier's start and place are set to 0, its growth to (1 - its time) x 10 and its reach
            // to 200; then the text itself at the least of its alpha and |pulse| + 0.25 (0.25 .. 1): it blinks.
            TextModifier* modifier = selected && s.menu.useModifier ? &s.modifier : nullptr;
            if (shadow) {
                drawString(f, textColour & 0xFF000000, item.text, x + 3.0f, baseline + 3.0f, modifier);
            }
            if (modifier != nullptr) {
                const float a = static_cast<float>(textColour >> 24) * 0.003921569f;  // [0x006FF918]
                if (a > modifier->alpha) colour = withAlpha(colour, alphaByte(modifier->alpha));
                modifier->start = 0.0f;
                modifier->place = 0.0f;
                modifier->grow = (1.0f - modifier->time) * 10.0f;
                modifier->reach = 200.0f;
                const float right = x + modifier->spread;
                drawString(f, colour, item.text, right, baseline, modifier);
                drawString(f, colour, item.text, right - (modifier->spread + modifier->spread), baseline, modifier);
                float pulse = std::fabs(modifier->pulse) + 0.25f;  // [0x006FFBEC]
                if (pulse < 0.25f) {
                    pulse = 0.25f;
                } else if (!(pulse <= 1.0f)) {
                    pulse = 1.0f;
                }
                colour = withAlpha(colour, alphaByte(a > pulse ? pulse : a));
            }
            drawString(f, colour, item.text, x, baseline);
        }
    }
    if (clip) {
        m_device.setClip(nullptr);
    }
    // 0x005B0D87: the arrows of AutoArrows, in the last item's colour.
    drawMenuArrows(e, s, s.menu.items.empty() ? 0xFFFFFFFF : s.menu.items.back()->colour);
    // The help ticker (0x005AFBB5, 0x005B1172): hidden while the menu's transition goes out (its +0x08), else drawn
    // after the menu.
    if (s.menu.helpTicker >= 0) {
        const auto h = static_cast<size_t>(s.menu.helpTicker);
        State& hs = m_states[h];
        if (s.hasTransition) hs.hidden = s.transition.out;
        if (!hs.hidden) {
            drawBox(m_window.elements[h], hs, m_drawSeconds);
            drawTicker(m_window.elements[h], hs);
        }
    }
}

// 0x005B0D87 .. 0x005B1172: with AutoArrows (+0x190) on a HORIZONTAL menu (+0x164 0) whose widest item is not 0
// (+0x150): the arrows' distance from the menu's middle (+0x198) goes to its target by 10 x the frame time's share
// (the front end's +0x104); the target (+0x19C) is half the highlighted item's text width in HighFont (else Font)
// with 15 more, and 25 more, or the widest item's width without a text or a font. The arrows (the menu's
// ui_arrow_left and ui_arrow_right, +0x188 and +0x18C, loaded by its constructor 0x005AE8D0) are half the arrows' size
// (+0x194, 24) wide and the size high, centred on the menu's middle in height: the left one from the distance left of
// the middle, the right one ending the distance right of it; each a cell of 4 x 2 at 12 a second (0x005184B0, as an
// Animated box) drawn adding, in the colour given with alpha 255 (45 at an end, where it is not drawn: the left one
// past the first item only, the right one before the last), no more than a fading transition's.
void Screen::drawMenuArrows(const Element& e, State& s, uint32_t colour) {
    if (!flag(e, "AutoArrows") || !horizontal(e) || s.menu.widest == 0.0f) return;
    const Property* fp = e.has("HighFont") ? e.find("HighFont") : e.find("Font");
    Font* f = fp != nullptr && !fp->values.empty() ? font(fp->values[0]) : nullptr;
    if (f != nullptr) {
        const std::wstring text = s.menu.selected < s.menu.items.size() ? s.menu.items[s.menu.selected]->text : std::wstring();
        const float w = static_cast<float>(static_cast<uint32_t>(textWidth(*f, text)));
        s.menu.arrowTarget = w > 0.0f ? (w + 15.0f) * 0.5f + 25.0f : s.menu.widest;  // [0x006EDB50], [0x00725814]
    } else {
        s.menu.arrowTarget = s.menu.widest;
    }
    s.menu.arrowGap = (s.menu.arrowTarget - s.menu.arrowGap) * m_drawTime * 10.0f + s.menu.arrowGap;
    float x0, y0, x1, y1;
    elementRect(e, s, x0, y0, x1, y1);
    const float cx = (x0 + x1) * 0.5f;
    const float cy = (y0 + y1) * 0.5f;
    constexpr float kSize = 24.0f;  // +0x194
    const float half = kSize * 0.5f;
    const Transition* t = s.hasTransition ? &s.transition : nullptr;
    const uint32_t c = fadeAlpha(withAlpha(colour, 0xFF), t);
    const uint32_t colours[4] = {c, c, c, c};
    // A cell of the 4 x 2 sheet at 12 a second by the game's clock.
    const uint32_t frame = static_cast<uint32_t>(x87::chop(12.0f * m_drawSeconds)) % 8u;
    const uint32_t column = frame % 4u;
    const uint32_t row = (frame - column) / 4u;
    const float u0 = static_cast<float>(column) / 4.0f;
    const float u1 = static_cast<float>(column + 1) / 4.0f;
    const float v0 = static_cast<float>(row) / 2.0f;
    const float v1 = static_cast<float>(row + 1) / 2.0f;
    const float left = cx - s.menu.arrowGap;
    const render::TextureId leftTexture = texture("ui_arrow_left");
    if (leftTexture != 0 && s.menu.selected != 0) {
        quad(left, cy - half, left + half, cy - half + kSize, colours, leftTexture, u0, u1, v0, v1, true);
    }
    const render::TextureId rightTexture = texture("ui_arrow_right");
    const auto last = static_cast<uint32_t>(s.menu.items.size()) - 1u;
    if (rightTexture != 0 && static_cast<uint32_t>(s.menu.selected) < last) {
        const float step = s.menu.arrowGap + s.menu.arrowGap - half;
        quad(left + step, cy - half, step + (left + half), cy - half + kSize, colours, rightTexture, u0, u1, v0, v1,
             true);
    }
}

}  // namespace oj::ui
