// What the garage's windows share: the car shown, its scene, the price, the money and the class shown,
// the parts put on the car's model.

#include "Game/Frontend/Garage/Garage.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>
#include <cstring>

namespace oj::frontend {

// The car of the dealer's list shown (+0x27D8 at +0x27DC): the showroom in mode 1, else the garage.
Car* Garage::car(WindowHost& host) {
    GameData* data = host.gameData();
    Career& career = *data->career;
    const auto at = static_cast<size_t>(career.dealer.chosen);
    std::vector<Car>& cars = career.dealer.mode == DealerMode::Offers ? career.dealerShowroom : career.garage;
    return at < cars.size() ? &cars[at] : nullptr;
}

// 0x00447090: the price kept and shown (thousands marked); the bought flag (+0x2C) 0.
void Garage::price(WindowHost& host, int32_t price) {
    texts.price = price;
    host.baseScreen().setText(texts.priceText, toWide(moneyText(price, host.language().index)));
    texts.bought = 0;
}

// 0x00447050: the price 0 and its text emptied; the rating, the bought flag and the refund 0.
void Garage::clear(WindowHost& host) {
    texts.price = 0;
    if (!texts.priceText.empty()) host.baseScreen().setText(texts.priceText, L"");
    texts.newRating = 0.0f;
    texts.bought = 0;
    texts.refund = 0.0f;
}

// 0x00447100: the money shown.
void Garage::cash(WindowHost& host) {
    GameData* data = host.gameData();
    host.baseScreen().setText(texts.cashText, toWide(moneyText(data->career->profile.money, host.language().index)));
}

// 0x004473B0: the price and its box come in (their transitions from the start, shown) or go out (hidden without a
// transition).
void Garage::priceShown(WindowHost& host, bool shown) {
    for (const std::string& name : {texts.priceText, texts.boxText}) {
        if (!name.empty()) host.baseScreen().restartTransition(name, !shown);
    }
}

// 0x00447170: with the class's element the car becoming a prototype (0x004A8F50) or losing it is noted (type 1:
// 1542, 1543; losing it sets the car's +0x0C 0xFF when it was 0); the class's name (604 + its class, 0x0048E9E0) and
// " (P)" for a prototype. The rating (cut) on bhp, and the rating less 100 until 100 or under (from 999 at most) x
// 0.01 on the class's meter. Each text set holds its effect.
void Garage::carInfo(WindowHost& host) {
    GameData* data = host.gameData();
    const TextTable& gameTexts = host.gameTexts();
    Car* car = this->car(host);
    if (car == nullptr) return;
    ui::Screen& s = host.baseScreen();
    if (!texts.classText.empty() && s.find(texts.classText) >= 0) {
        const bool prototype = heavilyTuned(*car, data->catalogue);
        if (!prototype && texts.prototype) {
            host.message(MessageType::Continue, gameTexts.at(kTextNowPrototype + 1), false, nullptr);
            if (car->body[8] == 0) car->body[8] = 0xFF;
        } else if (prototype && !texts.prototype) {
            host.message(MessageType::Continue, gameTexts.at(kTextNowPrototype), false, nullptr);
        }
        texts.prototype = prototype;
        std::wstring text = gameTexts.at(kTextClasses + static_cast<size_t>(carClass(*car)));
        if (prototype) text += L" (P)";
        s.setText(texts.classText, text);
    }
    if (!texts.bhpText.empty() && s.find(texts.bhpText) >= 0) {
        const float rating = std::bit_cast<float>(car->rating);
        s.setText(texts.bhpText, toWide(std::format("{}", x87::chop(rating))));
        if (!texts.classMeter.empty() && s.find(texts.classMeter) >= 0) {
            float v = rating;
            if (v > 999.0f) v = 999.0f;
            while (v > 100.0f) v = v - 100.0f;
            s.setMeter(texts.classMeter, v * 0.01f);
        }
    }
}

// 0x004B5580: once (+0x08): buying (mode 1) the entry's price: under the money "INSUFFICIENT CASH" (1884); else the
// spending noted on the showroom's car (0x0048F270), the price paid, the offer taken out of the offers (0x004B5690)
// and +0x08 set. Selling: the garage car's sale worth (0x0048DA40) paid to the player and +0x08 set.
bool Garage::deal(WindowHost& host) {
    GameData* data = host.gameData();
    const TextTable& gameTexts = host.gameTexts();
    Career& career = *data->career;
    Dealer& dealer = career.dealer;
    if (dealer.deal != 0) return false;
    if (dealer.mode == DealerMode::Offers) {
        const int32_t price = career.dealerEntry != nullptr ? career.dealerEntry->price : 0;
        if (career.profile.money < price) {
            host.message(MessageType::Continue, gameTexts.at(kTextInsufficientCash), false, nullptr);
            return false;
        }
        if (Car* car = this->car(host); car != nullptr) {
            car->spent = std::bit_cast<uint32_t>(std::bit_cast<float>(car->spent) + static_cast<float>(price));
            CareerProfile& p = career.profile;
            const auto spent = static_cast<float>(static_cast<int32_t>(p.moneySpent));
            p.moneySpent = static_cast<uint32_t>(x87::chop(spent + static_cast<float>(price)));
        }
        pay(career, price);
        // 0x004B5690: the offers after the entry's place move down one, every place numbered again.
        const int32_t index = career.dealerEntry != nullptr ? career.dealerEntry->index : 0;
        for (int32_t i = index; i >= 0 && i < dealer.offerCount - 1 && i + 1 < 159; ++i) {
            dealer.offers[static_cast<size_t>(i)] = dealer.offers[static_cast<size_t>(i) + 1];
        }
        for (int32_t i = 0; i < 159; ++i) dealer.offers[static_cast<size_t>(i)].index = i;
        --dealer.offerCount;
    } else {
        const Car* car = this->car(host);
        pay(career, -(car != nullptr ? saleWorth(*car, data->catalogue) : 0));
    }
    dealer.deal = 1;
    return true;
}

// 0x0048BF90 on the garage's car.
void Garage::repaint(WindowHost& host) {
    if (Car* car = this->car(host); car != nullptr && view && view->car() != nullptr) {
        view->car()->paint(*car);
    }
}

// 0x0048ABC0 for the wheels: the car's wheel (+0x12) the part and its SHAPE in the garage's car's wheel slots (-1 the
// CarDef's STANDARDWHEELS, from the car's folders; a part from the shared ones the garage has in use; past the list
// none).
void Garage::fitWheels(WindowHost& host, Car& car, int part) {
    GameData* data = host.gameData();
    std::string mesh;
    if (part == -1) {
        if (car.def != nullptr) mesh = car.def->wheels;
    } else if (part >= 0 && static_cast<size_t>(part) < data->catalogue.lists[kWheel].parts.size()) {
        mesh = data->catalogue.lists[kWheel].parts[static_cast<size_t>(part)].model;
    }
    car.mods[kWheel] = static_cast<uint8_t>(part);
    if (view && view->car() != nullptr) {
        view->car()->fillWheels(mesh, part == -1 ? scene::CarModel::Folders::Car
                                                         : scene::CarModel::Folders::Shared);
    }
}

// 0x0048BC30: with parts in the category's list (not the neon, 9) the car's body part (+0x04 + category) the part and
// its mesh (0x00447690: none for -1, 0x7F or past the list) in the garage's car's slots.
void Garage::fitBodyPart(Car& car, int category, int part) {
    if (car.def == nullptr || category < 0 || category > 8) return;
    const CarPartList& list = car.def->parts[static_cast<size_t>(category)];
    if (list.parts.empty()) return;
    std::string mesh;
    if (part != -1 && part != 0x7F && part >= 0 && static_cast<size_t>(part) < list.parts.size()) {
        mesh = list.parts[static_cast<size_t>(part)].model;
    }
    car.body[static_cast<size_t>(category)] = static_cast<uint8_t>(part);
    if (view && view->car() != nullptr) view->car()->fillBody(category, mesh);
}

// 0x0048B440 and 0x0048B690 on the garage's car: its sponsor decals.
void Garage::refreshParts(WindowHost& host) {
    GameData* data = host.gameData();
    if (Car* car = this->car(host); car != nullptr && view && view->car() != nullptr && data) {
        view->car()->sponsorDecals(*car, data->catalogue);
    }
}

// 0x0048C040 on the garage's car.
void Garage::retint(WindowHost& host) {
    if (Car* car = this->car(host); car != nullptr && view && view->car() != nullptr) {
        view->car()->tint(*car);
    }
}

// The decal (category 8) as decalmenu fits it (0x0048BB00 with the slot DECAL): the car's +0x0C the part, the
// part's mesh (none for -1, 0x7F or past the list) in the slot, the model object's decal material forgotten (+0x130)
// and the decal colour on it (0x0048BEF0).
void Garage::fitDecal(Car& car, int part) {
    if (car.def == nullptr) return;
    const std::vector<CarPart>& parts = car.def->parts[8].parts;
    if (parts.empty()) return;
    std::string mesh;
    if (part != -1 && part != 0x7F && part >= 0 && static_cast<size_t>(part) < parts.size()) {
        mesh = parts[static_cast<size_t>(part)].model;
    }
    car.body[8] = static_cast<uint8_t>(part);
    if (view && view->car() != nullptr) {
        view->car()->fillBody(8, mesh);
        view->car()->colourDecal(car);
    }
}

// The rating of the car with a part of a kind (0x0048E8A0), then its own part back and rated again.
float Garage::rateWithPart(WindowHost& host, Car& car, size_t partKind, uint8_t part) {
    GameData* data = host.gameData();
    const CarDynamics* dynamics = car.def != nullptr ? data->dynamics->of(*car.def) : nullptr;
    const uint8_t own = car.mods[partKind];
    car.mods[partKind] = part;
    const float rating = rateCar(car, dynamics, data->catalogue);
    car.mods[partKind] = own;
    rateCar(car, dynamics, data->catalogue);
    return rating;
}

// 0x0048BE30: the car's exhaust (+0x0F) the part, its mesh in the slots of the garage's car (the catalogue part's
// SHAPE; -1 the CarDef's STANDARDEXHAUST: none without one, the car's exhaust then left; past the list none).
void Garage::fitExhaust(WindowHost& host, Car& car, int part) {
    GameData* data = host.gameData();
    std::string mesh;
    if (part == -1) {
        if (car.def == nullptr || car.def->exhaust.empty()) return;
        mesh = car.def->exhaust;
    } else if (part >= 0 && static_cast<size_t>(part) < data->catalogue.lists[kExhaust].parts.size()) {
        mesh = data->catalogue.lists[kExhaust].parts[static_cast<size_t>(part)].model;
    }
    car.mods[kExhaust] = static_cast<uint8_t>(part);
    if (view && view->car() != nullptr) view->car()->fillExhaust(mesh);
}

// The customising windows' frame: the chassis lowered by MAXDROPHEIGHT x the car's +0x58.
void Garage::lowerChassis(WindowHost& host) {
    if (Car* car = this->car(host); car != nullptr && view) view->dropChassis(*car);
}

// 0x00440320, the leave of exhaustmenu, gearboxmenu and the styling windows: the highlighted item kept; the garage's
// price and its box go out.
void Garage::leavePriced(WindowHost& host, ui::Screen& s, const std::string& state) {
    host.baseLeave(s, state);
    priceShown(host, false);
}

}  // namespace oj::frontend
