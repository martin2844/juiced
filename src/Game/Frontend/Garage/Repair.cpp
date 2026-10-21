// The repair window (no window asks for it; opened by "unregistered:repair").

#include "Game/Frontend/Garage/Repair.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/Garage/Garage.h"
#include "Game/Frontend/WindowTools.h"

namespace oj::frontend {

namespace {

constexpr uint32_t kNitroFull = 0x41700000;   // 15.0

// The COST of the car's part of a kind, uncapped: none (-1) 0, past the list's end the list's first.
// Stock problem: an index below -1 reads before the list; the first part here.
float uncappedPartCost(const Car& car, const PartCatalogue& catalogue, size_t kind) {
    const auto index = static_cast<int8_t>(car.mods[kind]);
    if (index == -1) return 0.0f;
    const std::vector<CarPart>& parts = catalogue.lists[kind].parts;
    if (parts.empty()) return 0.0f;
    const int32_t cost = index >= 0 && index < static_cast<int>(parts.size()) ? parts[static_cast<size_t>(index)].cost
                                                                               : parts.front().cost;
    return static_cast<float>(cost);
}

// 0x00445660's and 0x00445730's condition picture of a wear: over 0.6 bad, over 0.3 middling, else good.
const char* wearPicture(float wear) {
    if (wear > 0.6f) return "ui_condition_bad";
    if (wear > 0.3f) return "ui_condition_mid";
    return "ui_condition_good";
}

// 0x0048F270: a spending on the car: its +0xA8 and the profile's +0x4E8 grow by it.
void noteCarSpending(Career& career, Car& car, float cost) {
    car.spent = std::bit_cast<uint32_t>(std::bit_cast<float>(car.spent) + cost);
    const auto spent = static_cast<float>(static_cast<int32_t>(career.profile.moneySpent));
    career.profile.moneySpent = static_cast<uint32_t>(x87::chop(spent + cost));
}

}  // namespace

// 0x00446030: the brakes' repair: 0 without their wear (+0x7C), else the brake part's COST x 0.3, 100 for 0.
float Repair::brakesCost(GarageHost& host) {
    GameData* data = host.gameData();
    Garage& garage = host.garage();
    const Car* car = garage.car(host);
    if (car == nullptr || std::bit_cast<float>(car->brakeWear) == 0.0f) return 0.0f;
    const float v = uncappedPartCost(*car, data->catalogue, kBrake) * 0.3f;
    return v == 0.0f ? 100.0f : v;
}

// 0x00445FA0: the tyres': 0 without their wear (+0x74), else the tyre part's COST x 0.3, 200 for 0.
float Repair::tyresCost(GarageHost& host) {
    GameData* data = host.gameData();
    Garage& garage = host.garage();
    const Car* car = garage.car(host);
    if (car == nullptr || std::bit_cast<float>(car->tyreWear) == 0.0f) return 0.0f;
    const float v = uncappedPartCost(*car, data->catalogue, kTyre) * 0.3f;
    return v == 0.0f ? 200.0f : v;
}

// 0x00445F10: the nitro's: 0 with the tank full (+0x84 15) or without a nitro part, else its COST x 0.1.
float Repair::nitroCost(GarageHost& host) {
    GameData* data = host.gameData();
    Garage& garage = host.garage();
    const Car* car = garage.car(host);
    if (car == nullptr || car->nitro == kNitroFull || car->mods[kNitro] == kNoMod) return 0.0f;
    return uncappedPartCost(*car, data->catalogue, kNitro) * 0.1f;
}

// 0x00445730: Condition1 the brakes' wear's picture and CostText1 their cost (cut, as money) or "NA" without wear;
// Condition2 and CostText2 the tyres'; Condition3 hidden without a nitro part, else the nitro left / 15's picture
// (over 0.9 good, over 0.75 middling, else bad), CostText3 "NA" with the tank full or no part, else its cost; then the
// items (0x00445660).
void Repair::showTexts(GarageHost& host, ui::Screen& s) {
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr) return;
    const int language = host.language().index;
    const auto cost = [&](const char* element, bool none, float value) {
        s.setText(element, none ? texts.at(kTextNotAvailable) : toWide(moneyText(x87::chop(value), language)));
    };
    const float brakes = std::bit_cast<float>(car->brakeWear);
    s.setTexture("Condition1", wearPicture(brakes));
    cost("CostText1", brakes == 0.0f, brakes == 0.0f ? 0.0f : brakesCost(host));
    const float tyres = std::bit_cast<float>(car->tyreWear);
    s.setTexture("Condition2", wearPicture(tyres));
    cost("CostText2", tyres == 0.0f, tyres == 0.0f ? 0.0f : tyresCost(host));
    const float left = std::bit_cast<float>(car->nitro) * 0.06666667f;
    const bool noPart = car->mods[kNitro] == kNoMod;
    if (noPart) {
        s.setHidden("Condition3", true);
    } else {
        s.setHidden("Condition3", false);
        s.setTexture("Condition3", left > 0.9f ? "ui_condition_good" : left > 0.75f ? "ui_condition_mid"
                                                                                     : "ui_condition_bad");
    }
    const bool full = left == 1.0f || noPart;
    cost("CostText3", full, full ? 0.0f : nitroCost(host));
    fillItems(host, s);
}

// 0x00445660: newmenu's items: brakes with their wear, tyres with theirs, nos with the tank not full and a nitro part,
// all with any of them.
void Repair::fillItems(GarageHost& host, ui::Screen& s) {
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr) return;
    bool none = true;
    const bool brakes = std::bit_cast<float>(car->brakeWear) != 0.0f;
    const bool tyres = std::bit_cast<float>(car->tyreWear) != 0.0f;
    const bool nitro = car->nitro != kNitroFull && car->mods[kNitro] != kNoMod;
    s.setMenuItemEnabled("newmenu", "brakes", brakes);
    s.setMenuItemEnabled("newmenu", "tyres", tyres);
    s.setMenuItemEnabled("newmenu", "nos", nitro);
    if (brakes || tyres || nitro) none = false;
    s.setMenuItemEnabled("newmenu", "all", !none);
}

// 0x004463D0: without wear "No maintenance required"; with less money than the cost "INSUFFICIENT CASH" and NO; else
// "Maintenance complete", the cost paid (cut), noted on the car (0x0048F270) and the brakes' wear 0 (0x0048F1C0).
void Repair::repairBrakes(GarageHost& host) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr) return;
    Career& career = *data->career;
    if (std::bit_cast<float>(car->brakeWear) == 0.0f) {
        host.message(MessageType::Continue, texts.at(kTextNoMaintenance), false, nullptr);
        return;
    }
    if (static_cast<float>(career.profile.money) < brakesCost(host)) {
        host.message(MessageType::Continue, texts.at(kTextInsufficientCash), false, nullptr);
        host.sound("NO");
        return;
    }
    host.message(MessageType::Continue, texts.at(kTextMaintenanceDone), false, nullptr);
    pay(career, x87::chop(brakesCost(host)));
    noteCarSpending(career, *car, brakesCost(host));
    car->brakeWear = 0;
}

// 0x00446270: the same for the tyres (+0x74; 0x0048F170).
void Repair::repairTyres(GarageHost& host) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr) return;
    Career& career = *data->career;
    if (std::bit_cast<float>(car->tyreWear) == 0.0f) {
        host.message(MessageType::Continue, texts.at(kTextNoMaintenance), false, nullptr);
        return;
    }
    if (static_cast<float>(career.profile.money) < tyresCost(host)) {
        host.message(MessageType::Continue, texts.at(kTextInsufficientCash), false, nullptr);
        host.sound("NO");
        return;
    }
    host.message(MessageType::Continue, texts.at(kTextMaintenanceDone), false, nullptr);
    pay(career, x87::chop(tyresCost(host)));
    noteCarSpending(career, *car, tyresCost(host));
    car->tyreWear = 0;
}

// 0x004460C0: the nitro (the tank not full and a part): the same, then the tank 15 (0x0048F210) and the leak (+0x70)
// cleared; else "No maintenance required".
void Repair::repairNitro(GarageHost& host) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr) return;
    Career& career = *data->career;
    if (car->nitro == kNitroFull || car->mods[kNitro] == kNoMod) {
        host.message(MessageType::Continue, texts.at(kTextNoMaintenance), false, nullptr);
        return;
    }
    if (static_cast<float>(career.profile.money) < nitroCost(host)) {
        host.message(MessageType::Continue, texts.at(kTextInsufficientCash), false, nullptr);
        host.sound("NO");
        return;
    }
    host.message(MessageType::Continue, texts.at(kTextMaintenanceDone), false, nullptr);
    pay(career, x87::chop(nitroCost(host)));
    noteCarSpending(career, *car, nitroCost(host));
    car->nitro = kNitroFull;
    car->nitroLeaks = 0;
}

// 0x00446530: the three costs (each cut) summed; 0 "No maintenance required"; more than the money "INSUFFICIENT CASH"
// and NO; else "Maintenance complete", the sum paid and noted, the tank 15, the brakes' and the tyres' wear 0.
void Repair::repairAll(GarageHost& host) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr) return;
    Career& career = *data->career;
    int32_t total = 0;
    if (std::bit_cast<float>(car->brakeWear) != 0.0f) total = x87::chop(brakesCost(host));
    if (std::bit_cast<float>(car->tyreWear) != 0.0f) total = total + x87::chop(tyresCost(host));
    if (car->nitro != kNitroFull) total = total + x87::chop(nitroCost(host));
    if (total == 0) {
        host.message(MessageType::Continue, texts.at(kTextNoMaintenance), false, nullptr);
        return;
    }
    if (career.profile.money < total) {
        host.message(MessageType::Continue, texts.at(kTextInsufficientCash), false, nullptr);
        host.sound("NO");
        return;
    }
    host.message(MessageType::Continue, texts.at(kTextMaintenanceDone), false, nullptr);
    pay(career, total);
    noteCarSpending(career, *car, static_cast<float>(total));
    car->nitro = kNitroFull;
    car->brakeWear = 0;
    car->tyreWear = 0;
}

// 0x00445630: the texts and the items (0x00445730), +0x0C -1, +0x10 0.
void Repair::enter(GarageHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    *this = Repair{};
    if (data == nullptr || !data->career) return;
    showTexts(host, s);
}

// 0x00445A70 ([0x0075F108] 2): an asked repair (+0x10) answered yes (+0x0C 1) is made by newmenu's highlighted item
// (brakes, tyres, nos, all) and the texts made again, CLICK, the window closes (mode 6); another answer clears the
// question. SELECT on an item that is on: +0x10 1 and its cost (all: the three cut and summed); 0 "No maintenance
// required"; more than the money "INSUFFICIENT CASH" and NO; else the item's line (1500 .. 1502, all 1499), " ", the
// cost as money, a line feed and "ARE YOU SURE?" (CLICK, type 3, into +0x0C). BACK (CLICK) closes the window.
void Repair::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    const bool hasItem = s.menuSize("newmenu") != 0;
    const size_t at = hasItem ? s.selectedMenuItem("newmenu") : 0;
    const std::string item = hasItem ? s.menuItemCommand("newmenu", at) : "";
    const auto is = [&](const char* name) { return _stricmp(item.c_str(), name) == 0; };
    if (m_asked != 0) {
        if (m_answer == 1) {
            if (is("brakes")) {
                repairBrakes(host);
                showTexts(host, s);
            }
            if (is("tyres")) {
                repairTyres(host);
                showTexts(host, s);
            }
            if (is("nos")) {
                repairNitro(host);
                showTexts(host, s);
            }
            if (is("all")) {
                repairAll(host);
                showTexts(host, s);
            }
            host.sound("CLICK");
            host.request("", SwitchMode::Pop);
            return;
        }
        m_asked = 0;
        m_answer = -1;
    }
    if (event == input::kBack) {
        host.sound("CLICK");
        host.request("", SwitchMode::Pop);
        return;
    }
    if (event != input::kSelect || !hasItem || !s.menuItemEnabled("newmenu", at)) return;
    m_asked = 1;
    float cost = 0.0f;
    size_t line = 0;
    if (is("brakes")) {
        line = kTextRepairBrakes;
        cost = brakesCost(host);
    } else if (is("tyres")) {
        line = kTextRepairTyres;
        cost = tyresCost(host);
    } else if (is("nos")) {
        line = kTextRepairNitro;
        cost = nitroCost(host);
    } else if (is("all")) {
        line = kTextRepairAll;
        const float b = static_cast<float>(x87::chop(brakesCost(host)));
        const float t = static_cast<float>(x87::chop(tyresCost(host))) + b;
        cost = static_cast<float>(x87::chop(nitroCost(host))) + t;
    } else {
        host.message(MessageType::Continue, texts.at(kTextNoMaintenance), false, nullptr);
        return;
    }
    if (cost == 0.0f) {
        host.message(MessageType::Continue, texts.at(kTextNoMaintenance), false, nullptr);
        return;
    }
    if (static_cast<float>(data->career->profile.money) < cost) {
        host.message(MessageType::Continue, texts.at(kTextInsufficientCash), false, nullptr);
        host.sound("NO");
        return;
    }
    std::wstring text = texts.at(line) + L" " + toWide(moneyText(x87::chop(cost), host.language().index)) + L"\n" +
                        texts.at(kTextAreYouSure);
    host.sound("CLICK");
    host.message(MessageType::YesNo, text, false, &m_answer, true);
}

}  // namespace oj::frontend
