// The windows of one part: the exhausts, the suspension's ride, the engine chip and the gearbox.

#include "Game/Frontend/Garage/Customise/PartMenus.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>

namespace oj::frontend {

namespace {

constexpr oj::Vec4 kCameraSide{-500.0f, 100.0f, 0.0f, 0.0f};           // 0x00720408
constexpr oj::Vec4 kCameraEngine{130.0f, 100.0f, 350.0f, 0.0f};        // 0x00720488
constexpr oj::Vec4 kCameraWheel{-200.0f, 30.0f, 230.0f, 0.0f};         // 0x00720498
constexpr oj::Vec4 kCameraExhaust{40.0f, 50.0f, -460.0f, 0.0f};        // 0x007204B8

}  // namespace

// 0x00440EF0: the garage's rig held (pitch and yaw 0, no turn, 0.25 s before input); +0x18 the car's exhaust; Menu
// emptied and an item for each exhaust of the supplier at the catalogue's level (0x004990F0): no text, the part's
// place, its UIOFFTEX and UIONTEX, its SHAPE; +0x14 Menu's highlighted item (the first past the list) and that part on
// the model (0x0048BE30); SuppLogo UI_<supplier>_ON; the money shown; the base enter; the camera to the exhaust
// (0x007204B8); the price and its box come in; the price 0 and its text emptied, the bought flag, the rating and the
// refund 0.
void ExhaustMenu::enter(GarageHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr || data == nullptr) return;
    if (garage.view) garage.view->holdRig();
    m_current = static_cast<int8_t>(car->mods[kExhaust]);
    if (s.find("Menu") >= 0) {
        s.clearMenu("Menu");
        garage.filtered = modBrandParts(data->catalogue.lists[kExhaust], garage.brand, garage.catalogueLevel);
        for (const CarPart* part : garage.filtered) {
            s.addMenuItem("Menu", L"", static_cast<float>(part->index), part->uiOff, part->uiOn);
            s.setMenuItemCommand("Menu", s.menuSize("Menu") - 1, part->model);
        }
        m_place = static_cast<int32_t>(s.selectedMenuItem("Menu"));
        if (s.menuSize("Menu") != 0) {
            if (static_cast<size_t>(m_place) >= s.menuSize("Menu")) {
                m_place = 0;
                s.selectMenuItem("Menu", 0);
            }
            garage.fitExhaust(host, *car, x87::chop(s.menuItemValue("Menu", static_cast<size_t>(m_place))));
        }
    }
    if (s.find("SuppLogo") >= 0) {
        const std::vector<ExeTables::Brand>& brands = data->tables.brands;
        const std::string name = garage.brand >= 0 && static_cast<size_t>(garage.brand) < brands.size()
                                     ? brands[static_cast<size_t>(garage.brand)].name
                                     : "";
        s.setTexture("SuppLogo", "UI_" + name + "_ON");
    }
    garage.cash(host);
    host.baseEnter(s, "exhaustmenu");
    if (garage.view) garage.view->cameraTo(kCameraExhaust);
    garage.priceShown(host, true);
    garage.clear(host);
}

// 0x004411C0: [0x0075F108] 2. With Menu's highlighted item: another than +0x14 puts its part on the model (0x0048BE30)
// and becomes +0x14, kept at [0x0075ED08]; SELECT: the car's own exhaust (+0x18) notes "You already have this mod"
// (type 1), else the rating with the part (the kind's part the item's, rated, the exhaust back to +0x18 and rated
// again), CLICK and buymod over the window. A part bought: the car's exhaust the item's and tuningmenu. Else Name the
// part's TEXT and the price its COST (the bought flag 0). Without a window over the windows `performance` over them.
// BACK (CLICK): suppliers, the car's exhaust (+0x18) back on the model, the price, rating, bought flag and refund
// cleared. The chassis lowered.
void ExhaustMenu::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr || data == nullptr) return;
    if (s.find("Menu") >= 0 && s.menuSize("Menu") != 0) {
        const size_t item = s.selectedMenuItem("Menu");
        const float value = s.menuItemValue("Menu", item);
        if (static_cast<int32_t>(item) != m_place) {
            garage.fitExhaust(host, *car, x87::chop(value));
            m_place = static_cast<int32_t>(item);
        }
        garage.partShown = m_place;
        if (event == input::kSelect) {
            if (x87::chop(value) == m_current) {
                host.message(MessageType::Continue, texts.at(kTextHaveMod), false, nullptr);
            } else {
                const auto kind = static_cast<size_t>(std::clamp(garage.kind, 0, 9));
                const CarDynamics* dynamics = car->def != nullptr ? data->dynamics->of(*car->def) : nullptr;
                car->mods[kind] = static_cast<uint8_t>(x87::nearest(value));
                const float rating = rateCar(*car, dynamics, data->catalogue);
                car->mods[kExhaust] = static_cast<uint8_t>(m_current);
                rateCar(*car, dynamics, data->catalogue);
                garage.texts.newRating = rating;
                host.sound("CLICK");
                host.request("buymod", SwitchMode::Push);
            }
        }
        if (garage.texts.bought != 0) {
            car->mods[kExhaust] = static_cast<uint8_t>(x87::nearest(value));
            host.request("tuningmenu", SwitchMode::Swap);
            return;
        }
        if (m_place >= 0 && static_cast<size_t>(m_place) < garage.filtered.size()) {
            const CarPart& part = *garage.filtered[static_cast<size_t>(m_place)];
            if (s.find("Name") >= 0) s.setText("Name", toWide(part.label));
            garage.price(host, part.cost);
        }
    }
    if (!host.overlayUp()) host.request("performance", SwitchMode::Overlay);
    if (event == input::kBack) {
        host.sound("CLICK");
        host.request("suppliers", SwitchMode::Swap);
        garage.fitExhaust(host, *car, m_current);
        garage.clear(host);
    }
    garage.lowerChassis(host);
}

// 0x00440560: the base enter; the price and its box go out; the price 0 (the bought flag 0); HEIGHTSLIDER at the
// car's ride (+0x58), kept at +0x10.
void SuspensionMenu::enter(GarageHost& host, ui::Screen& s) {
    Garage& garage = host.garage();
    host.baseEnter(s, "suspensionmenu");
    garage.priceShown(host, false);
    garage.price(host, 0);
    if (Car* car = garage.car(host); car != nullptr) {
        m_original = Car::asFloat(car->lowering);
        s.setSliderValue("heightslider", m_original);
    }
}

// 0x00440620: [0x0075F108] 2. A ride bought (the price 0): CLICK and tuningmenu. The chassis lowered, then the car's
// ride the slider's. SELECT (CLICK): buymod over the window. BACK (CLICK): tuningmenu and the ride back.
void SuspensionMenu::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (garage.texts.bought != 0) {
        host.sound("CLICK");
        host.request("tuningmenu", SwitchMode::Swap);
    }
    if (car != nullptr) {
        garage.lowerChassis(host);
        car->lowering = Car::bitsOf(s.sliderValue("heightslider"));
    }
    if (event == input::kSelect) {
        host.sound("CLICK");
        host.request("buymod", SwitchMode::Push);
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request("tuningmenu", SwitchMode::Swap);
        if (car != nullptr) car->lowering = Car::bitsOf(m_original);
    }
}

// 0x004407B0: the base enter; the price and its box come in; the price 250; CHIPSLIDER at (the chip (+0x60) - 1) x 10
// + 0.5, the chip kept at +0x14; the rating with the chip at 1 (+0x1C) and the car's own (cut, +0x24).
void EngineChip::enter(GarageHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    Garage& garage = host.garage();
    host.baseEnter(s, "enginechip");
    garage.priceShown(host, true);
    garage.price(host, 250);
    Car* car = garage.car(host);
    if (car == nullptr || data == nullptr) return;
    const CarDynamics* dynamics = car->def != nullptr ? data->dynamics->of(*car->def) : nullptr;
    m_original = Car::asFloat(car->torqueFactor);
    s.setSliderValue("chipslider", (m_original - 1.0f) * 10.0f + 0.5f);
    car->torqueFactor = Car::bitsOf(1.0f);
    m_rating = rateCar(*car, dynamics, data->catalogue);
    car->torqueFactor = Car::bitsOf(m_original);
    m_ownBhp = x87::chop(rateCar(*car, dynamics, data->catalogue));
}

// 0x004408E0: [0x0075F108] 2. A chip bought: CLICK, tuningmenu, the car's chip 1 + +0x18, the price and its box go
// out. With NEWBHP: +0x18 (the slider - 0.5) x 0.1, NEWBHP the rating (1 + it) x +0x1C to the nearest. SELECT
// (CLICK): the car's own rating tuningmenu, else the garage's rating that one and buymod over the window. BACK (CLICK):
// tuningmenu, the chip back, the price and its box go out.
void EngineChip::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr) return;
    if (garage.texts.bought != 0) {
        host.sound("CLICK");
        host.request("tuningmenu", SwitchMode::Swap);
        car->torqueFactor = Car::bitsOf(m_chosen + 1.0f);
        garage.priceShown(host, false);
    }
    if (s.find("NEWBHP") >= 0) {
        m_chosen = (s.sliderValue("chipslider") - 0.5f) * 0.1f;
        m_bhp = x87::nearest((m_chosen + 1.0f) * m_rating);
        s.setText("NEWBHP", toWide(std::format("{}", m_bhp)));
    }
    if (event == input::kSelect) {
        host.sound("CLICK");
        if (m_bhp == m_ownBhp) {
            host.request("tuningmenu", SwitchMode::Swap);
        } else {
            garage.texts.newRating = static_cast<float>(m_bhp);
            host.request("buymod", SwitchMode::Push);
        }
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request("tuningmenu", SwitchMode::Swap);
        car->torqueFactor = Car::bitsOf(m_original);
        garage.priceShown(host, false);
    }
}

// 0x004425B0: the base enter; `performance` over the windows (mode 8); GEARSLIDER at the car's gearing (+0x5C), kept
// at +0x10; the price and its box come in; the price 500; the question (+0x18) none, the answer (+0x14) -1.
void GearboxMenu::enter(GarageHost& host, ui::Screen& s) {
    Garage& garage = host.garage();
    host.baseEnter(s, "gearboxmenu");
    host.request("performance", SwitchMode::Overlay);
    if (Car* car = garage.car(host); car != nullptr) {
        m_original = Car::asFloat(car->gearing);
        s.setSliderValue("gearslider", m_original);
    }
    garage.priceShown(host, true);
    garage.price(host, 500);
    m_asked = 0;
    m_answer = -1;
}

// 0x004426B0: [0x0075F108] 2; the slider takes the direction held; the chassis lowered, then the car's gearing the
// slider's. After the question a "yes" pays the price, notes it on the car and the profile (0x0048F270), CLICK,
// tuningmenu, the car's gearbox (+0x17) 1, the price, rating, bought flag and refund cleared and the money shown;
// anything else clears the question and the answer. SELECT: the gearing as it was CLICK, tuningmenu and the garage's
// texts cleared; else the money under the price "INSUFFICIENT CASH" (type 1) and NO, or "NEW MOD PRICE" (1829), the
// price, "\n" and "ARE YOU SURE?" (CLICK, type 3, on NO) and the question. BACK: the gearing back, CLICK, tuningmenu
// and the garage's texts cleared.
void GearboxMenu::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    Car* car = garage.car(host);
    if (car == nullptr || data == nullptr || !data->career) return;
    Career& career = *data->career;
    s.setSliderActive("gearslider", true);
    garage.lowerChassis(host);
    car->gearing = Car::bitsOf(s.sliderValue("gearslider"));
    if (m_asked != 0) {
        if (m_answer == 1) {
            pay(career, garage.texts.price);
            const auto cost = static_cast<float>(garage.texts.price);
            car->spent = std::bit_cast<uint32_t>(std::bit_cast<float>(car->spent) + cost);
            CareerProfile& p = career.profile;
            const auto spent = static_cast<float>(static_cast<int32_t>(p.moneySpent));
            p.moneySpent = static_cast<uint32_t>(x87::chop(spent + cost));
            host.sound("CLICK");
            host.request("tuningmenu", SwitchMode::Swap);
            car->mods[kGearbox] = 1;
            garage.clear(host);
            garage.cash(host);
        } else {
            m_asked = 0;
            m_answer = -1;
        }
    }
    if (event == input::kSelect) {
        if (Car::asFloat(car->gearing) == m_original) {
            host.sound("CLICK");
            host.request("tuningmenu", SwitchMode::Swap);
            garage.clear(host);
        } else if (career.profile.money < garage.texts.price) {
            host.message(MessageType::Continue, texts.at(kTextInsufficientCash), false, nullptr);
            host.sound("NO");
        } else {
            const std::wstring text = texts.at(kTextRefund + 1) + L" " +
                                      toWide(moneyText(garage.texts.price, host.language().index)) + L"\n" +
                                      texts.at(kTextAreYouSure);
            host.sound("CLICK");
            host.message(MessageType::YesNo, text, false, &m_answer, true);
            m_asked = 1;
        }
    } else if (event == input::kBack) {
        car->gearing = Car::bitsOf(m_original);
        host.sound("CLICK");
        host.request("tuningmenu", SwitchMode::Swap);
        garage.clear(host);
    }
}

}  // namespace oj::frontend
