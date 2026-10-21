// The performance parts: tuningmenu, performtype, performmod, buymod, the performance meters
// (performance) and the suppliers.

#include "Game/Frontend/Garage/Customise/Tuning.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/Garage/CarStats.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>

namespace oj::frontend {

namespace {

constexpr oj::Vec4 kCameraSide{-500.0f, 100.0f, 0.0f, 0.0f};           // 0x00720408
constexpr oj::Vec4 kCameraEngine{130.0f, 100.0f, 350.0f, 0.0f};        // 0x00720488
constexpr oj::Vec4 kCameraWheel{-200.0f, 30.0f, 230.0f, 0.0f};         // 0x00720498
constexpr oj::Vec4 kCameraExhaust{40.0f, 50.0f, -460.0f, 0.0f};        // 0x007204B8

// 0x00498F40: the suppliers (+0x2A) of a kind's parts of a level, each once in the list's order.
std::vector<int> modBrands(const CarPartList& list, int level) {
    std::vector<int> brands;
    for (const CarPart& part : list.parts) {
        if (part.level != level) continue;
        const int brand = static_cast<int8_t>(part.brand);
        if (std::find(brands.begin(), brands.end(), brand) == brands.end()) brands.push_back(brand);
    }
    return brands;
}

// The kinds whose part changes no rating shown: the suspension (0), nitro (2), brakes (5), tyres (6), gearbox (9).
bool ratingShown(int kind) {
    return kind != kTyre && kind != kGearbox && kind != kNitro && kind != kBrake && kind != kSuspension;
}

}  // namespace

// 0x0043A780: the base enter; the car rated (0x0048E8A0) and its model's sponsor decals again (0x0048B440,
// 0x0048B690); its class shown (0x00447170); carsel's view in the map mode (0x00436010); menu back to its items
// (0x005AECF0) and without a suspension part the ride item taken out (0x005AFAD0); the highlighted item's help to the
// ticker (0x005B5240); another car than +0x10 highlights the first item (+0x10 the car).
void TuningMenu::enter(GarageHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    Garage& garage = host.garage();
    host.baseEnter(s, "tuningmenu");
    Car* car = garage.car(host);
    if (car == nullptr) return;
    rateCar(*car, car->def != nullptr ? data->dynamics->of(*car->def) : nullptr, data->catalogue);
    if (garage.view && garage.view->car() != nullptr) garage.view->car()->sponsorDecals(*car, data->catalogue);
    garage.carInfo(host);
    if (garage.view) garage.view->orbit();
    s.restoreMenu("menu");
    if (car->mods[kSuspension] == kNoMod) s.removeMenuItem("menu", "ride");
    s.refreshMenuHelp("menu");
    if (m_car != car) {
        s.selectMenuItem("menu", 0);
        m_car = car;
    }
}

// 0x0043A860: the highlighted item kept; the car's model object's +0xE8 0; +0x0C 1.
void TuningMenu::leave(GarageHost& host, ui::Screen& s) {
    host.baseLeave(s, "tuningmenu");
    m_returning = 1;
}

// 0x0043A8B0: [0x0075F108] 2; with the money under 10000 the window's help page (as 0x004850E0, once); back from a
// window (+0x0C) menu shown with its transition. SELECT on Menu's item (the model object's +0xE8 0): suspension (kind
// 0, the camera to the side), induction (8), nitro (2), turbo (3, these three and the chip the camera to the engine),
// exhaust (1, to the exhaust), brake (5) and tyre (6, to the wheel) CLICK and performtype; ride (kind 0, the side)
// suspensionmenu; gears (9) gearboxmenu; chip enginechip (each in place of this window); another item CLICK. PGDN:
// car_stats2 over the window (its garage flag [0x00721DE8] 1), every menu hidden and +0x0C 1. BACK (CLICK): custmenu.
// The chassis lowered.
void TuningMenu::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    Garage& garage = host.garage();
    if (data == nullptr || !data->career) return;
    if (static_cast<float>(data->career->profile.money) < 10000.0f) host.helpPage("tuningmenu", false);
    if (m_returning != 0) {
        s.show("menu", true);
        m_returning = 0;
    }
    if (event == input::kSelect) {
        if (s.find("Menu") >= 0 && s.menuSize("Menu") != 0) {
            const std::string command = s.menuItemCommand("Menu", s.selectedMenuItem("Menu"));
            const auto is1 = [&](const char* name) { return _stricmp(command.c_str(), name) == 0; };
            const auto part = [&](int kind, const oj::Vec4& camera, const char* window) {
                host.sound("CLICK");
                if (garage.view) garage.view->cameraTo(camera);
                garage.kind = kind;
                host.request(window, SwitchMode::Swap);
            };
            if (is1("suspension")) {
                part(kSuspension, kCameraSide, "performtype");
            } else if (is1("ride")) {
                part(kSuspension, kCameraSide, "suspensionmenu");
            } else if (is1("induction")) {
                part(kInduction, kCameraEngine, "performtype");
            } else if (is1("exhaust")) {
                part(kExhaust, kCameraExhaust, "performtype");
            } else if (is1("nitro")) {
                part(kNitro, kCameraEngine, "performtype");
            } else if (is1("turbo")) {
                part(kTurbo, kCameraEngine, "performtype");
            } else if (is1("brake")) {
                part(kBrake, kCameraWheel, "performtype");
            } else if (is1("tyre")) {
                part(kTyre, kCameraWheel, "performtype");
            } else if (is1("gears")) {
                host.sound("CLICK");
                garage.kind = kGearbox;
                host.request("gearboxmenu", SwitchMode::Swap);
            } else if (is1("chip")) {
                host.sound("CLICK");
                if (garage.view) garage.view->cameraTo(kCameraEngine);
                host.request("enginechip", SwitchMode::Swap);
            } else {
                host.sound("CLICK");
            }
        }
    } else if (event == input::kSelect2) {
        host.carStats().setGarage(1);
        host.request("car_stats2", SwitchMode::Push);
        s.setHidden("menu", true);
        m_returning = 1;
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request("custmenu", SwitchMode::Swap);
    }
    if (Car* car = garage.car(host); car != nullptr && garage.view) garage.view->dropChassis(*car);
}

// 0x00441560: the base enter; menu emptied; the refund (+0x1C of the garage's texts) 0; with a part of the kind on the
// car "STOCK" (102, value 1, stock) first; the first item highlighted; LOCKED hidden (+0x14); then "LEVEL 1" ..
// "LEVEL 3", "PROTOTYPE" (547 ..; street, sports, competition, prototype): a level past the one the profile unlocked
// for the model and kind (its +0x50) valued -1 (locked), the car's part's level ticked (ui_yes); +0x0C 1, +0x10 0.
void PerformType::enter(GarageHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    host.baseEnter(s, "performtype");
    Car* car = garage.car(host);
    if (car == nullptr || data == nullptr || !data->career) return;
    const auto kind = static_cast<size_t>(std::clamp(garage.kind, 0, 9));
    const auto model = static_cast<size_t>(std::clamp(car->def != nullptr ? car->def->id : 0, 0, 52));
    const int unlocked = data->career->profile.partsA[model][kind];
    s.clearMenu("menu");
    garage.texts.refund = 0.0f;
    if (car->mods[kind] != kNoMod) s.addCommandItem("menu", texts.at(kTextStockItem), 1.0f, "stock");
    if (s.menuSize("menu") != 0) s.selectMenuItem("menu", 0);
    s.setHidden("locked", true);
    static const char* const kLevels[] = {"street", "sports", "competition", "prototype"};
    const int own = modLevel(data->catalogue.lists[kind], car->mods[kind]);
    for (int level = 0; level < 4; ++level) {
        s.addCommandItem("menu", texts.at(kTextPartLevels + static_cast<size_t>(level)), 1.0f, kLevels[level]);
        const size_t item = s.menuSize("menu") - 1;
        if (unlocked < level) {
            s.setMenuItemValue("menu", item, -1.0f);
        } else if (car->mods[kind] != kNoMod && own == level) {
            s.setMenuItemTextures("menu", item, "ui_yes", "ui_yes");
        }
    }
    m_answer = 1;
    m_asked = 0;
}

// 0x00441800: [0x0075F108] 2. The removal asked (+0x10): a "yes" (+0x0C) takes the kind's part off (an exhaust also
// off the model, 0x0048BE30; the suspension's ride the car's +0x58 0), gives the refund back (cut), shows the money,
// clears the garage's price, rating, refund and bought and goes to tuningmenu; a "no" clears +0x10. A highlighted item
// valued -1 shows LOCKED, another hides it. SELECT on an item not locked (CLICK): stock asks "REMOVE MOD" (724), with
// a rating shown the rating without the part ("NEW BHP", 675) and "ARE YOU SURE?" (type 3, on NO) into +0x0C (+0x10
// 1); a level: the exhaust (kind 1) suppliers, else performmod (in place of this window), the catalogue's level 0 for
// street, 1 sports, 2 competition, else 3. BACK (CLICK): tuningmenu. The chassis lowered.
void PerformType::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr || data == nullptr || !data->career) return;
    Career& career = *data->career;
    const auto kind = static_cast<size_t>(std::clamp(garage.kind, 0, 9));
    if (m_asked != 0) {
        if (m_answer == 1) {
            car->mods[kind] = kNoMod;
            if (kind == kExhaust) {
                garage.fitExhaust(host, *car, -1);
            } else if (kind == kSuspension) {
                car->lowering = 0;
            }
            pay(career, -x87::chop(garage.texts.refund));
            garage.cash(host);
            garage.texts.price = 0;
            if (!garage.texts.priceText.empty()) host.baseScreen().setText(garage.texts.priceText, L"");
            garage.texts.newRating = 0.0f;
            garage.texts.bought = 0;
            garage.texts.refund = 0.0f;
            host.request("tuningmenu", SwitchMode::Swap);
        } else if (m_answer == 0) {
            m_asked = 0;
        }
    }
    bool locked = false;
    if (s.find("Menu") >= 0 && s.menuSize("Menu") != 0) {
        const size_t item = s.selectedMenuItem("Menu");
        locked = s.menuItemValue("Menu", item) == -1.0f;
        s.setHidden("locked", !locked);
        if (event == input::kSelect && !locked) {
            host.sound("CLICK");
            const std::string command = s.menuItemCommand("Menu", item);
            if (_stricmp(command.c_str(), "stock") == 0) {
                std::wstring text = texts.at(kTextRemoveMod);
                const float rating = garage.rateWithPart(host, *car, kind, 0xFF);
                if (ratingShown(static_cast<int>(kind))) {
                    text += L"\n" + texts.at(kTextNewBhp) + L" " + toWide(std::format("{}", x87::chop(rating)));
                }
                text += L"\n" + texts.at(kTextAreYouSure);
                host.message(MessageType::YesNo, text, false, &m_answer, true);
                m_asked = 1;
            } else {
                host.request(kind == kExhaust ? "suppliers" : "performmod", SwitchMode::Swap);
                if (_stricmp(command.c_str(), "street") == 0) {
                    garage.catalogueLevel = 0;
                } else if (_stricmp(command.c_str(), "sports") == 0) {
                    garage.catalogueLevel = 1;
                } else {
                    garage.catalogueLevel = _stricmp(command.c_str(), "competition") == 0 ? 2 : 3;
                }
            }
            event = input::kNone;
        }
    }
    if (event == input::kBack) {
        host.sound("CLICK");
        host.request("tuningmenu", SwitchMode::Swap);
    }
    if (garage.view) garage.view->dropChassis(*car);
}

// 0x00441D20: the base enter; the garage's price and its box come in (their transitions from the start, shown);
// Menu emptied, its first item highlighted; the kind's suppliers of the catalogue's level (0x00498F40), [0x0075ED08]
// 0; an item each: no text, its place, the pictures UI_<supplier>_OFF and _ON, its name the supplier's; none "NONE
// AVAILABLE" (96) and every element holding PRICE hidden; else Help (+0x0C), the money shown and Help's ticker by the
// kind (3 "Choose new turbo system" 92, 2 nitrous 93, 0 suspension 94, 5 brakes 95, 6 tires 91). +0x10 the car's part
// of the kind.
void PerformMod::enter(GarageHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    host.baseEnter(s, "performmod");
    Car* car = garage.car(host);
    if (car == nullptr || data == nullptr) return;
    for (const std::string& name : {garage.texts.priceText, garage.texts.boxText}) {
        if (!name.empty()) host.baseScreen().restartTransition(name, false);
    }
    const auto kind = static_cast<size_t>(std::clamp(garage.kind, 0, 9));
    s.clearMenu("Menu");
    garage.brands = modBrands(data->catalogue.lists[kind], garage.catalogueLevel);
    garage.partShown = 0;
    const std::vector<ExeTables::Brand>& brands = data->tables.brands;
    for (size_t i = 0; i < garage.brands.size(); ++i) {
        const int brand = garage.brands[i];
        const std::string name =
            brand >= 0 && static_cast<size_t>(brand) < brands.size() ? brands[static_cast<size_t>(brand)].name : "";
        s.addMenuItem("Menu", L"", static_cast<float>(i), "UI_" + name + "_OFF", "UI_" + name + "_ON");
        s.setMenuItemCommand("Menu", s.menuSize("Menu") - 1, name);
    }
    if (s.menuSize("Menu") != 0) s.selectMenuItem("Menu", 0);
    if (garage.brands.empty()) {
        s.addCommandItem("Menu", texts.at(kTextNoneAvailable), 0.0f, "");
        for (int i = 0; i < s.size(); ++i) {
            if (s.nameOf(i).find("PRICE") != std::string::npos) s.setHidden(i, true);
        }
    } else {
        garage.cash(host);
        if (s.find("Help") >= 0) {
            size_t line = 0;
            switch (kind) {
            case kTurbo: line = 92; break;
            case kNitro: line = 93; break;
            case kSuspension: line = 94; break;
            case kBrake: line = 95; break;
            case kTyre: line = 91; break;
            default: break;
            }
            if (line != 0) s.setText("Help", texts.at(line));
        }
    }
    m_current = static_cast<int8_t>(car->mods[kind]);
}

// 0x00441F90: the highlighted item kept; the garage's price and its box go out (or hidden); +0x14 none.
void PerformMod::leave(GarageHost& host, ui::Screen& s) {
    Garage& garage = host.garage();
    host.baseLeave(s, "performmod");
    for (const std::string& name : {garage.texts.priceText, garage.texts.boxText}) {
        if (!name.empty()) host.baseScreen().restartTransition(name, true);
    }
    m_chosen = nullptr;
}

// 0x00442020: [0x0075F108] 2. A part bought (the garage's +0x2C) with +0x14: the part fitted (0x0048BD70: the kind's
// part; the nitro full, the tyres' and the brakes' wear 0) and tuningmenu. With no window over the windows
// `performance` over them (mode 8). With suppliers: the highlighted item's supplier
// ([0x00715C30]), its parts of the level (0x004990F0), +0x14 the first and the garage's price its COST; SELECT: the
// car's own part notes "You already have this mod" (type 1); else the rating with the part (when shown), +0x14 the
// first, the refund 0, CLICK and buymod over the window. BACK (CLICK): performtype; the garage's price, rating,
// bought and refund cleared. The chassis lowered.
void PerformMod::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr || data == nullptr) return;
    const auto kind = static_cast<size_t>(std::clamp(garage.kind, 0, 9));
    if (garage.texts.bought != 0 && m_chosen != nullptr) {
        car->mods[kind] = static_cast<uint8_t>(m_chosen->index);
        if (kind == kNitro) {
            car->nitro = Car::bitsOf(15.0f);
        } else if (kind == kTyre) {
            car->tyreWear = 0;
        } else if (kind == kBrake) {
            car->brakeWear = 0;
        }
        host.request("tuningmenu", SwitchMode::Swap);
        return;
    }
    if (!host.overlayUp()) host.request("performance", SwitchMode::Overlay);
    bool handled = false;
    if (!garage.brands.empty() && s.find("Menu") >= 0 && s.menuSize("Menu") != 0) {
        const int i = x87::chop(s.menuItemValue("Menu", s.selectedMenuItem("Menu")));
        const int last = static_cast<int>(garage.brands.size()) - 1;
        garage.brand = i == -1 ? -1 : garage.brands[static_cast<size_t>(std::clamp(i, 0, last))];
        garage.filtered = modBrandParts(data->catalogue.lists[kind], garage.brand, garage.catalogueLevel);
        m_chosen = garage.filtered.empty() ? nullptr : garage.filtered.front();
        if (m_chosen != nullptr) garage.price(host, m_chosen->cost);
        if (event == input::kSelect && m_chosen != nullptr) {
            handled = true;
            if (static_cast<int32_t>(m_chosen->index) == m_current) {
                host.message(MessageType::Continue, texts.at(kTextHaveMod), false, nullptr);
            } else {
                const float rating = garage.rateWithPart(host, *car, kind, static_cast<uint8_t>(m_chosen->index));
                garage.texts.newRating = ratingShown(static_cast<int>(kind)) ? rating : 0.0f;
                garage.texts.refund = 0.0f;
                host.sound("CLICK");
                host.request("buymod", SwitchMode::Push);
            }
        }
    }
    if (!handled && event == input::kBack) {
        host.sound("CLICK");
        host.request("performtype", SwitchMode::Swap);
        garage.texts.price = 0;
        if (!garage.texts.priceText.empty()) host.baseScreen().setText(garage.texts.priceText, L"");
        garage.texts.newRating = 0.0f;
        garage.texts.bought = 0;
        garage.texts.refund = 0.0f;
    }
    if (garage.view) garage.view->dropChassis(*car);
}

// 0x00446750: the money under the garage's price: "INSUFFICIENT CASH" (type 1), NO and the window closed. Else "\n",
// "NEW MOD PRICE" (1829) and the price, with a refund "\n", "CURRENT MOD REFUND" (1828) and it (cut), "\n"; with a
// rating (+0x18 over 0): for another class "\n", "NEW CLASS" (676) and its name, "\n"; "NEW BHP" (675) and the
// rating (cut), "\n" and "ARE YOU SURE?"; without "\n\n" and "ARE YOU SURE?" (type 3, on NO) into +0x0C, the
// message's title "CLASS CHANGE" (1787, 0x005A8330) for another class. The rating 0.
void BuyMod::enter(GarageHost& host, ui::Screen& /*s*/) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    if (data == nullptr || !data->career) return;
    Car* car = garage.car(host);
    const int language = host.language().index;
    if (data->career->profile.money < garage.texts.price) {
        host.message(MessageType::Continue, texts.at(kTextInsufficientCash), false, nullptr);
        host.sound("NO");
        host.request("", SwitchMode::Pop);
        garage.texts.newRating = 0.0f;
        return;
    }
    std::wstring text = L"\n" + texts.at(kTextRefund + 1) + L" " + toWide(moneyText(garage.texts.price, language));
    if (garage.texts.refund > 0.0f) {
        text += L"\n" + texts.at(kTextRefund) + L" " + toWide(moneyText(x87::chop(garage.texts.refund), language));
    }
    text += L"\n";
    if (garage.texts.newRating > 0.0f) {
        const int newClass = ratingClass(garage.texts.newRating);
        if (car != nullptr && carClass(*car) != newClass) {
            text += L"\n" + texts.at(kTextNewBhp + 1) + L" " +
                    texts.at(kTextClasses + static_cast<size_t>(newClass)) + L"\n";
        }
        text += texts.at(kTextNewBhp) + L" " + toWide(std::format("{}", x87::chop(garage.texts.newRating))) + L"\n" +
                texts.at(kTextAreYouSure);
        host.message(MessageType::YesNo, text, false, &m_answer, true);
        if (car != nullptr && carClass(*car) != newClass) host.messageTitle(texts.at(kTextClassChange));
    } else {
        text += L"\n\n" + texts.at(kTextAreYouSure);
        host.message(MessageType::YesNo, text, false, &m_answer, true);
    }
    garage.texts.newRating = 0.0f;
}

// 0x00446B80: a "yes" (+0x0C): the price less the refund (cut) paid and noted on the car (0x0048F270), the money shown,
// the garage's price emptied, rating and refund 0, bought (+0x2C) 1; the window closed either way.
void BuyMod::update(GarageHost& host) {
    GameData* data = host.gameData();
    Garage& garage = host.garage();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    if (m_answer == 1) {
        const float cost = static_cast<float>(garage.texts.price) - garage.texts.refund;
        pay(career, x87::chop(cost));
        if (Car* car = garage.car(host); car != nullptr) {
            car->spent = std::bit_cast<uint32_t>(std::bit_cast<float>(car->spent) + cost);
            CareerProfile& p = career.profile;
            const auto spent = static_cast<float>(static_cast<int32_t>(p.moneySpent));
            p.moneySpent = static_cast<uint32_t>(x87::chop(spent + cost));
        }
        garage.cash(host);
        garage.texts.price = 0;
        if (!garage.texts.priceText.empty()) host.baseScreen().setText(garage.texts.priceText, L"");
        garage.texts.newRating = 0.0f;
        garage.texts.refund = 0.0f;
        garage.texts.bought = 1;
    }
    host.request("", SwitchMode::Pop);
}

// 0x004423C0: the base enter; Speed (+0x10), Accel (+0x0C), Braking (+0x14) and Handling (+0x18); +0x1C [0x0075ED08],
// +0x20 [0x00715C30].
void Performance::enter(GarageHost& host, ui::Screen& s) {
    Garage& garage = host.garage();
    host.baseEnter(s, "performance");
    m_part = garage.partShown;
    m_brand = garage.brand;
}

// 0x00442420: [0x0075F108] 2. The gearbox (kind 9): the car's gearing (+0x5C) under 0.5 Speed (0.5 - it) x 2 and
// Accel 0, else Speed 0 and Accel (it - 0.5) x 2; Braking and Handling 0. Else the supplier's parts of the level
// (0x004990F0): the one at [0x0075ED08] gives Speed, Accel, Braking and Handling its PHYSICS b, a, c, d / 255; another
// part or supplier than before shows the meters from 0.
void Performance::update(GarageHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr || data == nullptr) return;
    if (garage.kind == kGearbox) {
        const float gearing = Car::asFloat(car->gearing);
        if (gearing < 0.5f) {
            s.setMeter("Speed", (0.5f - gearing) * 2.0f);
            s.setMeter("Accel", 0.0f);
        } else {
            s.setMeter("Speed", 0.0f);
            s.setMeter("Accel", (gearing - 0.5f) * 2.0f);
        }
        s.setMeter("Braking", 0.0f);
        s.setMeter("Handling", 0.0f);
        return;
    }
    const auto kind = static_cast<size_t>(std::clamp(garage.kind, 0, 9));
    garage.filtered = modBrandParts(data->catalogue.lists[kind], garage.brand, garage.catalogueLevel);
    if (garage.partShown >= 0 && static_cast<size_t>(garage.partShown) < garage.filtered.size()) {
        const CarPart& part = *garage.filtered[static_cast<size_t>(garage.partShown)];
        s.setMeter("Speed", static_cast<float>(part.physics[1]) * kByteShare);
        s.setMeter("Accel", static_cast<float>(part.physics[0]) * kByteShare);
        s.setMeter("Braking", static_cast<float>(part.physics[2]) * kByteShare);
        s.setMeter("Handling", static_cast<float>(part.physics[3]) * kByteShare);
    }
    if (m_part != garage.partShown || m_brand != garage.brand) {
        for (const char* name : {"Speed", "Accel", "Braking", "Handling"}) s.setMeterShown(name, 0.0f);
    }
    m_brand = garage.brand;
    m_part = garage.partShown;
}

// 0x00440B80: Menu emptied; for the wheels (kind 4) with a wheel on the car "STOCK" (102, value -1) first; an item for
// each supplier of the kind's parts of the catalogue's level (0x00498F40): no text, its place, UI_<supplier>_OFF and
// _ON, the supplier's name; then the base enter.
void Suppliers::enter(GarageHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (data != nullptr && s.find("Menu") >= 0) {
        const auto kind = static_cast<size_t>(std::clamp(garage.kind, 0, 9));
        s.clearMenu("Menu");
        if (garage.kind == kWheel && car != nullptr && car->mods[kWheel] != kNoMod) {
            s.addMenuItem("Menu", texts.at(kTextStockItem), -1.0f);
        }
        garage.brands = modBrands(data->catalogue.lists[kind], garage.catalogueLevel);
        const std::vector<ExeTables::Brand>& brands = data->tables.brands;
        for (size_t i = 0; i < garage.brands.size(); ++i) {
            const int brand = garage.brands[i];
            const std::string name =
                brand >= 0 && static_cast<size_t>(brand) < brands.size() ? brands[static_cast<size_t>(brand)].name : "";
            s.addMenuItem("Menu", L"", static_cast<float>(i), "UI_" + name + "_OFF", "UI_" + name + "_ON");
            s.setMenuItemCommand("Menu", s.menuSize("Menu") - 1, name);
        }
    }
    host.baseEnter(s, "suppliers");
}

// 0x00440CC0: [0x0075F108] 2. SELECT on Menu's item: its supplier ([0x00715C30]; -1 for the value -1), CLICK, and by
// the kind suspensionmenu (0), exhaustmenu (1) or wheelsmenu (4). BACK (CLICK): stylingmenu for the wheels, else
// performtype. The chassis lowered.
void Suppliers::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    Garage& garage = host.garage();
    if (event == input::kSelect) {
        if (s.find("Menu") >= 0 && s.menuSize("Menu") != 0) {
            const int i = x87::chop(s.menuItemValue("Menu", s.selectedMenuItem("Menu")));
            garage.brand = i == -1 || i < 0 || static_cast<size_t>(i) >= garage.brands.size()
                              ? -1
                              : garage.brands[static_cast<size_t>(i)];
            host.sound("CLICK");
            if (garage.kind == kSuspension) {
                host.request("suspensionmenu", SwitchMode::Swap);
            } else if (garage.kind == kExhaust) {
                host.request("exhaustmenu", SwitchMode::Swap);
            } else if (garage.kind == kWheel) {
                host.request("wheelsmenu", SwitchMode::Swap);
            }
        }
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request(garage.kind == kWheel ? "stylingmenu" : "performtype", SwitchMode::Swap);
    }
    garage.lowerChassis(host);
}

}  // namespace oj::frontend
