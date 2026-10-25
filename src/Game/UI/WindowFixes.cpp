#include "Game/UI/WindowFixes.h"

#include <cmath>
#include <cstring>
#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

namespace oj::ui {

namespace {

// The rows of the window: an element within 0.01 of a row's y is in it.
bool sameRow(float a, float b) {
    return std::fabs(a - b) < 0.01f;
}

std::optional<Element> copyOf(const Window& w, const char* name, ElementKind kind) {
    for (const Element& e : w.elements) {
        if (e.kind == kind && _stricmp(e.name.c_str(), name) == 0) return e;
    }
    return std::nullopt;
}

// The first box of the name in the row.
std::optional<Element> boxInRow(const Window& w, const char* name, float y) {
    for (const Element& e : w.elements) {
        if (e.kind == ElementKind::Box && _stricmp(e.name.c_str(), name) == 0 && sameRow(e.y, y)) return e;
    }
    return std::nullopt;
}

void glowMeter(Window& w) {
    const std::optional<Element> detailButton = copyOf(w, "DETAIL_BUT", ElementKind::Button);
    const std::optional<Element> glowButton = copyOf(w, "GLOW_BUT", ElementKind::Button);
    const std::optional<Element> detail = copyOf(w, "DETAIL", ElementKind::Meter);
    const std::optional<Element> glow = copyOf(w, "GLOW", ElementKind::Text);
    if (!detailButton || !glowButton || !detail || !glow || copyOf(w, kGlowMeter, ElementKind::Meter)) return;
    const std::optional<Element> minus = boxInRow(w, "MINUSSIGN1", detail->y);
    const std::optional<Element> plus = boxInRow(w, "PLUSSIGN1", detail->y);
    if (!minus || !plus) return;
    const float down = glowButton->y - detailButton->y;
    std::vector<Element> added;
    for (const auto& [from, name] : {std::pair{*detail, kGlowMeter}, std::pair{*minus, kGlowMinus},
                                     std::pair{*plus, kGlowPlus}}) {
        Element e = from;
        e.name = name;
        e.y += down;
        added.push_back(std::move(e));
    }
    std::erase_if(w.elements, [&](const Element& e) {
        return e.kind == ElementKind::Box && _stricmp(e.name.c_str(), "ARROW") == 0 && sameRow(e.y, glow->y);
    });
    // After the GLOW text.
    auto at = w.elements.begin();
    while (at != w.elements.end() && !(at->kind == ElementKind::Text && _stricmp(at->name.c_str(), "GLOW") == 0)) {
        ++at;
    }
    w.elements.insert(at == w.elements.end() ? at : at + 1, added.begin(), added.end());
}

bool hasElement(const Window& w, const std::string& name) {
    for (const Element& e : w.elements) {
        if (_stricmp(e.name.c_str(), name.c_str()) == 0) return true;
    }
    return false;
}

Element textElement(const std::string& name, float x, float y, float w, float h) {
    Element e;
    e.kindName = "Text";
    e.kind = ElementKind::Text;
    e.name = name;
    e.x = x;
    e.y = y;
    e.w = w;
    e.h = h;
    e.properties = {{"Colour", {"0", "0", "0", "0"}},
                    {"Font", {"goodtime11", "255", "255", "255", "255"}},
                    {"Align", {"Left"}},
                    {"Transition", {"FADEIN", "0.40", "0.40"}}};
    return e;
}

void chatLines(Window& w) {
    if (hasElement(w, chatLineName(0))) return;
    for (int i = 0; i < kChatLines; ++i) {
        w.elements.push_back(textElement(chatLineName(i), 0.06f, 0.05f + 0.04f * static_cast<float>(i), 0.88f, 0.04f));
    }
}

void chatHint(Window& w) {
    if (hasElement(w, kChatButton)) return;
    // The bar's FRIENDS and PLAYERS hints (an online lobby's; 0.081 and 0.284) moved left to make room for the chat's
    // between PLAYERS and OK.
    for (Element& e : w.elements) {
        const std::string& n = e.name;
        if (_stricmp(n.c_str(), "FRIENDS_BUT") == 0) e.x = 0.021f;
        if (_stricmp(n.c_str(), "FRIENDS_TEXT") == 0) e.x = 0.068f;
        if (_stricmp(n.c_str(), "PLAYERS_BUT") == 0) e.x = 0.190f;
        if (_stricmp(n.c_str(), "PLAYERS_TEXT") == 0) e.x = 0.240f;
    }
    Element box;
    box.kindName = "Box";
    box.kind = ElementKind::Box;
    box.name = kChatButton;
    box.x = 0.365f;
    box.y = 0.846f;
    box.w = 0.050f;
    box.h = 0.069f;
    box.properties = {{"Colour", {"255", "255", "255", "255"}},
                      {"Texture", {"ui_selectb"}},
                      {"UVs", {"0.00", "1.00", "0.00", "1.00"}},
                      {"Transition", {"FADEIN", "0.40", "0.40"}}};
    w.elements.push_back(std::move(box));
    w.elements.push_back(textElement(kChatText, 0.415f, 0.858f, 0.115f, 0.042f));
}

}  // namespace

std::string chatLineName(int index) {
    return "CHAT_LINE" + std::to_string(index + 1);
}

void fixWindow(const std::string& name, Window& window) {
    if (_stricmp(name.c_str(), "opts_graphics") == 0) glowMeter(window);
    if (_stricmp(name.c_str(), "keyboard") == 0) chatLines(window);
    if (_stricmp(name.c_str(), "link_lobby") == 0) chatHint(window);
}

}  // namespace oj::ui
