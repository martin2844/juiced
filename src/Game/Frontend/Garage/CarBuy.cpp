// carbuy: buying a car of the dealer's.

#include "Game/Frontend/Garage/CarBuy.h"

#include "Core/Base/Log.h"
#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

namespace oj::frontend {

// 0x00438150: [0x0075ECF8] the career; PriceVal (emptied), CashVal and CashBox the garage's texts; every message
// hidden; CashVal the money; with Frame_Car3D a damaged used offer's damage put on the car (0x004B5700) and the scene
// (GarageView::enterBuy); the price the entry's (+0x1C); [0x00728C60] the car; sel_text "BUY" (459) for mode 1, else
// "Sell" (463); +0x2C -1, +0x30 0, +0x34 1, +0x38 0; Cash and Date.
void CarBuy::enter(GarageHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    *this = CarBuy{};
    garage.texts = GarageTexts{};
    garage.texts.priceText = "PriceVal";
    garage.texts.cashText = "CashVal";
    garage.texts.boxText = "CashBox";
    s.setText("PriceVal", L"");
    s.hideContaining("message");
    garage.cash(host);
    garage.view.reset();
    if (s.find("Frame_Car3D") >= 0) {
        const DealerOffer* offer = career.dealerEntry;
        const bool buying = career.dealer.mode == DealerMode::Offers;
        if (buying && offer != nullptr && offer->used != 0 && offer->damage != 0) {
            damageShowroomCar(career, gameRandom());
        }
        if (host.engine() != nullptr && garage.car(host) != nullptr) {
            garage.view = std::make_unique<scene::GarageView>(*host.engine(), s, "Frame_Car3D", data->catalogue);
            garage.view->enterBuy(*garage.car(host), offer, buying, host.viewLoading());
        }
    }
    g_statsCar = garage.car(host);
    if (g_statsCar != nullptr) garage.texts.prototype = heavilyTuned(*g_statsCar, data->catalogue);
    garage.price(host, career.dealerEntry != nullptr ? career.dealerEntry->price : 0);
    s.setText("sel_text", texts.at(career.dealer.mode == DealerMode::Offers ? kTextBuy : kTextSell));
    careerCashTexts(s, career, host.language().index, texts);
}

// 0x00438BB0: the plinth turned (a new car); +0x28: CMSMainMenu. SELECT: the deal made (+0x38): with the save system
// saving the dealer's deal finished (0x004B5490), its list again (0x004B43C0 by its mode) and the autosave over the
// window; +0x28 1. Else, not asked
// (+0x30): when the deal may be made (0x004B5550: the dealer's +0x08 0 and, buying, the price within the money) a full
// garage (55) for buying notes 1613 (CLICK); else "Buy this car for" / "Sell this car for" (460, 461), the price and
// "ARE YOU SURE?" (type 4, on NO) into +0x2C, +0x30 1; when it may not, every second SELECT NO and "INSUFFICIENT CASH"
// (type 2, +0x34). BACK, not asked (CLICK): CMSCarDealership. Asked: a "yes" makes the deal (0x004B5580): CLICK, the
// money shown, "Car purchased" (458) or "Car sold" (457, type 2), +0x38 1, +0x30 0; failing +0x2C -1, +0x30 0; a "no"
// +0x30 0. Then the chassis lowered.
void CarBuy::update(GarageHost& host, ui::Screen& /*s*/, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    // 0x00438BE6: the plinth turns 0.35 a second for a new car.
    if (garage.view) garage.view->turnPlinth(host.frameTime() * 0.35f);
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    Dealer& dealer = career.dealer;
    if (m_leaving != 0) {
        host.request("CMSMainMenu", SwitchMode::Replace);
        return;
    }
    const bool buying = dealer.mode == DealerMode::Offers;
    if (event == input::kSelect) {
        if (m_done != 0) {
            if (data->saving != 0) {
                dealerFinish(career);
                dealerShow(career, dealer.mode, data->tables, data->catalogue);
                host.request("autosave", SwitchMode::Push);
            }
            m_leaving = 1;
        } else if (m_asked == 0) {
            const int32_t price = career.dealerEntry != nullptr ? career.dealerEntry->price : 0;
            const bool may = dealer.deal == 0 && (!buying || price <= career.profile.money);
            if (may) {
                if (buying && career.garage.size() == 55) {
                    host.sound("CLICK");
                    host.message(MessageType::Continue, texts.at(kTextGarageFullToBuy), false, nullptr);
                    return;
                }
                const std::wstring text = texts.at(buying ? kTextBuyFor : kTextSellFor) + L" " +
                                          toWide(moneyText(price, host.language().index)) + L"\n" +
                                          texts.at(kTextAreYouSure);
                host.sound("CLICK");
                host.message(MessageType::YesNoModeless, text, false, &m_answer, true);
                m_asked = 1;
                m_answer = -1;
            } else if (m_note != 0) {
                host.sound("NO");
                host.message(MessageType::ContinueModeless, texts.at(kTextInsufficientCash), false, nullptr);
                m_note = 0;
            } else {
                m_note = 1;
            }
        }
    } else if (event == input::kBack && m_asked == 0) {
        host.sound("CLICK");
        if (host.messageUp()) host.closeMessage();
        host.request("CMSCarDealership", SwitchMode::Replace);
    }
    if (m_asked != 0) {
        if (m_answer == 1) {
            if (garage.deal(host)) {
                host.sound("CLICK");
                garage.cash(host);
                host.message(MessageType::ContinueModeless, texts.at(kTextCarSold + (buying ? 1 : 0)), false, nullptr);
                m_done = 1;
                m_asked = 0;
            } else {
                m_answer = -1;
                m_asked = 0;
            }
        } else if (m_answer == 0) {
            m_asked = 0;
        }
    }
    // 0x00438FDF: the chassis lowered by MAXDROPHEIGHT x the car's ride (the window's car object and the garage's
    // [0x0075ED00], the same here); not after the leave or the full garage's note.
    garage.lowerChassis(host);
}

// 0x00438A70: a Menu element's highlighted item kept; the views freed; the deal finished (0x004B5490) and the dealer's
// list again by its mode (0x004B43C0).
void CarBuy::leave(GarageHost& host) {
    GameData* data = host.gameData();
    Garage& garage = host.garage();
    if (garage.view) garage.view->clear();
    garage.view.reset();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    dealerFinish(career);
    dealerShow(career, career.dealer.mode, data->tables, data->catalogue);
}

}  // namespace oj::frontend
