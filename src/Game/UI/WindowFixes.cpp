#include "Game/UI/WindowFixes.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

namespace oj::ui {

namespace {

std::optional<Element> copyOf(const Window& w, const char* name, ElementKind kind) {
    for (const Element& e : w.elements) {
        if (e.kind == kind && _stricmp(e.name.c_str(), name) == 0) return e;
    }
    return std::nullopt;
}

bool hasElement(const Window& w, const std::string& name);

void fovRow(Window& w) {
    if (hasElement(w, kFovButton)) return;
    const std::optional<Element> sensitivity = copyOf(w, "SENSITIVITY_BUT", ElementKind::Button);
    const std::optional<Element> vib = copyOf(w, "VIB_BUT", ElementKind::Button);
    const std::optional<Element> meter = copyOf(w, "STEERING", ElementKind::Meter);
    const std::optional<Element> diff = copyOf(w, "DIFF", ElementKind::Text);
    if (!sensitivity || !vib || !meter || !diff) return;
    // The rows: the buttons' places from VIB's down (DON_BUT's apart), each with the elements from its place down to
    // the next one's (the frame's tall boxes apart).
    std::vector<float> rows;
    for (const Element& e : w.elements) {
        if (e.kind == ElementKind::Button && _stricmp(e.name.c_str(), "DON_BUT") != 0 && e.y >= vib->y - 0.005f &&
            e.y <= sensitivity->y + 0.005f) {
            rows.push_back(e.y);
        }
    }
    std::sort(rows.begin(), rows.end());
    const std::optional<Element> done = copyOf(w, "DON_BUT", ElementKind::Button);
    if (rows.size() < 2 || !done) return;
    const auto rowOf = [&](const Element& e) {
        if (e.h > 0.2f || e.y >= done->y - 0.005f) return -1;
        for (size_t k = rows.size(); k-- > 0;) {
            if (e.y >= rows[k] - 0.005f) return static_cast<int>(k);
        }
        return -1;
    };
    // SENSITIVITY's row copied as the field of view's, in the last place.
    std::vector<Element> added;
    for (const Element& e : w.elements) {
        if (rowOf(e) != static_cast<int>(rows.size()) - 1) continue;
        Element c = e;
        const std::string& n = e.name;
        if (_stricmp(n.c_str(), "SENSITIVITY_BUT") == 0) c.name = kFovButton;
        else if (_stricmp(n.c_str(), "STEERING") == 0) c.name = kFovMeter;
        else if (n.rfind("MINUSSIGN", 0) == 0) c.name = kFovMinus;
        else if (n.rfind("PLUSSIGN", 0) == 0) c.name = kFovPlus;
        else continue;
        added.push_back(std::move(c));
    }
    const std::optional<Element> plus = [&]() -> std::optional<Element> {
        for (const Element& e : added) {
            if (e.name == kFovPlus) return e;
        }
        return std::nullopt;
    }();
    if (!plus || added.size() != 4) return;
    Element value = *diff;
    value.name = kFovValue;
    value.x = plus->x + plus->w + 0.012f;
    value.y = meter->y - 0.006f;
    value.w = 0.1f;
    value.properties.erase(std::remove_if(value.properties.begin(), value.properties.end(),
                                          [](const Property& p) { return _stricmp(p.name.c_str(), "Caption") == 0; }),
                           value.properties.end());
    // The degrees from the box's start, just after the plus (DIFF's are centred in theirs).
    for (Property& p : value.properties) {
        if (_stricmp(p.name.c_str(), "Align") == 0) p.values = {"Left"};
    }
    added.push_back(std::move(value));
    // Every row below VIB's up a place (VIB's elements stay, hidden by the window).
    for (Element& e : w.elements) {
        const int r = rowOf(e);
        if (r >= 1) e.y += rows[static_cast<size_t>(r) - 1] - rows[static_cast<size_t>(r)];
    }
    w.elements.insert(w.elements.end(), added.begin(), added.end());
}

bool hasElement(const Window& w, const std::string& name) {
    for (const Element& e : w.elements) {
        if (_stricmp(e.name.c_str(), name.c_str()) == 0) return true;
    }
    return false;
}

Element* named(Window& w, const char* name) {
    for (Element& e : w.elements) {
        if (_stricmp(e.name.c_str(), name) == 0) return &e;
    }
    return nullptr;
}

// A property's values set (added when the element has none).
void setProperty(Element& e, const char* name, std::vector<std::string> values) {
    for (Property& p : e.properties) {
        if (_stricmp(p.name.c_str(), name) == 0) {
            p.values = std::move(values);
            return;
        }
    }
    e.properties.push_back({name, std::move(values)});
}

void dropProperty(Element& e, const char* name) {
    e.properties.erase(std::remove_if(e.properties.begin(), e.properties.end(),
                                      [&](const Property& p) { return _stricmp(p.name.c_str(), name) == 0; }),
                       e.properties.end());
}

Element copyAt(Element e, const std::string& name, float x, float y) {
    e.name = name;
    e.x = x;
    e.y = y;
    dropProperty(e, "Caption");
    dropProperty(e, "Highlight");
    return e;
}

void effectsPage(Window& w) {
    if (hasElement(w, kMoreButton)) return;
    const std::optional<Element> reset = copyOf(w, "RESET_BUT", ElementKind::Button);
    const std::optional<Element> row = copyOf(w, "SHADOWS_BUT", ElementKind::Button);
    const std::optional<Element> value = copyOf(w, "SHADOWS", ElementKind::Text);
    const std::optional<Element> left = copyOf(w, "VIB_ARROW1", ElementKind::Box);
    const std::optional<Element> right = copyOf(w, "VIB_ARROW2", ElementKind::Box);
    const std::optional<Element> meter = copyOf(w, "BRIGHT", ElementKind::Meter);
    const std::optional<Element> minus = copyOf(w, "MINUSSIGN1", ElementKind::Box);
    const std::optional<Element> plus = copyOf(w, "PLUSSIGN1", ElementKind::Box);
    const Element* dofText = named(w, "DOF");
    if (!reset || !row || !value || !left || !right || !meter || !minus || !plus || dofText == nullptr ||
        named(w, "DONE_BUT") == nullptr ||
        named(w, "RESET_BUT") == nullptr || named(w, "GLOW_BUT") == nullptr || named(w, "BRIGHT_BUT") == nullptr) {
        return;
    }
    // The depth of field's row out (the user, 2026-10-28: removed altogether): DOF_BUT, its value DOF and the arrows
    // on its value's line.
    const float dofLine = dofText->y;
    w.elements.erase(std::remove_if(w.elements.begin(), w.elements.end(),
                                    [&](const Element& e) {
                                        const bool arrow = e.kind == ElementKind::Box &&
                                                           (e.name == "ARROW" || e.name == "MAP_ARROW2") &&
                                                           std::fabs(e.y - dofLine) < 0.01f;
                                        return arrow || _stricmp(e.name.c_str(), "DOF_BUT") == 0 ||
                                               _stricmp(e.name.c_str(), "DOF") == 0;
                                    }),
                     w.elements.end());
    Element* done = named(w, "DONE_BUT");
    Element* resetButton = named(w, "RESET_BUT");
    Element* glow = named(w, "GLOW_BUT");
    Element* bright = named(w, "BRIGHT_BUT");
    // The main page: GLOW, ADVANCED (MORE_BUT) in the depth of field's place, RESET TO DEFAULTS and DONE where they are.
    constexpr float kMoreY = 0.715f;
    Element more = copyAt(*reset, kMoreButton, reset->x, kMoreY);
    setProperty(more, "UpButton", {"GLOW_BUT"});
    setProperty(more, "DownButton", {"RESET_BUT"});
    setProperty(*glow, "DownButton", {kMoreButton});
    setProperty(*resetButton, "UpButton", {kMoreButton});
    (void)done;
    std::vector<Element> added{std::move(more)};
    // The ADVANCED page's rows, one line each from BRIGHT's place; the values right of the labels, wider than
    // SHADOWS' (the right arrow as far after them). The step keeps the nine rows (the meter's among them) between
    // BRIGHT's place and MORE_BUT's: at the stock rows' 0.0625 the last row ran into the ADVANCED button, whose label
    // was drawn over HEAT HAZE (the user, 2026-10-30, in the main menu and in a race's pause alike) — 0.058 leaves
    // 0.017 between the last row's foot and the button.
    constexpr float kStep = 0.058f, kValueX = 0.543f, kValueWidth = 0.22f;
    const float valueShift = kValueX - value->x;
    const float rightShift = kValueX + kValueWidth - (value->x + value->w);
    for (int i = 0; i < kEffectsRows; ++i) {
        const float y = bright->y + kStep * static_cast<float>(i);
        Element button = copyAt(*row, effectsButton(i), row->x, y);
        setProperty(button, "UpButton", {i == 0 ? "DONE_BUT" : effectsButton(i - 1)});
        setProperty(button, "DownButton", {i + 1 == kEffectsRows ? std::string(kMoreButton) : effectsButton(i + 1)});
        added.push_back(std::move(button));
        if (i == kEffectsMeterRow) {
            // The minus, the meter and the plus across the value's place, each about its line's middle.
            const float middle = y + value->h * 0.5f;
            const float plusX = kValueX + kValueWidth - plus->w;
            added.push_back(copyAt(*minus, kEffectsMinus, kValueX, middle - minus->h * 0.5f));
            Element bar = copyAt(*meter, kEffectsMeter, kValueX + minus->w + 0.006f, middle - meter->h * 0.5f);
            bar.w = plusX - 0.006f - bar.x;
            added.push_back(std::move(bar));
            added.push_back(copyAt(*plus, kEffectsPlus, plusX, middle - plus->h * 0.5f));
            continue;
        }
        added.push_back(copyAt(*left, "FXLEFT" + std::to_string(i + 1), left->x + valueShift, y + (left->y - value->y)));
        Element stepped = copyAt(*value, effectsValue(i), kValueX, y);
        stepped.w = kValueWidth;
        added.push_back(std::move(stepped));
        added.push_back(copyAt(*right, "FXRIGHT" + std::to_string(i + 1), right->x + rightShift,
                               y + (right->y - value->y)));
    }
    w.elements.insert(w.elements.end(), added.begin(), added.end());
    // The stock's own SHADOWS row stays where it is and becomes the graphics presets' (the user, 2026-10-29: the
    // shadows' row is on the ADVANCED page now, and the presets take its place): its button, its value and the arrows
    // on its line, the same elements under their stock names (the page's navigation is by name), their texts ours
    // (GraphicsOptions' updateStock: the button's label GRAPHICS, the value MED, HI, ULTRA or CUSTOM).
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

// An arrow of the game's (ui_arrows; the right one mirrored) as opts_graphics has them.
Element arrowBox(const std::string& name, float x, float y, bool right) {
    Element e;
    e.kindName = "Box";
    e.kind = ElementKind::Box;
    e.name = name;
    e.x = x;
    e.y = y;
    e.w = 0.038f;
    e.h = 0.050f;
    e.properties = {{"Colour", {"255", "255", "255", "255"}},
                    {"Texture", {"ui_arrows"}},
                    {"UVs", right ? std::vector<std::string>{"1.00", "0.00", "0.00", "1.00"}
                                  : std::vector<std::string>{"0.00", "1.00", "0.00", "1.00"}},
                    {"Transition", {"FADEIN", "0.50", "0.50"}}};
    return e;
}

// The front end's audio options' EAX row: its button under PLAYLIST, its value between two arrows on the line a meter
// has under its row (the speech row's places across), DONE below.
void eaxRow(Window& w) {
    if (hasElement(w, kEaxButton)) return;
    const std::optional<Element> speechButton = copyOf(w, "SPEECH_BUT", ElementKind::Button);
    const std::optional<Element> meter = copyOf(w, "SPEECH", ElementKind::Meter);
    const std::optional<Element> minus = copyOf(w, "MINUSSIGN3", ElementKind::Box);
    const std::optional<Element> plus = copyOf(w, "PLUSSIGN3", ElementKind::Box);
    Element* done = named(w, "DON_BUT");
    Element* above = named(w, "PLAYLIST");
    if (!speechButton || !meter || !minus || !plus || done == nullptr || above == nullptr) return;
    const float y = above->y + above->h + 0.01f;
    Element button = copyAt(*speechButton, kEaxButton, speechButton->x, y);
    setProperty(button, "UpButton", {above->name});
    setProperty(button, "DownButton", {done->name});
    setProperty(*above, "DownButton", {kEaxButton});
    setProperty(*done, "UpButton", {kEaxButton});
    const float middle = y + (meter->y + meter->h * 0.5f - speechButton->y);
    Element value = textElement(kEaxValue, meter->x, middle - 0.021f, meter->w, 0.042f);
    setProperty(value, "Font", {"goodtime11", "255", "232", "106", "23"});
    setProperty(value, "Align", {"Centre"});
    setProperty(value, "Transition", {"FADEIN", "0.50", "0.50"});
    done->y = middle + 0.035f;
    w.elements.push_back(std::move(button));
    w.elements.push_back(arrowBox(kEaxLeft, minus->x + minus->w - 0.038f, middle - 0.025f, false));
    w.elements.push_back(std::move(value));
    w.elements.push_back(arrowBox(kEaxRight, plus->x, middle - 0.025f, true));
}

void chatLines(Window& w) {
    if (hasElement(w, chatLineName(0))) return;
    for (int i = 0; i < kChatLines; ++i) {
        // The upper half the chat's keyboard leaves (Frontend/Network/LobbyChat: the text from 0.455).
        w.elements.push_back(
            textElement(chatLineName(i), 0.06f, 0.05f + 0.033f * static_cast<float>(i), 0.88f, 0.033f));
    }
}

// The chat's hint in the bottom bar: SELECT3's picture (ui_selectc, the keyboard window's TAB) at x, its text after.
void chatHintAt(Window& w, float x) {
    Element box;
    box.kindName = "Box";
    box.kind = ElementKind::Box;
    box.name = kChatButton;
    box.x = x;
    box.y = 0.846f;
    box.w = 0.050f;
    box.h = 0.069f;
    box.properties = {{"Colour", {"255", "255", "255", "255"}},
                      {"Texture", {"ui_selectc"}},
                      {"UVs", {"0.00", "1.00", "0.00", "1.00"}},
                      {"Transition", {"FADEIN", "0.40", "0.40"}}};
    w.elements.push_back(std::move(box));
    w.elements.push_back(textElement(kChatText, x + 0.050f, 0.858f, 0.115f, 0.042f));
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
    chatHintAt(w, 0.365f);
}

// link_selcar's: left of its READY hint (SELECTA at 0.439).
void carsChatHint(Window& w) {
    if (hasElement(w, kChatButton)) return;
    chatHintAt(w, 0.270f);
}

// A box of a picture (white, its whole texture) at its place.
Element pictureElement(const std::string& name, const char* texture, float x, float y, float w, float h) {
    Element box;
    box.kindName = "Box";
    box.kind = ElementKind::Box;
    box.name = name;
    box.x = x;
    box.y = y;
    box.w = w;
    box.h = h;
    box.properties = {{"Colour", {"255", "255", "255", "255"}},
                      {"Texture", {texture}},
                      {"UVs", {"0.00", "1.00", "0.00", "1.00"}}};
    return box;
}

// loadreplay's: the frame square (192 pixels of 640 x 480) right of the details (their texts end at 0.521), level
// with them (0.209 .. 0.709); the picture inside it up to its line's outer edge (4 of its 128 texels in, the line 6
// wide), the line over the picture's edge. The picture Downtown's until the code sets the replay's (the Acclaim
// builds' file: ui_downtown).
void replayPicture(Window& w) {
    if (hasElement(w, kReplayPicture)) return;
    constexpr float x = 0.610f, y = 0.259f, width = 0.300f, height = 0.400f, in = 4.0f / 128.0f;
    w.elements.push_back(pictureElement(kReplayPicture, "ui_replay_downtown", x + width * in, y + height * in,
                                        width * (1.0f - 2.0f * in), height * (1.0f - 2.0f * in)));
    w.elements.push_back(pictureElement(kReplayPictureFrame, "ui_replayborder", x, y, width, height));
}

}  // namespace

std::string chatLineName(int index) {
    return "CHAT_LINE" + std::to_string(index + 1);
}

std::string effectsButton(int row) {
    return "FX" + std::to_string(row + 1) + "_BUT";
}

std::string effectsValue(int row) {
    return "FX" + std::to_string(row + 1);
}

void fixWindow(const std::string& name, Window& window) {
    if (_stricmp(name.c_str(), "opts_game_pc") == 0 || _stricmp(name.c_str(), "opts_ingame_game") == 0) fovRow(window);
    if (_stricmp(name.c_str(), "opts_graphics") == 0) effectsPage(window);
    if (_stricmp(name.c_str(), "opts_audio") == 0) eaxRow(window);
    if (_stricmp(name.c_str(), "keyboard") == 0) chatLines(window);
    if (_stricmp(name.c_str(), "link_lobby") == 0) chatHint(window);
    if (_stricmp(name.c_str(), "link_selcar") == 0) carsChatHint(window);
    if (_stricmp(name.c_str(), "loadreplay") == 0) replayPicture(window);
}

}  // namespace oj::ui
