// The career's menu (CMSMainMenu).

#include "Game/Frontend/Career/CareerMenu.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <cstring>

namespace oj::frontend {

// 0x004CE160: the career's top-up (0x004CE870) and free repair (0x004CE910), Cash and Date (0x004A4130), the window's
// +0x0C 0 and its answer -1, the dealer's +0x10 0; the menu's Workshop item enabled with cars in the garage.
void CareerMenu::enter(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    topUp(host, career);
    freeRepair(host, career);
    careerCashTexts(s, career, host.language().index, texts);
    *this = CareerMenu{};
    career.dealer.afterRace = 0;
    s.setMenuItemEnabled("menu", "Workshop", !career.garage.empty());
}

// 0x004CE1F0: BackMenu follows menu; Cash and Date; the phone's frame (0x004B60F0). The career's +0x78 object asks
// for windows: +0x04 CMSMobilePhone over this one; +0x10 the dealership
// (the profile's +0x4E4 the money, the dealer's +0x00 1, +0x08, +0x27D4, +0x27DC 0, its offers listed 0x004B4620,
// +0x27D8 its +0x27E8); +0x0C or +0x08 CMSCalendar (+0x0C cleared). A due challenge's advice (0x004A57B0)
// with the profile's +0x4E over 3 chooses the advice (0x004A57E0) and opens CMSAdvice over this one. Leaving (+0x0C)
// goes back to menu (mode 2);
// a "yes" (+0x10) to leaving asks for the save system's autosave over the window when saving is on and leaves.
// BACK (CLICK) asks "Save and exit career mode?" (with saving on) or "Leave career mode?". SELECT (CLICK) by the
// item: GoRace CMSCalendar, Advice the advice chosen (0x004A57E0) and CMSAdvice over this one, Dealership the
// dealership as above, Workshop CMSCarCollection2 (the profile's +0x4E4 the money, the dealer's garage shown,
// 0x004B43C0 mode 0), Respect CMSRespectOverview, Crew CMSCrew, Phone CMSMobilePhone over this one, Stats CMSStatsMenu.
void CareerMenu::update(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    CareerProfile& p = career.profile;
    Dealer& dealer = career.dealer;
    s.selectMenuItem("BackMenu", s.selectedMenuItem("menu"));
    careerCashTexts(s, career, host.language().index, texts);
    host.phoneFrame(s);
    const auto dealership = [&]() {
        p.moneySaved = static_cast<uint32_t>(p.money);
        dealer.mode = DealerMode::Offers;
        dealer.listCount = 0;
        dealer.deal = 0;
        dealer.chosen = 0;
        dealerOffersList(dealer);
        dealer.listCars = dealer.showroom;
        host.request("CMSCarDealership", SwitchMode::Replace);
    };
    if (career.advisor.openPhone != 0) {
        career.advisor.openPhone = 0;
        host.request("CMSMobilePhone", SwitchMode::Push);
        return;
    }
    if (career.advisor.openDealer != 0) {
        career.advisor.openDealer = 0;
        dealership();
        return;
    }
    if (career.advisor.openCalendar != 0 || career.advisor.openHosting != 0) {
        career.advisor.openCalendar = 0;
        host.request("CMSCalendar", SwitchMode::Replace);
        return;
    }
    if (adviceDue(career) && p.respectLevel > 3) {
        chooseAdvice(career, gameRandom(), data->catalogue);
        host.request("CMSAdvice", SwitchMode::Push);
        return;
    }
    if (m_leaving != 0) {
        host.request("menu", SwitchMode::ReplaceNow);
        return;
    }
    if (m_answer == 1) {
        if (data->saving != 0) host.request("autosave", SwitchMode::Push);
        m_leaving = 1;
        return;
    }
    if (event == input::kBack) {
        host.sound("CLICK");
        host.message(MessageType::YesNo, texts.at(data->saving != 0 ? kTextSaveAndExit : kTextLeaveCareer), false,
                     &m_answer, true);
        return;
    }
    if (event != input::kSelect) return;
    const std::string item = s.menuItemCommand("menu", s.selectedMenuItem("menu"));
    host.sound("CLICK");
    const auto is = [&](const char* name) { return _stricmp(item.c_str(), name) == 0; };
    if (is("GoRace")) {
        host.request("CMSCalendar", SwitchMode::Replace);
    } else if (is("Advice")) {
        chooseAdvice(career, gameRandom(), data->catalogue);
        host.request("CMSAdvice", SwitchMode::Push);
    } else if (is("Dealership")) {
        dealership();
    } else if (is("Workshop")) {
        p.moneySaved = static_cast<uint32_t>(p.money);
        dealerShow(career, DealerMode::Garage, data->tables, data->catalogue);
        host.request("CMSCarCollection2", SwitchMode::Replace);
    } else if (is("Respect")) {
        host.request("CMSRespectOverview", SwitchMode::Replace);
    } else if (is("Crew")) {
        host.request("CMSCrew", SwitchMode::Replace);
    } else if (is("Phone")) {
        host.request("CMSMobilePhone", SwitchMode::Push);
    } else if (is("Stats")) {
        host.request("CMSStatsMenu", SwitchMode::Replace);
    }
}

// 0x004CE870: a career without cars and with less than 30000 gets the rest of 30000 (the money counter +0x04, none
// below 0) and one of four notes (lines 1955 + rand() & 3).
void CareerMenu::topUp(CareerHost& host, Career& career) {
    const TextTable& texts = host.gameTexts();
    CareerProfile& p = career.profile;
    if (!career.garage.empty() || p.money >= 30000) return;
    const int32_t rest = 30000 - p.money;
    if (rest > 0) {
        career.stats.moneyReceived += rest;
    } else {
        career.stats.moneyPaid -= rest;
    }
    p.money += rest;
    if (p.money < 0) p.money = 0;
    const auto n = static_cast<size_t>(gameRandom().next() & 3);
    host.message(MessageType::Continue, texts.at(kTextTopUps + n), false, nullptr);
}

// 0x004CE910: when every garage car needs a repair (its grid damaged, its +0x74 or +0x7C over 0.1, or its nitro not
// full with a nitro part) that costs at least the money, one of four notes (lines 1960 + rand() & 3) and every car
// repaired (0x0048EB20).
void CareerMenu::freeRepair(CareerHost& host, Career& career) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (career.garage.empty()) return;
    const auto money = static_cast<float>(career.profile.money);
    for (const Car& car : career.garage) {
        bool needs = car.grid && damageLevel(*car.grid) != 0.0f;
        needs = needs || std::bit_cast<float>(car.tyreWear) > 0.1f || std::bit_cast<float>(car.brakeWear) > 0.1f;
        if (!needs) {
            if (car.mods[kNitro] == kNoMod) return;
            if (std::bit_cast<float>(car.nitro) == 15.0f) return;
        }
        if (money > repairCost(car, data->catalogue)) return;
    }
    const auto n = static_cast<size_t>(gameRandom().next() & 3);
    host.message(MessageType::Continue, texts.at(kTextFreeRepairs + n), false, nullptr);
    for (Car& car : career.garage) repairCar(car);
}

}  // namespace oj::frontend
