// carsel: choosing a car of the garage.

#include "Game/Frontend/Garage/CarSel.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/Garage/CarStats.h"
#include "Game/Frontend/WindowTools.h"

namespace oj::frontend {

// 0x00437000: the money changed since the profile's +0x4E4: that taken, the autosave over the window when saving, and
// the leave next frame (+0x20). Else CLICK and, in the career, CMSStatsCarCollection (the dealer's +0x0C), the
// dealership (mode 1), CMSMainMenu (+0x10) or CMSCarCollection2; outside it menu.
void CarSel::leaveGarage(GarageHost& host) {
    GameData* data = host.gameData();
    Career& career = *data->career;
    CareerProfile& p = career.profile;
    Dealer& dealer = career.dealer;
    if (p.moneySaved != static_cast<uint32_t>(p.money)) {
        p.moneySaved = static_cast<uint32_t>(p.money);
        if (data->saving != 0) host.request("autosave", SwitchMode::Push);
        m_leave = 1;
        return;
    }
    host.sound("CLICK");
    if (data->race.mode != RaceMode::Career) {
        host.request("menu", SwitchMode::Replace);
    } else if (dealer.fromStats != 0) {
        host.request("CMSStatsCarCollection", SwitchMode::Replace);
    } else if (dealer.mode == DealerMode::Offers) {
        host.request("CMSCarDealership", SwitchMode::Replace);
    } else if (dealer.afterRace != 0) {
        host.request("CMSMainMenu", SwitchMode::Replace);
    } else {
        host.request("CMSCarCollection2", SwitchMode::Replace);
    }
}

void CarSel::music(GarageHost& host, bool on) {
    GameData* data = host.gameData();
    Jukebox& music = jukebox();
    const float volume = std::bit_cast<float>(host.settingsValue(save::Settings::kMusicVolume));
    if (on) {
        host.setMusicVolume(0.0f, true);
        if (volume == 0.0f) return;
        if (!music.playing()) music.play(data != nullptr ? data->playlist : std::vector<uint32_t>{}, true, true);
        if (music.player() != nullptr && music.handle != 0) music.target = volume;
        m_music = 1;
    } else {
        if (music.player() != nullptr && music.handle != 0) music.target = 0.0f;
        host.startMusic();
        host.setMusicVolume(volume, true);
        m_music = 0;
    }
}

// 0x00436D40: the garage's menu hidden for a window over it (menu going out, 0x005A9B40; backmenu, aniline,
// animline2, menufade, frame_metalline2 and infobox hidden, each name without regard to case; BACK_TEXT "BACK") or
// shown again (infobox, menu with its transition, backmenu, aniline, animline2, menufade, frame_metalline2; BACK_TEXT
// "CAREER MENU" for the career menu's garage, the dealer's mode 0 with its +0x10); +0x18 takes it.
void CarSel::setMenuHidden(GarageHost& host, ui::Screen& s, bool hidden) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Dealer& dealer = data->career->dealer;
    if (hidden) {
        s.restartTransition("menu", true);
        for (const char* name : {"backmenu", "aniline", "animline2", "menufade", "frame_metalline2", "infobox"}) {
            s.setHidden(name, true);
        }
        s.setText("BACK_TEXT", texts.at(kTextBack));
    } else {
        s.show("infobox", false);
        s.show("menu", true);
        for (const char* name : {"backmenu", "aniline", "animline2", "menufade", "frame_metalline2"}) {
            s.show(name, false);
        }
        const bool career = dealer.mode == DealerMode::Garage && dealer.afterRace != 0;
        s.setText("BACK_TEXT", texts.at(career ? kTextCareerMenu : kTextBack));
    }
    m_menuHidden = hidden ? 1 : 0;
}

// 0x00436380: [game + 4]'s method +0x38 with 1; +0x20 0; BACK_TEXT as above; the race's type 5 and laps 1; the garage's
// texts Price, frame_Cash, PriceBox, frame_Class, frame_ClassMeter, frame_Bhp; Price emptied; +0x1C 0; the money
// shown; the scene (0x004B5700 for a damaged offer, GarageView::enterSel; the garage list's +0x10 0); [0x00728C60] the
// car and its class, prototype and rating (0x00447170). menu and backmenu back to their items (0x005AECF0). A car
// without damage (0x00557E80, 0x0048EF10) loses menu's repair item (0x005AFAD0) and
// backmenu's first (0x005AFAA0), else both select their first; every item of menu enabled, for a car with damage or
// worn parts those named cust, race, testtrack, shop, rolling and scheme (case aside) disabled. The view in the map
// mode turning about the car; +0x34 0, +0x24 .. +0x30 -1; Cash and Date; menu's first item highlighted with its help.
// A car neither damaged nor worn turns the garage's music on (0x00436C60).
void CarSel::enter(GarageHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    *this = CarSel{};
    const bool careerMenu = career.dealer.mode == DealerMode::Garage && career.dealer.afterRace != 0;
    s.setText("BACK_TEXT", texts.at(careerMenu ? kTextCareerMenu : kTextBack));
    data->race.type = RaceType::Circuit;
    data->race.laps = 1;
    garage.texts = GarageTexts{};
    garage.texts.priceText = "Price";
    garage.texts.cashText = "frame_Cash";
    garage.texts.boxText = "PriceBox";
    garage.texts.classText = "frame_Class";
    garage.texts.classMeter = "frame_ClassMeter";
    garage.texts.bhpText = "frame_Bhp";
    s.setText("Price", L"");
    garage.cash(host);
    Car* car = garage.car(host);
    garage.view.reset();
    const bool view3d = s.find("FRAME_CAR3D") >= 0;
    if (view3d) {
        damageShowroomCar(career, gameRandom());
        if (host.engine() != nullptr && car != nullptr) {
            garage.view = std::make_unique<scene::GarageView>(*host.engine(), s, "FRAME_CAR3D", data->catalogue);
            garage.view->enterSel(*car, host.viewLoading());
        }
    }
    g_statsCar = car;
    if (car != nullptr) garage.texts.prototype = heavilyTuned(*car, data->catalogue);
    garage.carInfo(host);
    // 0x00436200 with 0: the garage list's +0x10 0.
    if (view3d) career.garageTail = 0;
    s.restoreMenu("menu");
    s.restoreMenu("backmenu");
    if (s.find("menu") >= 0 && car != nullptr) {
        const bool damaged = gridDamaged(*car) || partsWorn(*car);
        if (!damaged) {
            s.removeMenuItem("menu", "repair");
            if (s.find("backmenu") >= 0) s.removeMenuItemAt("backmenu", 0);
        } else {
            s.selectMenuItem("menu", 0);
            if (s.find("backmenu") >= 0) s.selectMenuItem("backmenu", 0);
        }
        for (size_t i = 0; i < s.menuSize("menu"); ++i) {
            const std::string command = s.menuItemCommand("menu", i);
            bool enabled = true;
            if (damaged) {
                for (const char* name : {"cust", "race", "testtrack", "shop", "rolling", "scheme"}) {
                    if (_stricmp(name, command.c_str()) == 0) enabled = false;
                }
            }
            s.setMenuItemEnabledAt("menu", i, enabled);
        }
    }
    // 0x004369B0: the view in the map mode (0x00436010) and again looking at (0, 50, 0), the map mode and the turn
    // -0.1: the camera goes from the car's side (0x00436200) to the rig's arm and turns about the car.
    if (garage.view) {
        garage.view->orbit();
        garage.view->mapMode();
    }
    careerCashTexts(s, career, host.language().index, texts);
    // 0x00436A24: menu's first item highlighted (+0x13C and +0x148 0) and its help in the ticker (as 0x005B5240), then
    // the highlighted item's help again: without the repair item the ticker no longer holds its help.
    if (s.find("menu") >= 0) s.selectMenuItem("menu", 0);
    // 0x00436B6E: a car neither damaged nor worn turns the garage's music on.
    if (car != nullptr && !gridDamaged(*car) && !partsWorn(*car)) music(host, true);
}

// 0x004371C0: Cash and Date; +0x20: the leave (0x00437000). backmenu follows menu; a window over it gone, menu shown
// again (0x00436D40). The repair item highlighted with the car damaged shows its price (the repair's cost, cut) once
// (+0x1C), another item hides it. SELECT on an item (CLICK): cust (custmenu), shop (modshop), details (car_stats2, its
// garage flag [0x00721DE8] 1), maintenance (car_maintain), scheme (schememenu) over the window with the menu hidden;
// leave the leave; testtrack (the session set up for it, 0x004B9670; the career's +0xA8 1, +0xA4 4, the profile's
// +0x4E4 -1) and rolling (+0xA4 5) garageload; these and cust for a damaged car "Car is damaged" (475) instead; sell
// "Sell this car for" and the sale worth and "ARE YOU SURE?" into +0x2C; repair, once (+0x34), "INSUFFICIENT CASH" over
// the money (NO), else "Repair this car for", the cost and "ARE YOU SURE?" into +0x24. BACK, no repair asked: the
// leave. +0x28 1 (no question answers into it): CLICK, +0xA4 5, garageload. A "yes" to selling: CLICK, the sale
// (0x004B5580), CMSCarCollection2. A "yes" to the repair: its cost paid and noted (0x0048F270), the car repaired
// (0x0048EB20), +0xA4 7, garageload; a "no" +0x34 0. Then the chassis lowered.
void CarSel::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Garage& garage = host.garage();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    CareerProfile& p = career.profile;
    careerCashTexts(s, career, host.language().index, texts);
    if (m_leave != 0) {
        m_leave = 0;
        leaveGarage(host);
        return;
    }
    Car* car = garage.car(host);
    const bool damaged = car != nullptr && (gridDamaged(*car) || partsWorn(*car));
    const bool menu = s.find("menu") >= 0 && s.menuSize("menu") != 0;
    std::string command;
    if (s.find("menu") >= 0) {
        if (s.find("backmenu") >= 0 && menu) s.selectMenuItem("backmenu", s.selectedMenuItem("menu"));
        if (m_menuHidden != 0) setMenuHidden(host, s, false);
        if (menu) command = s.menuItemCommand("menu", s.selectedMenuItem("menu"));
        const auto is1 = [&](const char* name) { return _stricmp(command.c_str(), name) == 0; };
        if (is1("repair")) {
            if (damaged && m_repairShown == 0) {
                m_repairShown = 1;
                garage.priceShown(host, true);
                garage.price(host, x87::chop(repairCost(*car, data->catalogue)));
            }
        } else if (m_repairShown != 0) {
            m_repairShown = 0;
            garage.priceShown(host, false);
        }
        if (event == input::kSelect && menu) {
            host.sound("CLICK");
            const auto over = [&](const char* window) {
                host.request(window, SwitchMode::Push);
                setMenuHidden(host, s, true);
            };
            const auto damagedNote = [&] {
                host.message(MessageType::Continue, texts.at(kTextCarDamaged), false, nullptr);
            };
            if (is1("cust")) {
                if (damaged) {
                    damagedNote();
                } else {
                    over("custmenu");
                }
            } else if (is1("shop")) {
                over("modshop");
            } else if (is1("details")) {
                host.carStats().setGarage(1);
                over("car_stats2");
            } else if (is1("maintenance")) {
                over("car_maintain");
            } else if (is1("leave")) {
                leaveGarage(host);
            } else if (is1("scheme")) {
                over("schememenu");
            } else if (is1("testtrack")) {
                if (damaged) {
                    damagedNote();
                } else {
                    setupTestTrackSession(career, gameRandom());
                    data->careerTestTrack = 1;
                    data->careerScreen = CareerScreen::CutScene;
                    p.moneySaved = 0xFFFFFFFF;
                    host.request("garageload", SwitchMode::Replace);
                }
            } else if (is1("rolling")) {
                if (damaged) {
                    damagedNote();
                } else {
                    data->careerScreen = CareerScreen::RollingRoad;
                    host.request("garageload", SwitchMode::Replace);
                }
            } else if (is1("sell")) {
                const int32_t worth = car != nullptr ? saleWorth(*car, data->catalogue) : 0;
                host.message(MessageType::YesNo,
                             texts.at(kTextSellFor) + L" " + toWide(moneyText(worth, host.language().index)) + L"\n" +
                                 texts.at(kTextAreYouSure),
                             false, &m_sellAnswer, true);
            } else if (is1("repair") && damaged && m_repairAsked == 0) {
                const float cost = repairCost(*car, data->catalogue);
                if (cost > static_cast<float>(p.money)) {
                    host.message(MessageType::Continue, texts.at(kTextInsufficientCash), false, nullptr);
                    host.sound("NO");
                } else {
                    m_repairAsked = 1;
                    host.sound("CLICK");
                    host.message(MessageType::YesNo,
                                 texts.at(kTextRepairFor) + L" " +
                                     toWide(moneyText(x87::chop(cost), host.language().index)) + L"\n" +
                                     texts.at(kTextAreYouSure),
                                 false, &m_repairAnswer, true);
                    m_repairAnswer = -1;
                }
            }
        }
    }
    if (event == input::kBack && m_repairAsked == 0) leaveGarage(host);
    if (m_rollingAnswer == 1) {
        host.sound("CLICK");
        data->careerScreen = CareerScreen::RollingRoad;
        host.request("garageload", SwitchMode::Replace);
    }
    if (m_sellAnswer == 1) {
        host.sound("CLICK");
        garage.deal(host);
        host.request("CMSCarCollection2", SwitchMode::Replace);
    }
    if (m_repairAsked != 0) {
        if (m_repairAnswer == 1 && car != nullptr) {
            pay(career, x87::chop(repairCost(*car, data->catalogue)));
            const float cost = repairCost(*car, data->catalogue);
            car->spent = std::bit_cast<uint32_t>(std::bit_cast<float>(car->spent) + cost);
            const auto spent = static_cast<float>(static_cast<int32_t>(p.moneySpent));
            p.moneySpent = static_cast<uint32_t>(x87::chop(spent + cost));
            repairCar(*car);
            data->careerScreen = CareerScreen::CarSelRepaired;
            host.request("garageload", SwitchMode::Replace);
        } else if (m_repairAnswer == 0) {
            m_repairAsked = 0;
        }
    }
    // 0x00437967, 0x00437B69: the chassis lowered by MAXDROPHEIGHT x the car's ride (the garage's car object
    // [0x0075ED00] and the window's, the same here) each frame but the leave's.
    garage.lowerChassis(host);
}

// 0x00436B90: a Menu element's highlighted item kept; [game + 4]'s method +0x38 with 2; the views freed; the deal
// finished (0x004B5490); the garage's music off (0x00436CF0).
void CarSel::leave(GarageHost& host) {
    GameData* data = host.gameData();
    Garage& garage = host.garage();
    if (garage.view) garage.view->clear();
    garage.view.reset();
    if (data == nullptr || !data->career) return;
    dealerFinish(*data->career);
    music(host, false);
}

}  // namespace oj::frontend
