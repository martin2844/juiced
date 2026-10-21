// The dealer's cars and the player's garage in the career (CMSCarDealership, CMSCarCollection2).

#include "Game/Frontend/Garage/Dealership.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/Garage/CarStats.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>

namespace oj::frontend {

namespace {

constexpr const char* kDealerDetails[] = {"state", "condition", "bhp", "value", "class", "drive"};

}  // namespace

// 0x004B4680: the texts of the details emptied; carmenu an item for each entry of the dealer's list (with a class
// chosen, +0x27E4 not -1, only those of that class, 0x00447480): the model's display name (0x0048CC30) with " (M)"
// for +0x20, " (D)" for +0x24 and " (P)" for +0x28, the value the entry's place.
void Dealership::menu(GarageHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const Career& career = *data->career;
    const Dealer& dealer = career.dealer;
    for (const char* name : kDealerDetails) {
        const int e = s.find(name);
        if (e >= 0) s.clearText(e);
    }
    s.clearMenu("carmenu");
    ++m_carMenuFill;
    const int32_t count = std::min(dealer.listCount, static_cast<int32_t>(dealer.list.size()));
    for (int32_t i = 0; i < count; ++i) {
        const DealerOffer& e = dealer.list[static_cast<size_t>(i)];
        if (dealer.listClass != -1 &&
            ratingClass(static_cast<float>(e.rating)) != dealer.listClass) {
            continue;
        }
        const CarDef* def = findCarDef(data->defs, data->tables.carModels[static_cast<size_t>(e.model)]);
        std::wstring text = def != nullptr ? widenSigned(def->displayName) : std::wstring();
        if (e.modified != 0) text += L" (M)";
        if (e.damage != 0) text += L" (D)";
        if (e.prototypeParts != 0) text += L" (P)";
        s.addMenuItem("carmenu", text, static_cast<float>(i));
    }
}

// 0x004B48D0: the details of the list's entry highlighted in carmenu: state New or Used (445 + its +0x08), condition
// (447 + its +0x0C), bhp its rating (thousands marked, 0x004A3AF0), drive (1830 + its +0x10), class (604 + the
// class of the rating, 0x00447480) and value its price (0x004A3A90).
void Dealership::details(GarageHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    const Dealer& dealer = data->career->dealer;
    if (s.menuSize("carmenu") == 0) return;
    const int32_t place = x87::chop(s.menuItemValue("carmenu", s.selectedMenuItem("carmenu")));
    const DealerOffer& e = dealer.list[static_cast<size_t>(place)];
    const int language = host.language().index;
    s.setText("state", texts.at(kTextNewUsed + static_cast<size_t>(e.used)));
    s.setText("condition", texts.at(kTextConditions + static_cast<size_t>(e.condition)));
    s.setText("bhp", toWide(groupThousands(e.rating, language)));
    s.setText("drive", texts.at(kTextDrives + static_cast<size_t>(e.modelClass)));
    if (s.find("class") >= 0) {
        const auto rated = static_cast<size_t>(ratingClass(static_cast<float>(e.rating)));
        s.setText("class", texts.at(kTextClasses + rated));
    }
    s.setText("value", toWide(moneyText(e.price, language)));
}

// 0x004C2AD0 (CMSCarCollection2 and CMSCarDealership): +0x0C -1; the dealer in mode 1 shows its offers (0x004B4620,
// +0x27D8 its +0x27E8), else mode 0 and the garage (0x004B4440, +0x27D8 the garage), +0x27D4, +0x08, +0x27DC 0; Cash
// and Date (0x004A4130); with Repair_Text and Repair_But both there, Repair_Text "REPAIR CAR" (1593) and both hidden;
// +0x18 0, +0x1C -1. The car view (+0x20, 0x0045A2C0 on CAR3D, LOGO and ORIGIN, the zoom 0.7, the mirror image, the
// car's folder kept and the shadow on) and FADEBOX (+0x24). A first visit (the profile's +0x00 set, then cleared): the
// career's head 1, the garage emptied (0x00489C30) and the history's step (0x004BD8A0). +0x2C and the dealer's +0x27E4
// (the class shown) -1, carmenu filled (0x004B4680), the dealer's cars made (0x004B5330), +0x30 0; carmenu's first item
// highlighted, then the one left highlighted ([0x0075EF68] for the offers, [0x0075EF6C] for the garage) when the menu
// is that long. The elements with SPEC_ or SEL_ shown with a car to show (mode 0 a garage car, else an item in
// carmenu), else hidden.
void Dealership::enter(GarageHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    Dealer& dealer = career.dealer;
    CareerProfile& p = career.profile;
    m_shown = -1;
    dealerShow(career, dealer.mode == DealerMode::Offers ? DealerMode::Offers : DealerMode::Garage, data->tables,
               data->catalogue);
    careerCashTexts(s, career, host.language().index, texts);
    m_repairText = s.find("Repair_Text") >= 0;
    m_repairButton = s.find("Repair_But") >= 0;
    if (m_repairText && m_repairButton) {
        s.setText("Repair_Text", texts.at(kTextRepairCar));
        s.setHidden("Repair_Text", true);
        s.setHidden("Repair_But", true);
    }
    m_repairable = 0;
    m_repairAnswer = -1;
    m_view.reset();
    if (host.engine() != nullptr) {
        scene::CarView::Setup setup;
        setup.element = "CAR3D";
        setup.logo = "LOGO";
        setup.origin = "ORIGIN";
        setup.zoom = 0.7f;
        setup.mirror = true;
        setup.keepFolder = true;
        setup.shadow = true;
        m_view = std::make_unique<scene::CarView>(*host.engine(), s, setup, data->catalogue, data->tables);
    }
    if (p.introduction != 0) {
        career.head = 1;
        p.introduction = 0;
        career.garage.clear();
        recordHistory(career);
    }
    m_unused = -1;
    dealer.listClass = -1;
    menu(host, s);
    data->makeDealerCars(gameRandom());
    m_leaving = 0;
    s.selectMenuItem("carmenu", 0);
    const size_t row = dealer.mode == DealerMode::Offers ? m_offersRow : m_garageRow;
    if (s.menuSize("carmenu") > row) s.selectMenuItem("carmenu", row);
    const bool any = dealer.mode == DealerMode::Garage ? !career.garage.empty() : s.menuSize("carmenu") != 0;
    if (any) {
        s.showContaining("SPEC_", false);
        s.showContaining("SEL_", false);
    } else {
        s.hideContaining("SPEC_");
        s.hideContaining("SEL_");
    }
}

// 0x004C2F10: the car view's frame (0x00459EE0) and FADEBOX's alpha 1 - the view's +0x4C. An empty carmenu empties
// the view (0x0045AE90). With an item highlighted in carmenu: when its value is not the place shown (+0x0C) and it was
// highlighted the frame before too ([0x0079E854]), +0x0C takes it (to the nearest) and the view shows that car
// (0x0045A080, 2.64, 1, fading: mode 1 the dealer's car of the place, else the garage car of the entry); in mode 0
// with the repair elements the entry's damage (+0x24) shows them and sets +0x18, else hides them and clears it.
// Without the view busy (its +0x40 and +0x3C) the details (0x004B48D0) and their six elements shown, else hidden.
// +0x30 set: CMSMainMenu. BACK (CLICK): the dealer's car
// bought or sold made so (0x004B5490); the money changed since the profile's +0x4E4: that taken and the autosave over
// this window when the save system saves; +0x30 set. SELECT with an item (CLICK): the entry taken to the garage
// (0x004B5070), the dealer's +0x0C 0, garageload and the career's +0xA4 0; without one NO. The repair
// answered "yes" (+0x1C): +0x18 0, +0x0C -1, its cost paid (as 0x004B8200) and noted (0x0048F270), the car repaired
// (0x0048EB20), the garage listed again (mode 0, 0x004B4440), carmenu filled again, Cash and Date, +0x1C -1. The
// repair's button (event 7) with +0x18 (CLICK): its cost over the money "INSUFFICIENT CASH", else "Repair this car
// for <cost>" and "ARE YOU SURE?" into +0x1C. PGDN in mode 0 on a car with the view not busy (0x0045A880): CLICK,
// [0x00721F30] 1, the car ([0x00728C60], 0x004B5470) and car_stats2purple over the window; else NO.
void Dealership::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    Dealer& dealer = career.dealer;
    CareerProfile& p = career.profile;
    const bool any = s.menuSize("carmenu") != 0;
    const size_t item = any ? s.selectedMenuItem("carmenu") : 0;
    const float value = any ? s.menuItemValue("carmenu", item) : 0.0f;
    if (m_view) {
        m_view->frame(host.frameTime(), gameRandom());
        if (const int fade = s.find("FADEBOX"); fade >= 0) s.setAlpha(fade, 1.0f - m_view->shown());
        if (!any) m_view->clear();
    }
    if (any) {
        const bool settled = m_lastCarFill == m_carMenuFill && m_lastCarItem == item;
        if (!(static_cast<float>(m_shown) == value) && settled) {
            m_shown = static_cast<int32_t>(std::nearbyint(value));
            if (m_view) {
                const auto place = static_cast<size_t>(m_shown);
                if (dealer.mode == DealerMode::Offers) {
                    if (place < career.dealerCars.size()) {
                        m_view->show(career.dealerCars[place], &m_carsCurrent, static_cast<uint32_t>(place),
                                     2.64f, 1.0f, true, host.viewLoading());
                    }
                } else if (place < dealer.list.size()) {
                    const auto index = static_cast<uint32_t>(dealer.list[place].index);
                    if (index < career.garage.size()) {
                        m_view->show(career.garage[index], &career.garageTail, index, 2.64f, 1.0f, true,
                                     host.viewLoading());
                    }
                }
            }
            if (dealer.mode == DealerMode::Garage && m_repairText && m_repairButton) {
                const bool damaged = dealer.list[static_cast<size_t>(x87::chop(value))].damage != 0;
                s.setHidden("Repair_Text", !damaged);
                s.setHidden("Repair_But", !damaged);
                m_repairable = damaged ? 1 : 0;
            }
        }
        m_lastCarFill = m_carMenuFill;
        m_lastCarItem = item;
        if (!m_view || !m_view->busy()) {
            details(host, s);
            for (const char* name : kDealerDetails) s.setHidden(name, false);
        } else {
            for (const char* name : kDealerDetails) s.setHidden(name, true);
        }
    }
    if (m_leaving != 0) {
        host.request("CMSMainMenu", SwitchMode::Replace);
        return;
    }
    if (event == input::kBack) {
        host.sound("CLICK");
        dealerFinish(career);
        if (p.moneySaved != static_cast<uint32_t>(p.money)) {
            p.moneySaved = static_cast<uint32_t>(p.money);
            if (data->saving != 0) host.request("autosave", SwitchMode::Push);
        }
        m_leaving = 1;
    } else if (event == input::kSelect) {
        if (any) {
            host.sound("CLICK");
            if (data->dealerTake(gameRandom(), x87::chop(value)) != 0) {
                dealer.fromStats = 0;
                host.request("garageload", SwitchMode::Replace);
                data->careerScreen = CareerScreen::CarSel;
            }
        } else {
            host.sound("NO");
        }
    }
    if (m_repairAnswer == 1 && any) {
        m_repairable = 0;
        m_shown = -1;
        if (Car* repaired = dealerGarageCar(career, x87::chop(value)); repaired != nullptr) {
            Car& car = *repaired;
            pay(career, x87::chop(repairCost(car, data->catalogue)));
            const float cost = repairCost(car, data->catalogue);
            car.spent = std::bit_cast<uint32_t>(std::bit_cast<float>(car.spent) + cost);
            const auto spent = static_cast<float>(static_cast<int32_t>(p.moneySpent));
            p.moneySpent = static_cast<uint32_t>(x87::chop(spent + cost));
            repairCar(car);
        }
        dealerShow(career, DealerMode::Garage, data->tables, data->catalogue);
        menu(host, s);
        careerCashTexts(s, career, host.language().index, texts);
        m_repairAnswer = -1;
    }
    if (event == input::kSelect3) {
        if (m_repairable == 0 || !any) return;
        host.sound("CLICK");
        const Car* asked = dealerGarageCar(career, x87::chop(value));
        if (asked == nullptr) return;
        const float cost = repairCost(*asked, data->catalogue);
        if (cost > static_cast<float>(p.money)) {
            host.message(MessageType::Continue, texts.at(kTextInsufficientCash), false, nullptr);
        } else {
            const std::wstring text = texts.at(kTextRepairFor) + L" " +
                                      toWide(moneyText(x87::chop(cost), host.language().index)) + L"\n" +
                                      texts.at(kTextAreYouSure);
            host.message(MessageType::YesNo, text, false, &m_repairAnswer, true);
            m_repairAnswer = -1;
        }
    } else if (event == input::kSelect2 && dealer.mode == DealerMode::Garage) {
        if (any && (!m_view || !m_view->busy())) {
            host.sound("CLICK");
            host.carStatsPurple().setGarage(1);
            g_statsCar = dealerGarageCar(career, x87::chop(value));
            host.request("car_stats2purple", SwitchMode::Push);
        } else {
            host.sound("NO");
        }
    }
}

// 0x004C2E70: the state keeps a Menu element's highlighted item; the view freed (0x0045AE90, 0x0045A340); carmenu's
// highlighted item kept for the next visit ([0x0075EF68] in mode 1, else [0x0075EF6C]); the dealer's cars freed
// (0x00489C30).
void Dealership::leave(GarageHost& host) {
    GameData* data = host.gameData();
    if (m_view) {
        m_view->clear();
        m_view.reset();
    }
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    if (host.baseScreen().find("carmenu") >= 0) {
        const size_t row = host.baseScreen().selectedMenuItem("carmenu");
        if (career.dealer.mode == DealerMode::Offers) {
            m_offersRow = row;
        } else {
            m_garageRow = row;
        }
    }
    career.dealerCars.clear();
}

}  // namespace oj::frontend
