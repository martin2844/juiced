#if defined(OJ_MACOS) || defined(OJ_LINUX)
#include "Mods/Creative.h"
#endif
// The accessories: the windows' tint, the neon, the dials' colours, the ICE and the horn.

#include "Game/Frontend/Garage/Customise/Accessories.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>
#include <cstring>

namespace oj::frontend {

// 0x00440250: the base enter; the price and its box come in; the price 299; TINTSLIDER at the car's tint (+0x44),
// kept at +0x10.
void TintsMenu::enter(GarageHost& host, ui::Screen& s) {
    Garage& garage = host.garage();
    host.baseEnter(s, "tintsmenu");
    garage.priceShown(host, true);
    garage.price(host, 299);
    if (Car* car = garage.car(host); car != nullptr) {
        m_own = car->tint;
        s.setSliderValue("TINTSLIDER", Car::asFloat(car->tint));
    }
}

// 0x004403B0: [0x0075F108] 2. Bought: the model's parts again (0x0048B440, 0x0048B690) and stylingmenu.
// The car's tint the slider's, on the model (0x0048C040). BACK (CLICK): stylingmenu and the tint back. SELECT (CLICK):
// another tint buymod, the same stylingmenu.
void TintsMenu::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr) return;
    if (garage.texts.bought != 0) {
        garage.refreshParts(host);
        host.request("stylingmenu", SwitchMode::Swap);
    }
    float tint = 0.0f;
    if (s.find("TINTSLIDER") >= 0) {
        tint = s.sliderValue("TINTSLIDER");
        car->tint = Car::bitsOf(tint);
        garage.retint(host);
    }
    if (event == input::kBack) {
        host.sound("CLICK");
        host.request("stylingmenu", SwitchMode::Swap);
        car->tint = m_own;
        garage.retint(host);
    } else if (event == input::kSelect) {
        host.sound("CLICK");
        host.request(tint != Car::asFloat(m_own) ? "buymod" : "stylingmenu",
                     tint != Car::asFloat(m_own) ? SwitchMode::Push : SwitchMode::Swap);
    }
}

// 0x0043E690: the base enter; LOCKED hidden. A car without a neon (+0x0D none): the neon shown (0x00436160), +0x10 0,
// MENU hidden and COLOURMENU shown, the price and its box in, the price 1000. With one: +0x10 1, COLOURMENU hidden and
// MENU (remove, change) shown. +0x0C the car's neon colour (+0x2C). COLOURMENU holds each [NeonColour] of the CarDefs
// in its colour (ui_neon_off, ui_neon_choose; valued -1 under the profile's neon level 0 for the model, +0x26B), the
// car's colour highlighted; the price 0.
void NeonMenu::enter(GarageHost &host, ui::Screen &s) {
    m_custom = false;
    m_restorePrice = false;
    GameData *data = host.gameData();
    Garage &garage = host.garage();
    host.baseEnter(s, "neonmenu");
    Car *car = garage.car(host);
    if (car == nullptr || car->def == nullptr || data == nullptr || !data->career)
        return;
    s.setHidden("locked", true);
    if (car->body[9] == 0xFF) {
        garage.fitBodyPart(*car, garage.styleCategory, 0);
        if (garage.view)
            garage.view->showNeon(*car, host.viewLoading());
        m_had = 0;
        s.setHidden("MENU", true);
        s.setHidden("COLOURMENU", false);
        m_own = car->neon;
        garage.priceShown(host, true);
        garage.price(host, 1000);
    } else {
        m_had = 1;
        s.setHidden("COLOURMENU", true);
        s.setHidden("MENU", false);
        m_own = car->neon;
    }
    const auto model = static_cast<size_t>(std::clamp(car->def->id, 0, int(CareerProfile::kModelCapacity - 1)));
    const bool unlocked = data->career->profile.bodyParts(model)[9] >= 0;
    if (s.find("COLOURMENU") >= 0) {
        s.clearMenu("COLOURMENU");
        size_t own = 0;
        const std::vector<uint32_t> &neons = data->defs.neons;
        for (size_t i = 0; i < neons.size(); ++i) {
            s.addMenuItem("COLOURMENU", L"", unlocked ? 0.0f : -1.0f, "ui_neon_off", "ui_neon_choose");
            s.setMenuItemColour("COLOURMENU", i, neons[i]);
            if (m_own == neons[i])
                own = i;
        }
#if defined(OJ_MACOS) || defined(OJ_LINUX)
        if (mods::feature("neon")) {
            s.addMenuItem("COLOURMENU", L"CUSTOM RGB + EFFECTS", unlocked ? 0.f : -1.f, "ui_neon_off",
                          "ui_neon_choose");
            s.setMenuItemHelp("COLOURMENU", s.menuSize("COLOURMENU") - 1,
                              L"Preview colours and animated effects on your car.");
        }
        if (mods::feature("neon") && std::find(neons.begin(), neons.end(), m_own) == neons.end())
            own = neons.size();
#endif
        if (s.menuSize("COLOURMENU") != 0) {
            s.selectMenuItem("COLOURMENU", own > s.menuSize("COLOURMENU") - 1 ? 0 : own);
        }
    }
    garage.price(host, 0);
}

// 0x0043E9E0: [0x0075F108] 2. Bought: with MENU shown (the removal) the car's neon none and hidden; stylingmenu. With
// COLOURMENU shown: the highlighted colour the car's neon (+0x2C) on the neon, the car's neon 1, the price 1000, LOCKED
// shown for a locked colour; else LOCKED hidden. BACK (CLICK): stylingmenu; a car without a neon before loses it again;
// the colour back (0x0048BEB0). SELECT (CLICK): on MENU remove buymod, change COLOURMENU instead with the price and its
// box in and the price 1000; on COLOURMENU the car's own colour "You already have this mod" (type 1), a locked one NO,
// another buymod.
void NeonMenu::update(GarageHost &host, ui::Screen &s, uint32_t event) {
    GameData *data = host.gameData();
    const TextTable &texts = host.gameTexts();
    Garage &garage = host.garage();
    Car *car = garage.car(host);
    if (car == nullptr)
        return;
    const bool menuShown = s.find("MENU") >= 0 && !s.hidden("MENU");
    const bool coloursShown = s.find("COLOURMENU") >= 0 && !s.hidden("COLOURMENU");
    if (m_restorePrice) {
        // A nested priced popup hides these on leaving; restore once when the palette resumes.
        m_restorePrice = false;
        if (coloursShown && garage.texts.bought == 0)
            garage.priceShown(host, true);
    }
    if (coloursShown && (event == input::kLeft || event == input::kRight)) {
        m_custom = false;
#if defined(OJ_MACOS) || defined(OJ_LINUX)
        mods::clearNeonPreview();
#endif
    }
    if (garage.texts.bought != 0) {
#if defined(OJ_MACOS) || defined(OJ_LINUX)
        if (!menuShown && mods::feature("neon") &&
            !mods::saveNeon(*car, m_custom ? mods::neon(*car) : mods::Neon{car->neon & 0xffffff, 0, 2}))
            host.message(MessageType::Continue, L"Neon purchased, but the effect settings could not be saved.", false,
                         nullptr);
        mods::clearNeonPreview();
#endif
        if (menuShown) {
            car->body[9] = 0xFF;
            if (garage.view)
                garage.view->hideNeon();
        }
        host.request("stylingmenu", SwitchMode::Swap);
    }
    float value = 0.0f;
    if (coloursShown && s.menuSize("COLOURMENU") != 0) {
        const size_t item = s.selectedMenuItem("COLOURMENU");
        const std::vector<uint32_t> &neons = data->defs.neons;
        if (item < neons.size() && !m_custom)
            car->neon = neons[item];
        if (garage.view)
            garage.view->colourNeon(car->neon);
        car->body[9] = 1;
        garage.price(host, 1000);
        value = s.menuItemValue("COLOURMENU", item);
        s.setHidden("locked", value != -1.0f);
    } else {
        s.setHidden("locked", true);
    }
#if defined(OJ_MACOS) || defined(OJ_LINUX)
    if (event == input::kSelect && coloursShown && mods::feature("neon") &&
        s.selectedMenuItem("COLOURMENU") == data->defs.neons.size()) {
        if (value == -1.f) {
            host.sound("NO");
            return;
        }
        m_custom = true;
        m_restorePrice = true;
        garage.paintMode = PaintMode::Neon;
        host.request("colourchoose", SwitchMode::Push);
        return;
    }
#endif
    if (event == input::kBack) {
#if defined(OJ_MACOS) || defined(OJ_LINUX)
        mods::clearNeonPreview();
#endif
        host.sound("CLICK");
        host.request("stylingmenu", SwitchMode::Swap);
        if (m_had == 0) {
            car->body[9] = 0xFF;
            if (garage.view)
                garage.view->hideNeon();
        }
        car->neon = m_own;
        if (garage.view)
            garage.view->colourNeon(car->neon);
    } else if (event == input::kSelect) {
        host.sound("CLICK");
        if (menuShown) {
            if (s.menuSize("MENU") != 0 &&
                _stricmp(s.menuItemCommand("MENU", s.selectedMenuItem("MENU")).c_str(), "remove") == 0) {
                host.request("buymod", SwitchMode::Push);
            } else {
                s.setHidden("MENU", true);
                s.setHidden("COLOURMENU", false);
                garage.priceShown(host, true);
                garage.price(host, 1000);
            }
        } else if (!m_custom && m_had != 0 && car->neon == m_own) {
            host.message(MessageType::Continue, texts.at(kTextHaveMod), false, nullptr);
        } else if (value == -1.0f) {
            host.sound("NO");
        } else {
            host.request("buymod", SwitchMode::Push);
        }
    }
}

// 0x0043ED70: the base enter; the price and its box come in; the price 100; the car's dial colours (+0x30, +0x34)
// kept; COLOURMENU holds each [DialColour] of the CarDefs in its first colour (ui_dials_off, ui_dial_choose: Stock
// problem, not in the game's files, so the highlighted dial is drawn with ui_dials_off; the fixed build's
// ui_dials_choose, as the neons' ui_neon_off and ui_neon_choose), the car's highlighted; MPH
// "MPH" or "KPH" (1494, 1495) by the settings' units (+0x94).
void HudColourMenu::enter(GarageHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    host.baseEnter(s, "hudcolourmenu");
    garage.priceShown(host, true);
    garage.price(host, 100);
    Car* car = garage.car(host);
    if (car == nullptr || data == nullptr) return;
    m_own[0] = car->dialColour;
    m_own[1] = car->dialTextColour;
    if (s.find("COLOURMENU") >= 0) {
        s.clearMenu("COLOURMENU");
        size_t own = 0;
        const auto& dials = data->defs.dials;
        for (size_t i = 0; i < dials.size(); ++i) {
            s.addMenuItem("COLOURMENU", L"", 0.0f, "ui_dials_off", "ui_dials_choose");
            s.setMenuItemColour("COLOURMENU", i, dials[i][0]);
            if (m_own[0] == dials[i][0]) own = i;
        }
        if (s.menuSize("COLOURMENU") != 0) {
            s.selectMenuItem("COLOURMENU", own > s.menuSize("COLOURMENU") - 1 ? 0 : own);
        }
    }
    if (s.find("MPH") >= 0) {
        s.setText("MPH", texts.at(kTextUnits + (host.optionSettings().u32(save::Settings::kMph) != 0 ? 0 : 1)));
    }
}

// 0x0043F120: [0x0075F108] 2. The highlighted dial colours: bought puts them on the car (+0x30, +0x34) and goes to
// stylingmenu; REVSBG and TIME_BG take the first (made opaque), TIME, SPEED and GEAR's fonts the second. BACK (CLICK):
// stylingmenu and the car's colours back. SELECT (CLICK): the car's own "You already have this mod" (type 1), another
// buymod.
void HudColourMenu::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr || data == nullptr || s.find("COLOURMENU") < 0) return;
    const auto& dials = data->defs.dials;
    if (dials.empty()) return;
    const size_t item = s.selectedMenuItem("COLOURMENU");
    const std::array<uint32_t, 2>& colours = item < dials.size() ? dials[item] : dials[0];
    if (garage.texts.bought != 0) {
        car->dialColour = colours[0];
        car->dialTextColour = colours[1];
        host.request("stylingmenu", SwitchMode::Swap);
    }
    const uint32_t first = colours[0] | 0xFF000000u;
    s.setBoxColour("revsbg", first);
    s.setBoxColour("time_bg", first);
    for (const char* text : {"time", "speed", "gear"}) s.setFontColours(text, colours[1], colours[1]);
    if (event == input::kBack) {
        host.sound("CLICK");
        host.request("stylingmenu", SwitchMode::Swap);
        car->dialColour = m_own[0];
        car->dialTextColour = m_own[1];
    } else if (event == input::kSelect) {
        host.sound("CLICK");
        if (colours[0] == m_own[0]) {
            host.message(MessageType::Continue, texts.at(kTextHaveMod), false, nullptr);
        } else {
            host.request("buymod", SwitchMode::Push);
        }
    }
}

// 0x0043F7F0: the base enter; the price and its box come in; the price 199; +0x18 the car's ICE (+0x15); +0x1C none;
// MENU emptied and the catalogue's level 0: the ICE parts of the suppliers 40 then 41 (0x004990F0) each an item of
// its UIONTEX at its index, its SHAPE; the car's own highlighted.
void IceMenu::enter(GarageHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    Garage& garage = host.garage();
    host.baseEnter(s, "icemenu");
    garage.priceShown(host, true);
    garage.price(host, 199);
    Car* car = garage.car(host);
    if (car == nullptr || data == nullptr) return;
    m_own = static_cast<int8_t>(car->mods[kIce]);
    if (s.find("MENU") < 0) return;
    m_shown = -1;
    s.clearMenu("MENU");
    garage.catalogueLevel = 0;
    for (const int brand : {0x28, 0x29}) {
        for (const CarPart* part : modBrandParts(data->catalogue.lists[kIce], brand, garage.catalogueLevel)) {
            s.addMenuItem("MENU", L"", static_cast<float>(part->index), part->uiOn);
            s.setMenuItemCommand("MENU", s.menuSize("MENU") - 1, part->model);
            if (static_cast<int32_t>(part->index) == m_own) s.selectMenuItem("MENU", s.menuSize("MENU") - 1);
        }
    }
}

// 0x0043FB40: [0x0075F108] 2. Bought: the model's parts again and stylingmenu. Another highlighted item
// than +0x1C: HEAD_LOGO ui_<supplier>_on, the car's ICE the item's, HEAD_NAME its TEXT, the price its COST. BACK
// (CLICK): stylingmenu and the car's ICE back. SELECT: the car's own "You already have this mod" (type 1), else CLICK
// and buymod.
void IceMenu::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr || data == nullptr || s.find("MENU") < 0 || s.menuSize("MENU") == 0) return;
    if (garage.texts.bought != 0) {
        garage.refreshParts(host);
        host.request("stylingmenu", SwitchMode::Swap);
    }
    const size_t item = s.selectedMenuItem("MENU");
    const float value = s.menuItemValue("MENU", item);
    const std::vector<CarPart>& parts = data->catalogue.lists[kIce].parts;
    if (static_cast<int32_t>(item) != m_shown && !parts.empty()) {
        m_shown = static_cast<int32_t>(item);
        const int index = x87::chop(value);
        const bool inList = index >= 0 && static_cast<size_t>(index) < parts.size();
        const CarPart& part = inList ? parts[static_cast<size_t>(index)] : parts[0];
        if (s.find("HEAD_LOGO") >= 0) {
            const std::vector<ExeTables::Brand>& brands = data->tables.brands;
            const std::string name = part.brand >= 0 && static_cast<size_t>(part.brand) < brands.size()
                                         ? brands[static_cast<size_t>(part.brand)].name
                                         : "";
            s.setTexture("HEAD_LOGO", "ui_" + name + "_on");
        }
        car->mods[kIce] = static_cast<uint8_t>(index);
        if (s.find("HEAD_NAME") >= 0) s.setText("HEAD_NAME", toWide(part.label));
        garage.price(host, part.cost);
    }
    if (event == input::kBack) {
        host.sound("CLICK");
        host.request("stylingmenu", SwitchMode::Swap);
        car->mods[kIce] = static_cast<uint8_t>(m_own);
        garage.refreshParts(host);
    } else if (event == input::kSelect) {
        if (value == static_cast<float>(m_own)) {
            host.message(MessageType::Continue, texts.at(kTextHaveMod), false, nullptr);
        } else {
            host.sound("CLICK");
            host.request("buymod", SwitchMode::Push);
        }
    }
}

// 0x0043FEC0: the base enter; the price and its box come in; the price 399; +0x10 the car's horn (+0xB0), MENU's
// item horn - 1 highlighted; +0x14 none, +0x18 no horn playing (-1).
void HornMenu::enter(GarageHost& host, ui::Screen& s) {
    Garage& garage = host.garage();
    host.baseEnter(s, "hornmenu");
    m_handle = 0xFFFFFFFF;
    garage.priceShown(host, true);
    garage.price(host, 399);
    Car* car = garage.car(host);
    if (car == nullptr) return;
    m_own = car->horn;
    if (s.find("MENU") >= 0 && s.menuSize("MENU") != 0) {
        const size_t item = static_cast<size_t>(m_own - 1);
        s.selectMenuItem("MENU", item < s.menuSize("MENU") ? item : 0);
    }
    m_shown = -1;
}

// 0x00440040: [0x0075F108] 2. Bought: stylingmenu. Another highlighted item than +0x14: the horn playing (+0x18, not
// -1) stopped (the interface's player's +0x20), carhorn<item + 1> played (its handle +0x18), the car's horn the item
// + 1 (0x00440124). BACK (CLICK): stylingmenu and the car's horn back. SELECT: the car's own "You already have this
// mod" (type 1), another CLICK and buymod.
void HornMenu::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr || s.find("MENU") < 0) return;
    if (garage.texts.bought != 0) host.request("stylingmenu", SwitchMode::Swap);
    const auto item = static_cast<int32_t>(s.selectedMenuItem("MENU"));
    if (m_shown != item) {
        if (m_handle != 0xFFFFFFFF && host.uiSounds().player() != nullptr) host.uiSounds().player()->stop(m_handle);
        m_handle = host.uiSounds().play(std::format("carhorn{}", item + 1).c_str());
        m_shown = item;
        car->horn = static_cast<uint32_t>(item + 1);
    }
    if (event == input::kBack) {
        host.sound("CLICK");
        host.request("stylingmenu", SwitchMode::Swap);
        car->horn = m_own;
        garage.refreshParts(host);
    } else if (event == input::kSelect) {
        if (car->horn == m_own) {
            host.message(MessageType::Continue, texts.at(kTextHaveMod), false, nullptr);
        } else {
            host.sound("CLICK");
            host.request("buymod", SwitchMode::Push);
        }
    }
}

// 0x0043FF90: hornmenu's leave is 0x00440320's and stops the horn playing (+0x18, not -1).
void HornMenu::stopHorn(GarageHost& host) {
    if (m_handle != 0xFFFFFFFF && host.uiSounds().player() != nullptr) host.uiSounds().player()->stop(m_handle);
}

}  // namespace oj::frontend
