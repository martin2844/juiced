// modshop: the upgrades to the next classes.

#include "Game/Frontend/Garage/ModShop.h"

#include "Core/Base/Log.h"
#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>

namespace oj::frontend {

namespace {

// A catalogue part by its place; the list's first past its end.
const CarPart& modPart(const CarPartList& list, int32_t part) {
    return list.parts[part >= 0 && static_cast<size_t>(part) < list.parts.size() ? static_cast<size_t>(part) : 0];
}

// The class of a rating as modshop takes it: 7 under 200, then one less for each 100 up to 0 from 800.
int modClass(float rating) {
    if (rating < 200.0f) return 7;
    if (rating < 300.0f) return 6;
    if (rating < 400.0f) return 5;
    if (rating < 500.0f) return 4;
    if (rating < 600.0f) return 3;
    if (rating < 700.0f) return 2;
    if (rating < 800.0f) return 1;
    return 0;
}

// The elements of the catalogue's kinds: ICON_, LEVEL_ and LOGO_ (0x00444664); none for the others ("nowt").
const char* modKindName(size_t kind) {
    switch (kind) {
    case kSuspension: return "SUSPENSION";
    case kExhaust: return "EXHAUST";
    case kNitro: return "NOS";
    case kTurbo: return "TURBO";
    case kBrake: return "BRAKES";
    case kTyre: return "TYRES";
    case kInduction: return "INDUCTION";
    default: return nullptr;
    }
}

}  // namespace

// The catalogue's draw of a part of a kind and level: the suppliers (+0x2A) of the kind's parts of the level, each
// once in the list's order (0x00498F40), one of them by rand() % their number, its parts of the level (0x004990F0,
// kept as the catalogue's +0x104) and the first one's place (+0x24). Stock problem: without a part of the level the
// draw divides by zero and the first part is read through a null pointer; here rand() is drawn and nothing chosen
// (-1).
int32_t ModShop::drawPart(GarageHost& host, size_t kind, int level) {
    GameData* data = host.gameData();
    Garage& garage = host.garage();
    const CarPartList& list = data->catalogue.lists[kind];
    std::vector<int> brands;
    for (const CarPart& part : list.parts) {
        if (part.level != level) continue;
        const int brand = static_cast<int8_t>(part.brand);
        if (std::find(brands.begin(), brands.end(), brand) == brands.end()) brands.push_back(brand);
    }
    const auto r = static_cast<uint32_t>(gameRandom().next());
    if (brands.empty()) return -1;
    const int brand = brands[r % brands.size()];
    garage.filtered.clear();
    for (const CarPart& part : list.parts) {
        if (static_cast<int8_t>(part.brand) == brand && part.level == level) garage.filtered.push_back(&part);
    }
    return garage.filtered.empty() ? -1 : static_cast<int32_t>(garage.filtered[0]->index);
}

// 0x00442A80: the upgrades. The car's induction, exhaust and turbo kept; each level (-1 none .. 3) of the turbo, then
// the exhaust, then the induction, all three none left out, none past the levels the profile unlocked for the model
// (+0x50) nor below the car's: the car takes the three (for a level a part drawn, 0x00498F40 and 0x004990F0, put on
// unless the car's own is of that level or more; -1 none) and is rated (0x0048E8A0; ITER .. printed for debugging: not
// followed). A rating whose whole number (to the nearest) passes the car's (cut) and passes the best of its class (7
// under 200 .. 0 from 800) found before takes that class: the nitro, the suspension, the brakes and the tyres up to the
// highest of the three levels and the profile's, each drawn when above the car's; the class's parts the car's
// induction, exhaust and turbo; the cost the parts' COST (the three only when not of the car's levels) and the meters:
// the three's PHYSICS a and b (/ 255) into accel and speed, the nitro sets speed (0.35, 0.25, 0.1 by its level 2, 1,
// 0), the suspension's d into handling, the brakes' c into braking, the tyres' c and d into braking and handling; accel
// x 0.3, handling and braking x 0.4, each held within 0 .. 1; the three of the car's levels none again; a class with no
// part none again. Stock problem: the meters and the parts drawn before stay with a class taken again, so the meters
// add up (and are scaled again) and a part of a weaker set may stay; the fixed build starts the set anew. Then the
// car's three back and rated again; with the nitro's level unlocked above the car's, a class the car's own rating
// takes, found by nothing yet, gets the nitro of that level drawn (when above the car's), its cost and speed. Every
// class found, from 7 to 0, makes an upgrade (+0x04 the class + 1, +0x08 1); menu and backmenu emptied, for each
// upgrade from the last "UPGRADE CLASS <n> " (1792, 1791) with its place, backmenu a bar ui_pan5_green drawn adding;
// both on their last item. cash the money; BHP, PRICE, CLASS, CLASSMETER, Speed, Accel, Braking and Handling hidden;
// CARNAME the model's display name; for each kind with them ICON_, LEVEL_ and LOGO_ hidden; +0x310 none, +0x3B4 -1, the
// analysis 3 seconds and 3 beeps; ANALYSIS over several lines.
void ModShop::enter(GarageHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    *this = ModShop{};
    garage.filtered.clear();  // the catalogue's list, kept in the record here before
    if (data == nullptr || !data->career) return;
    Car* car = garage.car(host);
    if (car == nullptr) return;
    const PartCatalogue& catalogue = data->catalogue;
    const auto& lists = catalogue.lists;
    const auto model = static_cast<size_t>(std::clamp(car->def != nullptr ? car->def->id : 0, 0, 52));
    const std::array<int8_t, 10>& unlocked = data->career->profile.partsA[model];
    const CarDynamics* dynamics = car->def != nullptr ? data->dynamics->of(*car->def) : nullptr;
    const auto rate = [&] { return rateCar(*car, dynamics, catalogue); };

    const int ownRating = x87::chop(Car::asFloat(car->rating));
    const uint8_t keepInduction = car->mods[kInduction];
    const uint8_t keepExhaust = car->mods[kExhaust];
    const uint8_t keepTurbo = car->mods[kTurbo];
    const int levelInduction = modLevel(lists[kInduction], keepInduction);
    const int levelExhaust = modLevel(lists[kExhaust], keepExhaust);
    const int levelTurbo = modLevel(lists[kTurbo], keepTurbo);
    const int levelNitro = modLevel(lists[kNitro], car->mods[kNitro]);

    struct Slot {
        int32_t found = 0;
        float rating = 0.0f, cost = 0.0f, speed = 0.0f, accel = 0.0f, braking = 0.0f, handling = 0.0f;
        std::array<int32_t, 10> parts{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    };
    std::array<Slot, 8> slots{};

    // A kind fitted on the car for a level: a part drawn and put on above the car's own level; none for -1.
    const auto fit = [&](size_t kind, int level) {
        if (level == -1) {
            car->mods[kind] = kNoMod;
            return;
        }
        const int32_t part = drawPart(host, kind, level);
        if (car->mods[kind] == kNoMod || level > modLevel(lists[kind], car->mods[kind])) {
            car->mods[kind] = static_cast<uint8_t>(part);
        }
    };
    // A kind of the class: the profile's level (at most `top`) drawn when above the car's.
    const auto offer = [&](size_t kind, int top, int32_t& into) {
        int level = unlocked[kind];
        if (level <= -1) return;
        if (level > top) level = top;
        if (car->mods[kind] != kNoMod && level <= modLevel(lists[kind], car->mods[kind])) return;
        into = drawPart(host, kind, level);
    };
    const auto addCost = [](float& cost, const CarPart& part) { cost = static_cast<float>(part.cost) + cost; };
    const auto add = [](float& meter, uint8_t value) { meter = static_cast<float>(value) * kByteShare + meter; };
    const auto nitroSpeed = [](float& speed, const CarPart& part) {
        if (part.level == 2) {
            speed = 0.35f;
        } else if (part.level == 1) {
            speed = 0.25f;
        } else if (part.level == 0) {
            speed = 0.1f;
        }
    };
    const auto hold = [](float& v) {
        if (v < 0.0f) {
            v = 0.0f;
        } else if (v > 1.0f) {
            v = 1.0f;
        }
    };

    for (int t = -1; t < 4; ++t) {
        for (int e = -1; e < 4; ++e) {
            for (int i = -1; i < 4; ++i) {
                if (i == -1 && e == -1 && t == -1) continue;
                if (i > unlocked[kInduction] || e > unlocked[kExhaust] || t > unlocked[kTurbo]) continue;
                if (i < levelInduction || e < levelExhaust || t < levelTurbo) continue;
                car->mods[kInduction] = keepInduction;
                car->mods[kExhaust] = keepExhaust;
                car->mods[kTurbo] = keepTurbo;
                fit(kInduction, i);
                fit(kExhaust, e);
                fit(kTurbo, t);
                const float rating = rate();
                const int c = modClass(rating);
                const auto whole = static_cast<int>(std::nearbyint(rating));
                const int top = std::max({-1, i, e, t});
                if (whole <= ownRating) continue;
                Slot& slot = slots[static_cast<size_t>(c)];
                if (slot.found != 0 && !(rating > slot.rating)) continue;
                // The fixed build: a class taken again by a stronger set starts its set anew (the draws as before).
                slot = Slot{};
                slot.rating = rating;
                slot.found = 1;
                offer(kNitro, top, slot.parts[kNitro]);
                offer(kSuspension, top, slot.parts[kSuspension]);
                offer(kBrake, top, slot.parts[kBrake]);
                offer(kTyre, top, slot.parts[kTyre]);
                slot.parts[kInduction] = static_cast<int8_t>(car->mods[kInduction]);
                slot.parts[kExhaust] = static_cast<int8_t>(car->mods[kExhaust]);
                slot.parts[kTurbo] = static_cast<int8_t>(car->mods[kTurbo]);
                slot.cost = 0.0f;
                const std::array<std::pair<size_t, bool>, 3> three = {
                    std::pair{kInduction, levelInduction != i}, {kExhaust, levelExhaust != e},
                    {kTurbo, levelTurbo != t}};
                for (const auto& [kind, changed] : three) {
                    if (slot.parts[kind] == -1 || !changed) continue;
                    const CarPart& part = modPart(lists[kind], slot.parts[kind]);
                    addCost(slot.cost, part);
                    add(slot.accel, part.physics[0]);
                    add(slot.speed, part.physics[1]);
                }
                if (slot.parts[kNitro] != -1) {
                    const CarPart& part = modPart(lists[kNitro], slot.parts[kNitro]);
                    addCost(slot.cost, part);
                    nitroSpeed(slot.speed, part);
                }
                if (slot.parts[kSuspension] != -1) {
                    const CarPart& part = modPart(lists[kSuspension], slot.parts[kSuspension]);
                    addCost(slot.cost, part);
                    add(slot.handling, part.physics[3]);
                }
                if (slot.parts[kBrake] != -1) {
                    const CarPart& part = modPart(lists[kBrake], slot.parts[kBrake]);
                    addCost(slot.cost, part);
                    add(slot.braking, part.physics[2]);
                }
                if (slot.parts[kTyre] != -1) {
                    const CarPart& part = modPart(lists[kTyre], slot.parts[kTyre]);
                    addCost(slot.cost, part);
                    add(slot.braking, part.physics[2]);
                    add(slot.handling, part.physics[3]);
                }
                slot.accel = slot.accel * 0.3f;
                slot.handling = slot.handling * 0.4f;
                slot.braking = slot.braking * 0.4f;
                hold(slot.handling);
                hold(slot.braking);
                hold(slot.speed);
                hold(slot.accel);
                if (levelInduction == i) slot.parts[kInduction] = -1;
                if (levelExhaust == e) slot.parts[kExhaust] = -1;
                if (levelTurbo == t) slot.parts[kTurbo] = -1;
                if (std::all_of(slot.parts.begin(), slot.parts.end(), [](int32_t p) { return p == -1; })) {
                    slot.found = 0;
                }
            }
        }
    }
    car->mods[kInduction] = keepInduction;
    car->mods[kExhaust] = keepExhaust;
    car->mods[kTurbo] = keepTurbo;
    rate();
    if (unlocked[kNitro] > levelNitro) {
        const float rating = rate();
        Slot& slot = slots[static_cast<size_t>(modClass(rating))];
        if (slot.found == 0) {
            slot.rating = rating;
            slot.found = 1;
            const int level = unlocked[kNitro];
            if (level > -1 && (car->mods[kNitro] == kNoMod || level > modLevel(lists[kNitro], car->mods[kNitro]))) {
                slot.parts[kNitro] = drawPart(host, kNitro, level);
            }
            if (slot.parts[kNitro] != -1) {
                const CarPart& part = modPart(lists[kNitro], slot.parts[kNitro]);
                addCost(slot.cost, part);
                nitroSpeed(slot.speed, part);
            }
        }
    }
    for (int c = 7; c >= 0; --c) {
        const Slot& slot = slots[static_cast<size_t>(c)];
        if (slot.found == 0) continue;
        ModShopEntry& e = m_entries[static_cast<size_t>(m_count)];
        e.classNumber = c + 1;
        e.found = 1;
        e.rating = slot.rating;
        e.cost = slot.cost;
        e.speed = slot.speed;
        e.accel = slot.accel;
        e.braking = slot.braking;
        e.handling = slot.handling;
        e.parts = slot.parts;
        ++m_count;
    }

    if (s.find("menu") >= 0 && s.find("backmenu") >= 0) {
        s.clearMenu("menu");
        s.clearMenu("backmenu");
        for (int n = 9; n >= 0; --n) {
            const ModShopEntry& e = m_entries[static_cast<size_t>(n)];
            if (e.found == 0) continue;
            const std::wstring text = texts.at(kTextUpgrade) + L" " + texts.at(kTextClassWord) +
                                      toWide(std::format(" {} ", e.classNumber));
            s.addCommandItem("menu", text, static_cast<float>(n), "");
            s.addMenuItem("backmenu", L"", 0.0f, "", "ui_pan5_green");
            s.setMenuItemAdditive("backmenu", s.menuSize("backmenu") - 1, true);
        }
        if (s.menuSize("menu") != 0) s.selectMenuItem("menu", s.menuSize("menu") - 1);
        if (s.menuSize("backmenu") != 0) s.selectMenuItem("backmenu", s.menuSize("backmenu") - 1);
    }
    if (s.find("cash") >= 0) {
        s.setText("cash", toWide(moneyText(data->career->profile.money, host.language().index)));
    }
    for (const char* name : {"BHP", "PRICE", "CLASS", "CLASSMETER", "Speed", "Accel", "Braking", "Handling"}) {
        s.setHidden(name, true);
    }
    if (s.find("CARNAME") >= 0 && car->def != nullptr) s.setText("CARNAME", widenSigned(car->def->displayName));
    for (size_t kind = 0; kind < 10; ++kind) {
        const char* name = modKindName(kind);
        if (name == nullptr) continue;
        for (const char* what : {"ICON_", "LEVEL_", "LOGO_"}) s.setHidden(std::string(what) + name, true);
    }
    if (s.find("ANALYSIS") >= 0) s.setMultiline("ANALYSIS", true);
}

// 0x00444690: [0x0075F108] (the clouds' mode) 2. While the analysis lasts: BACK (CLICK) closes the window (mode 6);
// a beep (WARNING) each time its whole seconds (cut) fall below +0x3B0 (that one less); ANALYSIS shown at the
// seconds' fraction above whole ones as its alpha; menu and backmenu hidden; the frame time taken off; the elements
// holding "carname" hidden (strstr); once over: 0, the message window closed (the front end's +0x48), menu and
// backmenu shown, ANALYSIS hidden, "carname" shown again, BHP, PRICE, CLASS, CLASSMETER and the four meters shown.
// Then, with menu empty: "No upgrades available!" (1799), "\n" and "This car's performance is maxed" (1801, every
// kind unlocked to 3) or "Race this car to unlock or win more mods" (1800) as a note (type 1), the window closed.
// Else the highlighted item's upgrade: bought ("yes", +0x3B4 1): its parts on the car (an exhaust also on the car's
// model, 0x0048BE30; the model's parts and lights again, 0x0048B440 and 0x0048B690), the car rated and
// made ready (0x0048EA80), its cost paid and noted (the car's +0xA8, the profile's +0x4E8), the money and the class
// shown (0x00447100, 0x00447170), the window closed. Another item than +0x310: BHP its rating (cut), CLASS its class
// number; with [0x0075ED08] a place of the catalogue's +0x104 list the meters that part's PHYSICS b, a, c and d /
// 255; CLASSMETER the rating (999 at most) less 100 while over 100 x 0.01 (1 over 900); PRICE its cost (cut) as
// money; the meters Braking, Handling, Accel and Speed its own; for each kind with its elements: a part makes ICON_
// white, LEVEL_ its level + 1 ("P" for 4) and LOGO_ "ui_<supplier>_on" coming in; none makes ICON_ 0x80808080,
// LEVEL_ "-" and LOGO_ go out. SELECT: money over the cost (both as floats) asks "BUY UPGRADE CLASS <n> \n <cost>\n
// ARE YOU SURE?" (in German the item first: 1792 .. then 459) (CLICK, type 3 on NO) into +0x3B4; else "INSUFFICIENT
// CASH" (type 1) and NO. BACK: CLICK, the window closed.
void ModShop::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    CareerProfile& p = career.profile;
    Car* car = garage.car(host);
    if (car == nullptr) return;
    const auto& lists = data->catalogue.lists;
    if (m_timer > 0.0f) {
        if (event == input::kBack) {
            host.sound("CLICK");
            host.request("", SwitchMode::Pop);
            return;
        }
        if (x87::chop(m_timer) < m_beeps) {
            --m_beeps;
            host.sound("WARNING");
        }
        if (s.find("ANALYSIS") >= 0) {
            float alpha = m_timer;
            while (alpha > 1.0f) alpha = alpha - 1.0f;
            s.setTextAlpha(s.find("ANALYSIS"), alpha);
            s.setHidden("ANALYSIS", false);
        }
        s.setHidden("menu", true);
        s.setHidden("backmenu", true);
        m_timer = m_timer - host.frameTime();
        s.hideContaining("carname");
        if (m_timer > 0.0f) return;
        m_timer = 0.0f;
        if (host.messageUp()) host.closeMessage();
        s.setHidden("menu", false);
        s.setHidden("backmenu", false);
        s.hideContaining("ANALYSIS");
        s.showContaining("carname", false);
        for (const char* name : {"BHP", "PRICE", "CLASS", "CLASSMETER", "Speed", "Accel", "Braking", "Handling"}) {
            s.setHidden(name, false);
        }
    }
    if (s.menuSize("menu") == 0) {
        const auto model = static_cast<size_t>(std::clamp(car->def != nullptr ? car->def->id : 0, 0, 52));
        const std::array<int8_t, 10>& unlocked = p.partsA[model];
        const bool maxed = std::all_of(unlocked.begin(), unlocked.end(), [](int8_t level) { return level == 3; });
        host.message(MessageType::Continue,
                     texts.at(kTextNoUpgrades) + L"\n" + texts.at(kTextNoUpgrades + (maxed ? 2 : 1)), false, nullptr);
        host.request("", SwitchMode::Pop);
        return;
    }
    const size_t item = s.selectedMenuItem("menu");
    const int32_t n = std::clamp(x87::chop(s.menuItemValue("menu", item)), 0, 9);
    ModShopEntry& e = m_entries[static_cast<size_t>(n)];
    if (m_answer == 1) {
        for (size_t kind = 0; kind < 10; ++kind) {
            if (e.parts[kind] != -1) car->mods[kind] = static_cast<uint8_t>(e.parts[kind]);
        }
        if (e.parts[kExhaust] != -1) garage.fitExhaust(host, *car, e.parts[kExhaust]);
        if (garage.view && garage.view->car() != nullptr) {
            garage.view->car()->sponsorDecals(*car, data->catalogue);
        }
        rateCar(*car, car->def != nullptr ? data->dynamics->of(*car->def) : nullptr, data->catalogue);
        refreshCar(*car);
        pay(career, x87::chop(e.cost));
        car->spent = std::bit_cast<uint32_t>(e.cost + std::bit_cast<float>(car->spent));
        const auto spent = static_cast<float>(static_cast<int32_t>(p.moneySpent));
        p.moneySpent = static_cast<uint32_t>(x87::chop(spent + e.cost));
        garage.cash(host);
        garage.carInfo(host);
        host.request("", SwitchMode::Pop);
    }
    if (static_cast<int>(item) != m_shown) {
        m_shown = static_cast<int>(item);
        if (s.find("BHP") >= 0) {
            s.setText("BHP", toWide(std::format("{}", x87::chop(e.rating))));
            s.setHidden("BHP", false);
        }
        if (s.find("CLASS") >= 0) {
            s.setText("CLASS", toWide(std::format("{}", e.classNumber)));
            s.setHidden("CLASS", false);
        }
        if (garage.partShown >= 0 && static_cast<size_t>(garage.partShown) < garage.filtered.size()) {
            const CarPart& part = *garage.filtered[static_cast<size_t>(garage.partShown)];
            s.setMeter("Speed", static_cast<float>(part.physics[1]) * kByteShare);
            s.setMeter("Accel", static_cast<float>(part.physics[0]) * kByteShare);
            s.setMeter("Braking", static_cast<float>(part.physics[2]) * kByteShare);
            s.setMeter("Handling", static_cast<float>(part.physics[3]) * kByteShare);
        }
        if (s.find("CLASSMETER") >= 0) {
            float v = e.rating;
            if (!(v <= 999.0f)) v = 999.0f;
            while (v > 100.0f) v = v - 100.0f;
            v = v * 0.01f;
            if (e.rating > 900.0f) v = 1.0f;
            s.setMeter("CLASSMETER", v);
            s.setHidden("CLASSMETER", false);
        }
        if (s.find("PRICE") >= 0) s.setText("PRICE", toWide(moneyText(x87::chop(e.cost), host.language().index)));
        s.setMeter("Braking", e.braking);
        s.setMeter("Handling", e.handling);
        s.setMeter("Accel", e.accel);
        s.setMeter("Speed", e.speed);
        for (size_t kind = 0; kind < 10; ++kind) {
            const char* name = modKindName(kind);
            if (name == nullptr) continue;
            const std::string icon = std::string("ICON_") + name;
            const std::string level = std::string("LEVEL_") + name;
            const std::string logo = std::string("LOGO_") + name;
            if (e.parts[kind] != -1) {
                if (s.find(icon) >= 0) {
                    s.setHidden(icon, false);
                    s.setColour(s.find(icon), 0xFFFFFFFF);
                }
                const CarPart& part = modPart(lists[kind], e.parts[kind]);
                std::string number = std::format("{}", part.level + 1);
                if (_stricmp(number.c_str(), "4") == 0) number = "P";
                if (s.find(level) >= 0) {
                    s.setText(level, toWide(number));
                    s.setHidden(level, false);
                }
                const int brand = static_cast<int8_t>(part.brand);
                const std::vector<ExeTables::Brand>& brands = data->tables.brands;
                const std::string supplier = brand >= 0 && static_cast<size_t>(brand) < brands.size()
                                                 ? brands[static_cast<size_t>(brand)].name
                                                 : std::string();
                if (s.find(logo) >= 0) {
                    s.setTexture(logo, "ui_" + supplier + "_on");
                    s.restartTransition(logo, false);
                }
            } else {
                if (s.find(icon) >= 0) {
                    s.setHidden(icon, false);
                    s.setColour(s.find(icon), 0x80808080);
                }
                if (s.find(level) >= 0) s.setText(level, L"-");
                if (s.find(logo) >= 0) s.restartTransition(logo, true);
            }
        }
    }
    if (event == input::kSelect) {
        if (static_cast<float>(p.money) > e.cost) {
            const std::wstring itemText = s.menuItemText("menu", item);
            std::wstring text;
            if (host.language().index == 2) {
                text = itemText + texts.at(kTextBuy);
            } else {
                text = texts.at(kTextBuy) + L" " + itemText;
            }
            text += L"\n " + toWide(moneyText(x87::chop(e.cost), host.language().index)) + L"\n " +
                    texts.at(kTextAreYouSure);
            host.sound("CLICK");
            host.message(MessageType::YesNo, text, false, &m_answer, true);
        } else {
            host.message(MessageType::Continue, texts.at(kTextInsufficientCash), false, nullptr);
            host.sound("NO");
        }
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request("", SwitchMode::Pop);
    }
}

}  // namespace oj::frontend
