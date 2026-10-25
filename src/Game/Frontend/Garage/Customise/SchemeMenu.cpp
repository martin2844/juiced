// schememenu: the car's look kept as the settings' scheme or put on the car.

#include "Game/Frontend/Garage/Customise/SchemeMenu.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>
#include <cstring>

namespace oj::frontend {

namespace {

// A car's look as schememenu keeps it: the base, metallic and pearl colours and amounts, the dial colours, the tint,
// the sponsor decals, the decal colour (+0x4C), the decal, its colour, the wheel colour and its flag, the wheel, the
// neon and its colour.
std::array<uint32_t, 17> carLook(const Car& c) {
    return {c.base,
            c.metallic,
            c.pearl,
            c.metallicAmount,
            c.pearlAmount,
            c.dialColour,
            c.dialTextColour,
            c.tint,
            c.sponsorDecals,
            c.sponsorColour,
            static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(c.body[8]))),
            c.decalColour,
            c.wheelColour,
            c.wheelsColoured,
            static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(c.mods[kWheel]))),
            static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(c.body[9]))),
            c.neon};
}

}  // namespace

// A look on the car and its model: 0x0043C750 (the kept look back) and schememenu's apply (the settings' scheme; its
// decal 0 keeps the car's but for a prototype, 0x004A8F50, which takes 0). The decal (0x0048BB00), the wheels
// (0x0048B860), the decal colour, the model's parts, the wheel colour (a look without one puts the wheels' kept
// colours back, 0x0043C92C, 0x0043CE0D: 0x0048C530), the paint, the tint, the neon's colour and the neon shown or
// not.
void SchemeMenu::onCar(GarageHost& host, Car& car, const std::array<uint32_t, 17>& look, bool applying) {
    GameData* data = host.gameData();
    Garage& garage = host.garage();
    car.base = look[0];
    car.metallic = look[1];
    car.pearl = look[2];
    car.metallicAmount = look[3];
    car.pearlAmount = look[4];
    car.dialColour = look[5];
    car.dialTextColour = look[6];
    car.tint = look[7];
    car.sponsorDecals = look[8];
    car.sponsorColour = look[9];
    if (!applying || look[10] != 0) {
        car.body[8] = static_cast<uint8_t>(look[10]);
    } else if (heavilyTuned(car, data->catalogue)) {
        car.body[8] = 0;
    }
    car.decalColour = look[11];
    car.wheelColour = look[12];
    car.wheelsColoured = look[13];
    garage.fitDecal(car, static_cast<int8_t>(car.body[8]));
    garage.fitWheels(host, car, static_cast<int32_t>(look[14]));
    scene::CarModel* model = garage.view ? garage.view->car() : nullptr;
    if (model != nullptr) {
        model->colourDecal(car);
        model->sponsorDecals(car, data->catalogue);
        if (look[13] != 0) {
            model->colourWheels(car);
        } else {
            model->restoreWheelColours();
        }
    }
    car.neon = look[16];
    car.body[9] = static_cast<uint8_t>(look[15]);
    if (garage.view) {
        garage.view->colourNeon(car.neon);
        if (static_cast<int32_t>(look[15]) > -1) {
            garage.view->showNeon(car, host.viewLoading());
        } else {
            garage.view->hideNeon();
        }
    }
    garage.repaint(host);
    garage.retint(host);
}

// 0x0043C600: menu (its help shown); saving not asked, the answer -1; the car's look kept; not applied.
void SchemeMenu::enter(GarageHost& host, ui::Screen& /*s*/) {
    Garage& garage = host.garage();
    m_saving = 0;
    m_answer = -1;
    if (const Car* car = garage.car(host); car != nullptr) m_kept = carLook(*car);
    m_applied = 0;
}

// 0x0043CAB0: [0x0075F108] 2. A "yes": saving puts the kept look in the settings' scheme; applying pays its price and
// shows the money; CLICK and the window closed. The highlighted item apply (1) puts the scheme on the car once (+0x60),
// set (0) after it puts the kept look back (0x0043C750). SELECT: the scheme's tint and amounts within 0.05 of the
// kept ones become them; set: the scheme as the look "THIS SCHEME IS ALREADY SAVED" (1919, type 1), else "SAVE THIS
// SCHEME. YOUR EXISTING CREW SCHEME WILL BE OVERWRITTEN!" (1916), "\n" and "ARE YOU SURE?" (type 3, on NO); apply: the
// look as the scheme (0x0043C9F0: alike but the decal when the look has the first) "THIS SCHEME IS ALREADY APPLIED"
// (1920), else the price: 1000 for another base, 3000 for another metallic not black in any of red, green and blue,
// 5000 for another pearl not the base, 500 for another wheel colour, 299 for another tint, the wheel's COST for another
// wheel, 2000 for another decal; with the money over it "APPLY YOUR CREW'S SCHEME TO THIS CAR." (1917), "\n", "\n
// <price> \n", "\n" and "ARE YOU SURE?" (type 3, on NO); else "\n <price> \n", "\n" and "INSUFFICIENT CASH" (type 1)
// and NO. BACK: the kept look back, CLICK, the window closed.
void SchemeMenu::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr || data == nullptr || !data->career) return;
    save::Settings& set = host.optionSettings();
    const auto scheme = [&](size_t i) { return set.u32(save::Settings::kScheme + static_cast<uint32_t>(i) * 4); };
    if (m_answer == 1) {
        if (m_saving != 0) {
            for (size_t i = 0; i < m_kept.size(); ++i) {
                set.setU32(save::Settings::kScheme + static_cast<uint32_t>(i) * 4, m_kept[i]);
            }
        } else {
            pay(*data->career, m_price);
            garage.cash(host);
        }
        host.sound("CLICK");
        host.request("", SwitchMode::Pop);
    }
    std::array<uint32_t, 17> saved{};
    for (size_t i = 0; i < saved.size(); ++i) saved[i] = scheme(i);
    const size_t item = s.find("menu") >= 0 && s.menuSize("menu") != 0 ? s.selectedMenuItem("menu") : 0;
    if (item == 1 && m_applied == 0) {
        m_applied = 1;
        onCar(host, *car, saved, true);
    } else if (item == 0 && m_applied != 0) {
        onCar(host, *car, m_kept, false);
        m_applied = 0;
    }
    if (event == input::kSelect) {
        const auto near = [&](size_t i) {
            if (std::fabs(Car::asFloat(scheme(i)) - Car::asFloat(m_kept[i])) < 0.05f) {
                set.setU32(save::Settings::kScheme + static_cast<uint32_t>(i) * 4, m_kept[i]);
            }
        };
        near(7);
        near(3);
        near(4);
        for (size_t i = 0; i < saved.size(); ++i) saved[i] = scheme(i);
        if (item == 0) {
            if (saved == m_kept) {
                host.message(MessageType::Continue, texts.at(kTextSchemeSaved), false, nullptr);
            } else {
                m_saving = 1;
                host.message(MessageType::YesNo, texts.at(kTextSchemeSave) + L"\n" + texts.at(kTextAreYouSure), false,
                             &m_answer, true);
            }
        } else {
            const std::array<uint32_t, 17>& k = m_kept;
            const auto same = [&](size_t i) { return Car::asFloat(k[i]) == Car::asFloat(saved[i]); };
            const bool alike = k[0] == saved[0] && k[1] == saved[1] && k[2] == saved[2] && same(3) && same(4) &&
                               k[5] == saved[5] && k[6] == saved[6] && same(7) && k[8] == saved[8] &&
                               k[9] == saved[9] && k[11] == saved[11] && k[12] == saved[12] && k[13] == saved[13] &&
                               k[14] == saved[14] && k[15] == saved[15] && k[16] == saved[16] &&
                               (k[10] == 0 || k[10] == saved[10]);
            if (alike) {
                host.message(MessageType::Continue, texts.at(kTextSchemeSaved + 1), false, nullptr);
            } else {
                m_saving = 0;
                m_price = 0;
                if (k[0] != saved[0]) m_price += 1000;
                if (k[1] != saved[1] && (saved[1] & 0xFF0000u) != 0 && (saved[1] & 0xFF00u) != 0 &&
                    (saved[1] & 0xFFu) != 0) {
                    m_price += 3000;
                }
                if (k[2] != saved[2] && saved[2] != saved[0]) m_price += 5000;
                if (k[12] != saved[12]) m_price += 500;
                if (!same(7)) m_price += 299;
                const auto wheel = static_cast<int32_t>(saved[14]);
                if (static_cast<int32_t>(k[14]) != wheel && wheel > -1) {
                    const std::vector<CarPart>& wheels = data->catalogue.lists[kWheel].parts;
                    if (!wheels.empty()) {
                        const size_t w = static_cast<size_t>(wheel) < wheels.size() ? static_cast<size_t>(wheel) : 0;
                        m_price += wheels[w].cost;
                    }
                }
                if (k[10] != saved[10]) m_price += 2000;
                const std::wstring price = L"\n " + toWide(moneyText(m_price, host.language().index)) + L" \n";
                if (data->career->profile.money > m_price) {
                    host.message(MessageType::YesNo,
                                 texts.at(kTextSchemeSave + 1) + L"\n" + price + L"\n" + texts.at(kTextAreYouSure),
                                 false, &m_answer, true);
                } else {
                    host.sound("NO");
                    host.message(MessageType::Continue, price + L"\n" + texts.at(kTextInsufficientCash), false,
                                 nullptr);
                }
            }
        }
    } else if (event == input::kBack) {
        onCar(host, *car, m_kept, false);
        m_applied = 0;
        host.sound("CLICK");
        host.request("", SwitchMode::Pop);
    }
}

}  // namespace oj::frontend
