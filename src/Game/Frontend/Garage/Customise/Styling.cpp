#if defined(OJ_MACOS) || defined(OJ_LINUX) || defined(OJ_ANDROID)
#include "Mods/Creative.h"
#endif
// The styling windows: stylingmenu, the body parts (modmenu), the wheels, the decals and the
// sponsors' decals.

#include "Game/Frontend/Garage/Customise/Styling.h"

#include "Core/Career/CareerInit.h"
#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>
#include <cstring>

namespace oj::frontend {

namespace {

// stylingmenu's items: the body part category (-1 none) and the rig's pose ([0x0079E7A0 ..], never written, are 0).
struct StylingItem {
    const char* command;
    float pitch;
    float yaw;
};
constexpr StylingItem kStylingItems[] = {
    {"fbump", 0.0f, 3.14f},      {"rbump", 0.0f, 0.0f},       {"wmirrors", 0.0f, 2.14f},  {"spoil", -0.35f, 4.5f},
    {"skirt", 0.0f, 4.1f},       {"bonnet", -0.2f, 3.28f},    {"headlight", 0.0f, 3.14f}, {"wheels", 0.0f, 3.75f},
    {"decal", 0.0f, 5.0f},       {"neon", -0.2f, -2.0f},      {"hud", 0.0f, -2.0f},       {"ice", 0.0f, -1.57f},
    {"horn", 0.0f, -1.57f},      {"tints", -0.25f, -1.57f},   {"sponsor", 0.0f, -1.57f},
};

// The styling menu's names of the body part categories 0 .. 8 (6, the rear lights, has none).
constexpr const char* kStylingCategoryItems[] = {"fbump",    "rbump", "bonnet", "spoil", "skirt",
                                                 "headlight", nullptr, "wmirrors", "decal"};

// decalmenu's price of a decal (its place): none (127) 0, the first 12000, up to 14 1000, under [0x00725358] (23)
// 5000, else 9000.
int32_t decalPrice(float value) {
    if (value == 127.0f) return 0;
    if (value == 0.0f) return 12000;
    if (value <= 14.0f) return 1000;
    return 23.0f > value ? 5000 : 9000;
}

// Added cars can append retail styles after their existing saved indices. The
// prototype lock, price and tint rules follow the style, not its new list position.
float decalStyle(const GameData& data, const Car& car, float selected) {
    const auto index = static_cast<int>(selected);
    if (!car.def || car.def->id < 53 || index < 0 || index == 127 ||
        size_t(index) >= car.def->parts[8].parts.size()) return selected;
    const auto& name = car.def->parts[8].parts[index].model;
    const auto start = name.find("DECAL_");
    const auto* retail = data.defs.byId(1);
    if (start != std::string::npos && retail) {
        const auto key = name.substr(start);
        const auto& styles = retail->parts[8].parts;
        for (size_t i = 0; i < styles.size(); ++i) {
            const auto pos = styles[i].model.find("DECAL_");
            if (pos != std::string::npos && styles[i].model.substr(pos) == key) return float(i);
        }
    }
    return 1.f; // Car-specific graphics are ordinary tintable decals.
}

}  // namespace

// 0x0043B060: the base enter; the camera to the garage's place; with the money under 10000 the window's help page
// (0x004850E0 inline); the items of the body part categories (0 .. 8) without parts, or with one part that is a
// standard one (the list's +0x14), taken out; without any performance part on the car (+0x0E .. +0x16 all none) the sponsor item;
// past the list the first item highlighted; another car than +0x18 highlights the first; the answer -1, no question.
void StylingMenu::enter(GarageHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    Garage& garage = host.garage();
    host.baseEnter(s, "stylingmenu");
    if (garage.view) garage.view->cameraTo(kCameraGarage);
    if (data == nullptr || !data->career) return;
    if (static_cast<float>(data->career->profile.money) < 10000.0f) host.helpPage("stylingmenu", false);
    Car* car = garage.car(host);
    if (car != nullptr && car->def != nullptr && s.find("Menu") >= 0) {
        for (size_t category = 0; category < 10; ++category) {
            const CarPartList& list = car->def->parts[category];
            const bool none = list.parts.empty() || (list.parts.size() == 1 && list.standard);
            if (none && category <= 8 && kStylingCategoryItems[category] != nullptr) {
                s.removeMenuItem("Menu", kStylingCategoryItems[category]);
            }
        }
        bool noMods = true;
        for (size_t kind = 0; kind < 9; ++kind) {
            if (static_cast<int8_t>(car->mods[kind]) > -1) noMods = false;
        }
        if (noMods) s.removeMenuItem("Menu", "sponsor");
        if (s.menuSize("Menu") != 0 && s.selectedMenuItem("Menu") > s.menuSize("Menu") - 1) {
            s.selectMenuItem("Menu", 0);
        }
        if (m_car != car) {
            if (s.menuSize("Menu") != 0) s.selectMenuItem("Menu", 0);
            m_car = car;
        }
    }
    m_answer = -1;
    m_asked = 0;
}

// 0x0043B4A0: [0x0075F108] 2. A question (+0x14, nothing asks it) answered "yes" switches the car's +0x48 and its
// model's parts (0x0048B440, 0x0048B690). Each frame the highlighted item sets the garage's rig to its
// pose (0x00436340); SELECT (CLICK): the body parts fbump (0), rbump (1), bonnet (2), spoil (3), skirt (4), headlight
// (5), wmirrors (7) modmenu with the category ([0x00715C28]); wheels the catalogue's level 0 and suppliers for the
// wheels (kind 4); decal (8) decalmenu, neon (9) neonmenu, hud hudcolourmenu, ice icemenu, horn hornmenu, tints
// tintsmenu, sponsor sponsordecalmenu. BACK (CLICK): custmenu. The chassis lowered.
void StylingMenu::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (m_asked != 0) {
        if (m_answer == 1 && car != nullptr) {
            car->sponsorDecals = car->sponsorDecals == 0 ? 1 : 0;
            garage.refreshParts(host);
        }
        m_asked = 0;
    }
    bool handled = false;
    if (s.find("Menu") >= 0 && s.menuSize("Menu") != 0) {
        const std::string command = s.menuItemCommand("Menu", s.selectedMenuItem("Menu"));
        for (const StylingItem& item : kStylingItems) {
            if (_stricmp(command.c_str(), item.command) != 0) continue;
            if (garage.view) garage.view->setRig(item.pitch, item.yaw);
            handled = true;
            if (event != input::kSelect) break;
            host.sound("CLICK");
            static const std::pair<const char*, int> kBody[] = {{"fbump", 0}, {"rbump", 1},     {"bonnet", 2},
                                                                {"spoil", 3}, {"skirt", 4},     {"headlight", 5},
                                                                {"wmirrors", 7}};
            const char* next = nullptr;
            for (const auto& [name, category] : kBody) {
                if (_stricmp(command.c_str(), name) == 0) {
                    garage.styleCategory = category;
                    next = "modmenu";
                }
            }
            if (next == nullptr) {
                if (_stricmp(command.c_str(), "wheels") == 0) {
                    garage.catalogueLevel = 0;
                    garage.kind = kWheel;
                    next = "suppliers";
                } else if (_stricmp(command.c_str(), "decal") == 0) {
                    garage.styleCategory = 8;
                    next = "decalmenu";
                } else if (_stricmp(command.c_str(), "neon") == 0) {
                    garage.styleCategory = 9;
                    next = "neonmenu";
                } else if (_stricmp(command.c_str(), "hud") == 0) {
                    next = "hudcolourmenu";
                } else if (_stricmp(command.c_str(), "ice") == 0) {
                    next = "icemenu";
                } else if (_stricmp(command.c_str(), "horn") == 0) {
                    next = "hornmenu";
                } else if (_stricmp(command.c_str(), "tints") == 0) {
                    next = "tintsmenu";
                } else {
                    next = "sponsordecalmenu";
                }
            }
            host.request(next, SwitchMode::Swap);
            break;
        }
    }
    (void)handled;
    if (event == input::kBack) {
        host.sound("CLICK");
        host.request("custmenu", SwitchMode::Swap);
    }
    garage.lowerChassis(host);
}

// 0x0043BDE0: +0x10 the car's part of the category; LOCKED hidden; Menu emptied; with ALLOW_NONE "None" (164, value
// 127) first and the price 500; for each part of the list: the first one or one of no brand "STOCK" (102, its place)
// when the car has a part (+0x10 not -1), else UI_<brand>_off and _on at its place; ARROWLEFT and ARROWRIGHT shown;
// the base enter; the car's part highlighted (one later after "None"; past the list the first); +0x0C none; the price
// and its box come in.
void ModMenu::enter(GarageHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr || car->def == nullptr || data == nullptr) return;
    const auto category = static_cast<size_t>(std::clamp(garage.styleCategory, 0, 9));
    const CarPartList& list = car->def->parts[category];
    m_own = static_cast<int8_t>(car->body[category]);
    s.setHidden("locked", true);
    if (s.find("Menu") >= 0) {
        s.clearMenu("Menu");
        if (list.allowNone) {
            s.addCommandItem("Menu", texts.at(kTextNone), 127.0f, "");
            garage.price(host, 500);
        }
        const std::vector<ExeTables::Brand>& brands = data->tables.brands;
        for (size_t i = 0; i < list.parts.size(); ++i) {
            const int brand = list.parts[i].brand;
            if (i == 0 || brand == -1) {
                if (m_own != -1) {
                    s.addCommandItem("Menu", texts.at(kTextStockItem), static_cast<float>(i), "");
                }
            } else {
                const std::string name =
                    brand >= 0 && static_cast<size_t>(brand) < brands.size() ? brands[static_cast<size_t>(brand)].name
                                                                             : "";
                s.addMenuItem("Menu", L"", static_cast<float>(i), "ui_" + name + "_off", "ui_" + name + "_on");
            }
        }
    }
    s.setHidden("ARROWLEFT", false);
    s.setHidden("ARROWRIGHT", false);
    host.baseEnter(s, "modmenu");
    if (s.find("Menu") >= 0 && s.menuSize("Menu") != 0) {
        auto item = static_cast<uint32_t>(m_own + (list.allowNone ? 1 : 0));
        if (item > s.menuSize("Menu") - 1) item = 0;
        s.selectMenuItem("Menu", item);
    }
    m_shown = -1;
    garage.priceShown(host, true);
}

// 0x0043C1B0: [0x0075F108] 2. The arrows shown but ARROWLEFT at the first item and ARROWRIGHT at the last. Another
// highlighted item than +0x0C: its part on the car and the model (0x0048BC30); the price 500 for "None", 0 for the
// car's own part, else 500 x (place + 3) + 500 (0x00447660); LOCKED shown for a part past the one the profile
// unlocked for the model and category (place - 1 over [profile + 0x262 + model x 10 + category]), not for "None".
// SELECT: a locked part NO; else CLICK and the car's own part "You already have this mod" (type 1), another buymod.
// BACK: the car's part back, CLICK, stylingmenu and the garage's texts cleared. Bought: stylingmenu. The chassis
// lowered.
void ModMenu::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr || car->def == nullptr || data == nullptr || !data->career) return;
    const auto model = static_cast<size_t>(std::clamp(car->def->id, 0, int(CareerProfile::kModelCapacity - 1)));
    // The fixed build's wing mirrors (7) open by their own steps (mirrorLevel): the unlock schedule never raises
    // theirs.
    const auto category = static_cast<size_t>(std::clamp(garage.styleCategory, 0, 9));
    const int level = garage.styleCategory == 7 ? mirrorLevel(data->career->profile, model)
                                                : data->career->profile.bodyParts(model)[category];
    float value = -1.0f;
    bool hasItem = false;
    if (s.find("Menu") >= 0 && s.menuSize("Menu") != 0) {
        const size_t count = s.menuSize("Menu");
        const size_t item = s.selectedMenuItem("Menu");
        if (s.find("ARROWLEFT") >= 0 && s.find("ARROWRIGHT") >= 0) {
            s.setHidden("ARROWRIGHT", false);
            s.setHidden("ARROWLEFT", false);
            if (item == 0) s.setHidden("ARROWLEFT", true);
            if (item == count - 1) s.setHidden("ARROWRIGHT", true);
        }
        hasItem = true;
        value = s.menuItemValue("Menu", item);
#if defined(OJ_MACOS) || defined(OJ_LINUX) || defined(OJ_ANDROID)
        if(event==input::kSelect && mods::feature("logo") && int(value)==mods::logoIndex(car->modelName()) && !mods::hasImage(false)) {mods::requestMenu();return;}
#endif
        if (static_cast<int32_t>(item) != m_shown) {
            garage.fitBodyPart(*car, garage.styleCategory, x87::chop(value));
            int32_t price = 0;
            if (value == 127.0f) {
                price = 500;
            } else if (value != static_cast<float>(m_own)) {
                const int part = x87::chop(value);
                const int cost = garage.styleCategory == -1 || part == -1 || part == 0 || part > 0x7E ? 0 : (part + 3) * 500;
                price = cost + 500;
            }
            garage.price(host, price);
            s.setHidden("locked", !(value - 1.0f > static_cast<float>(level) && value != 127.0f));
            m_shown = static_cast<int32_t>(item);
        }
    }
    if (event == input::kSelect) {
        if (hasItem) {
            if (value - 1.0f > static_cast<float>(level) && value != 127.0f) {
                host.sound("NO");
            } else {
                host.sound("CLICK");
                if (value != static_cast<float>(m_own)) {
                    host.request("buymod", SwitchMode::Push);
                } else {
                    host.message(MessageType::Continue, texts.at(kTextHaveMod), false, nullptr);
                }
            }
        }
    } else if (event == input::kBack) {
        garage.fitBodyPart(*car, garage.styleCategory, m_own);
        host.sound("CLICK");
        host.request("stylingmenu", SwitchMode::Swap);
        garage.clear(host);
        garage.lowerChassis(host);
        return;
    }
    if (garage.texts.bought != 0) host.request("stylingmenu", SwitchMode::Swap);
    garage.lowerChassis(host);
}

// 0x00439300: the base enter; the price 0; +0x20 the car's wheel. The supplier "STOCK" ([0x00715C30] -1): the DATA_
// elements hidden, the price and its box in, buymod over the window and the standard wheels on the model (0x0048B860
// with -1). Else the DATA_ elements shown, the price and its box out, Menu emptied (the first highlighted) and an item
// for each wheel of the supplier at the catalogue's level but the car's own: its UIONTEX, its place in the list, its
// SHAPE; +0x1C none; data_name, data_ize and data_suppLogo (UI_<supplier>_ON); the money shown; the price and its box
// come in.
void WheelsMenu::enter(GarageHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    Garage& garage = host.garage();
    host.baseEnter(s, "wheelsmenu");
    garage.price(host, 0);
    Car* car = garage.car(host);
    if (car == nullptr || data == nullptr) return;
    m_own = static_cast<int8_t>(car->mods[kWheel]);
    if (garage.brand == -1) {
        s.hideContaining("DATA_");
        garage.priceShown(host, true);
        host.request("buymod", SwitchMode::Push);
        garage.fitWheels(host, *car, -1);
        return;
    }
    s.showContaining("DATA_", false);
    garage.priceShown(host, false);
    if (s.find("Menu") >= 0) {
        s.clearMenu("Menu");
        garage.filtered = modBrandParts(data->catalogue.lists[kWheel], garage.brand, garage.catalogueLevel);
        for (size_t i = 0; i < garage.filtered.size(); ++i) {
            const CarPart& part = *garage.filtered[i];
            if (static_cast<int32_t>(part.index) == m_own) continue;
            s.addMenuItem("Menu", L"", static_cast<float>(i), part.uiOn);
            s.setMenuItemCommand("Menu", s.menuSize("Menu") - 1, part.model);
        }
        if (s.menuSize("Menu") != 0) s.selectMenuItem("Menu", 0);
        m_shown = -1;
    }
    garage.cash(host);
    if (s.find("data_suppLogo") >= 0) {
        const std::vector<ExeTables::Brand>& brands = data->tables.brands;
        const std::string name = garage.brand >= 0 && static_cast<size_t>(garage.brand) < brands.size()
                                     ? brands[static_cast<size_t>(garage.brand)].name
                                     : "";
        s.setTexture("data_suppLogo", "UI_" + name + "_ON");
    }
    garage.priceShown(host, true);
}

// 0x00439630: the highlighted item kept; the price and its box go out. For "STOCK" not bought the car's wheel back on
// the model with its colour (0x0048C440) and parts; a wheel bought clears the car's wheel colour (+0x40).
void WheelsMenu::leave(GarageHost& host, ui::Screen& s) {
    Garage& garage = host.garage();
    host.baseLeave(s, "wheelsmenu");
    garage.priceShown(host, false);
    Car* car = garage.car(host);
    if (car == nullptr) return;
    if (garage.brand == -1 && garage.texts.bought == 0) {
        garage.fitWheels(host, *car, m_own);
        if (garage.view && garage.view->car() != nullptr) garage.view->car()->colourWheels(*car);
        return;
    }
    if (garage.texts.bought != 0) car->wheelsColoured = 0;
}

// 0x00439730: [0x0075F108] 2. Bought: stylingmenu, the model's parts again, the car's wheel colour cleared (+0x40).
// "STOCK" not bought: suppliers. The highlighted wheel: data_name its TEXT, the price its COST, data_ize its DATAVAL
// (cut); another item than +0x1C puts it on the car and the model (0x0048B860). SELECT: the car's own wheel "You
// already have this mod" (type 1), else CLICK and buymod. BACK (CLICK): suppliers, the car's wheel back with its
// colour, the garage's texts cleared. The chassis lowered.
void WheelsMenu::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr || data == nullptr) return;
    if (garage.texts.bought != 0) {
        host.request("stylingmenu", SwitchMode::Swap);
        garage.refreshParts(host);
        car->wheelsColoured = 0;
        return;
    }
    if (garage.brand == -1) host.request("suppliers", SwitchMode::Swap);
    if (s.find("Menu") >= 0 && s.menuSize("Menu") != 0) {
        const size_t item = s.selectedMenuItem("Menu");
        const int place = x87::chop(s.menuItemValue("Menu", item));
        const CarPart* part = place >= 0 && static_cast<size_t>(place) < garage.filtered.size()
                                  ? garage.filtered[static_cast<size_t>(place)]
                                  : nullptr;
        if (part != nullptr) {
            if (s.find("data_name") >= 0) s.setText("data_name", toWide(part->label));
            garage.price(host, part->cost);
            if (s.find("data_ize") >= 0) s.setText("data_ize", toWide(std::format("{}", x87::chop(part->dataValue))));
        }
        if (static_cast<int32_t>(item) != m_shown) {
            garage.fitWheels(host, *car, part != nullptr ? static_cast<int>(part->index) : -1);
            m_shown = static_cast<int32_t>(item);
        }
        if (event == input::kSelect && part != nullptr) {
            if (static_cast<int32_t>(part->index) == m_own) {
                host.message(MessageType::Continue, texts.at(kTextHaveMod), false, nullptr);
            } else {
                host.sound("CLICK");
                host.request("buymod", SwitchMode::Push);
            }
            event = input::kNone;
        }
    }
    if (event == input::kBack) {
        host.sound("CLICK");
        host.request("suppliers", SwitchMode::Swap);
        garage.fitWheels(host, *car, m_own);
        if (garage.view && garage.view->car() != nullptr) garage.view->car()->colourWheels(*car);
        garage.clear(host);
    }
    garage.lowerChassis(host);
}

// 0x00439AD0: Menu emptied; +0x0C and +0x10 the car's decal (+0x0C); +0x14 and +0x18 the place of the car's decal
// colour (+0x38) in [DecalColour] (0 when not there); an item for each part of the CarDef's decal list: its picture
// UI_ and the part's mesh name from "DECAL" on, the first with "LOCKED" (775) unless the car is a prototype
// ([0x0075ED0C]), the others valued their place; with ALLOW_NONE "None" (164, value 127), the first highlighted for
// a car with a decal not 0x7F. The base enter; the price and its box come in; the garage's texts cleared; the
// DECAL_COLOUR elements hidden, +0x1C 1; DECAL_COLOUR_CHART looks for the car's decal colour, kept at +0x2C.
void DecalMenu::enter(GarageHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr || car->def == nullptr || data == nullptr) return;
    m_own = static_cast<int8_t>(car->body[8]);
    m_shown = m_own;
    int32_t colour = 0;
    const std::vector<uint32_t>& decalColours = data->defs.decals;
    for (size_t i = 0; i < decalColours.size(); ++i) {
        if (decalColours[i] == car->decalColour) {
            colour = static_cast<int32_t>(i);
            break;
        }
    }
    m_colours[0] = colour;
    m_colours[1] = colour;
    if (s.find("Menu") >= 0) {
        s.clearMenu("Menu");
        const CarPartList& list = car->def->parts[8];
        for (size_t i = 0; i < list.parts.size(); ++i) {
            const char* decal = std::strstr(list.parts[i].model.c_str(), "DECAL");
            const std::string picture = std::string("UI_") + (decal != nullptr ? decal : "");
            if (decalStyle(*data, *car, float(i)) == 0.f) {
                s.addMenuItem("Menu", garage.texts.prototype ? L"" : texts.at(kTextLocked), float(i), picture, picture);
            } else {
                s.addMenuItem("Menu", L"", static_cast<float>(i), picture, picture);
            }
#if defined(OJ_MACOS) || defined(OJ_LINUX) || defined(OJ_ANDROID)
            if(mods::feature("logo") && int(i)==mods::logoIndex(car->modelName())) {
                s.setMenuItemText("Menu",s.menuSize("Menu")-1,L"CUSTOM LOGO");
            }
#endif
            if (list.parts[i].model.starts_with("DECAL_NFS_")) {
                const auto& part = list.parts[i];
                const auto caption = part.label.substr(0, part.label.find(':')) + " " +
                                     part.model.substr(part.model.size() - 4);
                s.setMenuItemText("Menu", s.menuSize("Menu") - 1, toWide(caption));
                s.setMenuItemHelp("Menu", s.menuSize("Menu") - 1, toWide(part.label));
            }
            s.setHidden("Menu", false);
        }
        if (list.allowNone) {
            s.addCommandItem("Menu", texts.at(kTextNone), 127.0f, "");
            if (m_shown != 0x7F) s.selectMenuItem("Menu", 0);
        }
    }
    host.baseEnter(s, "decalmenu");
    garage.priceShown(host, true);
    garage.clear(host);
    s.hideContaining("DECAL_COLOUR");
    m_menuShown = 1;
    if (s.find("DECAL_COLOUR_CHART") >= 0) {
        s.chartFind("DECAL_COLOUR_CHART", car->decalColour);
        m_colourOwn = car->decalColour;
    }
}

// 0x00439E50: [0x0075F108] 2. Bought: stylingmenu. With the chart shown (+0x1C 0) its colour is the car's decal
// colour (+0x38) on the model, GRIDX and GRIDY "X<column>", "Y<row>". The highlighted decal's price (none 0, the
// first 12000, up to 14 1000, under 23 5000, else 9000); another than +0x0C goes on the car and the model and +0x0C
// becomes Menu's highlighted place. SELECT: the first decal on a car not a prototype notes "Logo for prototype cars
// only" (778, type 1). The car's own decal (or none on a car without one) with the colour places alike (+0x18, +0x14,
// which nothing changes): CLICK; a colourable decal (1 .. 14) with the list shown shows the chart (the DECAL_COLOUR
// elements in, Menu hidden); the chart shown: another colour buymod, the same (or another decal) "You already have
// this mod". Another decal: a colourable one with the list shown shows the chart; else CLICK and buymod. BACK: the
// chart shown: back to the list (the DECAL_COLOUR elements out, Menu shown), the colour back, CLICK; else CLICK,
// stylingmenu, the colour and the decal back, the garage's texts cleared. The chassis lowered.
void DecalMenu::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr || data == nullptr) return;
    if (garage.texts.bought != 0) host.request("stylingmenu", SwitchMode::Swap);
    const bool hasMenu = s.find("Menu") >= 0 && s.menuSize("Menu") != 0;
    float value = 0.0f;
    if (hasMenu) {
        const size_t item = s.selectedMenuItem("Menu");
        value = s.menuItemValue("Menu", item);
        if (m_menuShown == 0) {
            car->decalColour = s.chartColour("DECAL_COLOUR_CHART");
            if (garage.view && garage.view->car() != nullptr) garage.view->car()->colourDecal(*car);
            if (s.find("DECAL_COLOUR_GRIDX") >= 0) {
                s.setText("DECAL_COLOUR_GRIDX", toWide(std::format("X{}", s.chartColumn("DECAL_COLOUR_CHART"))));
            }
            if (s.find("DECAL_COLOUR_GRIDY") >= 0) {
                s.setText("DECAL_COLOUR_GRIDY", toWide(std::format("Y{}", s.chartRow("DECAL_COLOUR_CHART"))));
            }
        }
        const float style = decalStyle(*data, *car, value);
        garage.price(host, decalPrice(style));
        if (x87::chop(value) != m_shown) {
            garage.fitDecal(*car, x87::chop(value));
            m_shown = static_cast<int32_t>(item);
        }
        if (event == input::kSelect) {
            const bool colourable = style <= 14.0f && style != 0.0f;
            const auto showChart = [&] {
                s.showContaining("DECAL_COLOUR", true);
                m_menuShown = 0;
                s.setHidden("Menu", true);
            };
            if (style == 0.0f && !garage.texts.prototype) {
                host.message(MessageType::Continue, texts.at(kTextPrototypeLogo), false, nullptr);
            } else if ((value == static_cast<float>(m_own) || (m_own == -1 && value == 127.0f)) &&
                       m_colours[1] == m_colours[0]) {
                host.sound("CLICK");
                if (colourable && m_menuShown != 0) {
                    showChart();
                } else if (colourable && m_colourOwn != car->decalColour) {
                    host.request("buymod", SwitchMode::Push);
                } else {
                    host.message(MessageType::Continue, texts.at(kTextHaveMod), false, nullptr);
                }
            } else if (colourable && m_menuShown != 0) {
                showChart();
            } else {
                host.sound("CLICK");
                host.request("buymod", SwitchMode::Push);
            }
        }
    }
    if (event == input::kBack) {
        if (m_menuShown == 0) {
            s.hideContaining("DECAL_COLOUR", true);
            m_menuShown = 1;
            if (hasMenu) s.setHidden("Menu", false);
            car->decalColour = m_colourOwn;
            if (garage.view && garage.view->car() != nullptr) garage.view->car()->colourDecal(*car);
            host.sound("CLICK");
        } else {
            host.sound("CLICK");
            host.request("stylingmenu", SwitchMode::Swap);
            car->decalColour = m_colourOwn;
            garage.fitDecal(*car, m_own);
            garage.clear(host);
        }
    }
    garage.lowerChassis(host);
}

// 0x0043A520: MENU back to its items (0x005AECF0: the window is kept, the items taken out last time come back); with
// the sponsor decals shown (the car's +0x48) show taken out, else remove and colour; the help of the highlighted item
// again (0x005AF860).
void SponsorDecalMenu::enter(GarageHost& host, ui::Screen& s) {
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr || s.find("MENU") < 0) return;
    s.restoreMenu("MENU");
    if (car->sponsorDecals != 0) {
        s.removeMenuItem("MENU", "show");
    } else {
        s.removeMenuItem("MENU", "remove");
        s.removeMenuItem("MENU", "colour");
    }
    s.refreshMenuHelp("MENU");
}

// 0x0043A590: [0x0075F108] 2. SELECT (CLICK): remove hides them (+0x48 0) and goes to stylingmenu; show shows them
// (+0x48 1), colour keeps them: the garage's texts cleared, the decal paint (3) and colourchoose; the model's parts
// again (0x0048B440, 0x0048B690). BACK (CLICK): stylingmenu. The chassis lowered.
void SponsorDecalMenu::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr) return;
    if (event == input::kSelect && s.find("MENU") >= 0 && s.menuSize("MENU") != 0) {
        host.sound("CLICK");
        const std::string command = s.menuItemCommand("MENU", s.selectedMenuItem("MENU"));
        if (_stricmp(command.c_str(), "remove") == 0) {
            car->sponsorDecals = 0;
            host.request("stylingmenu", SwitchMode::Swap);
        } else {
            garage.clear(host);
            if (_stricmp(command.c_str(), "show") == 0) car->sponsorDecals = 1;
            garage.paintMode = PaintMode::SponsorDecals;
            host.request("colourchoose", SwitchMode::Swap);
        }
        garage.refreshParts(host);
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request("stylingmenu", SwitchMode::Swap);
    }
    garage.lowerChassis(host);
}

}  // namespace oj::frontend
