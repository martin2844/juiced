// CMSOnlineRaceSetup: the online career's race's cars.

#include "Game/Frontend/Network/OnlineRaceSetup.h"

#include "Core/Data/GameTexts.h"
#include "Core/Network/Matchmaking.h"
#include "Game/Frontend/Career/RaceSetup.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>

namespace oj::frontend {

namespace {

// 0x0049A850: each byte widened with its sign. Stock problem: a byte from 0x80 up (a description in the ANSI code
// page) becomes 0xFF80 .. 0xFFFF, not its character.
std::wstring widenSigned(const std::string& text) {
    std::wstring wide;
    for (char c : text) wide.push_back(static_cast<wchar_t>(static_cast<int16_t>(static_cast<int8_t>(c))));
    return wide;
}

std::wstring wide(std::u16string_view text) {
    return {text.begin(), text.end()};
}

}  // namespace

// 0x004D4E50: CarMenu emptied (its +0x100 set) and an item for each garage car whose class (0x0048E9E0) lies within the
// session's upper .. lower class and that meets its criteria (1 its territory +8, 2 its make +4, 3 its model +0), its
// value the garage index: the display name, " (D)" damaged, " (P)" with more than three prototype parts; after each
// item bhp the car's rating with its thousands marked (0x004A3AF0, so the last car's stays); then "GO" (999); the
// cars' items enabled.
void OnlineRaceSetup::cars(NetworkHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Career& career = *data->career;
    const net::CareerSession& session = net::onlineHosted().career;
    s.clearMenu("CarMenu");
    size_t listed = 0;
    for (size_t index = 0; index < career.garage.size(); ++index) {
        const Car& car = career.garage[index];
        const int carClassOf = oj::carClass(car);
        if (carClassOf < session.upper || carClassOf > session.lower || car.def == nullptr) continue;
        if (session.criteriaType == 1 && car.def->region != session.criteria) continue;
        if (session.criteriaType == 2 && car.def->make != session.criteria) continue;
        if (session.criteriaType == 3 && car.def->id != session.criteria) continue;
        std::wstring text = widenSigned(car.def->displayName);
        if (gridDamaged(car) || partsWorn(car)) text += L" (D)";
        if (heavilyTuned(car, data->catalogue)) text += L" (P)";
        s.addMenuItem("CarMenu", text, static_cast<float>(index));
        s.setText("bhp", toWide(groupThousands(x87::chop(std::bit_cast<float>(car.rating)), host.language().index)));
        ++listed;
    }
    s.addMenuItem("CarMenu", texts.at(kTextGo), kItemGo);
    for (size_t i = 0; i < listed && i < s.menuSize("CarMenu"); ++i) s.setMenuItemEnabledAt("CarMenu", i, true);
}

// 0x004D4720: the elements carmenu, bhp and drive kept; the car menu (0x004D4E50); Driver1 .. 3 emptied, Car1 .. 3
// hidden, emptied and over several lines, the slots -1; Crewmate1 .. 3 and Border1 .. 3 hidden; for each driver of
// the team Driver<n> the short name of its member slot's crewmate (the profile's +0x5F4, +0x500), Crewmate<n> its
// ui_crewmember_<number> and Border<n> shown; +0x0C the profile's first car listed (+0x514); the markers (0x004D5F90);
// Cash and Date (0x004A4130); the car view (+0x20: CAR3D, LOGO, ORIGIN, the zoom 0.7, the mirror image, the car's
// folder kept and the shadow on) and FADEBOX (+0x24); +0x38 0; +0x10 -1.
void OnlineRaceSetup::enter(NetworkHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    const CareerProfile& p = career.profile;
    cars(host, s);
    for (int k = 1; k <= 3; ++k) {
        s.setText(std::format("Driver{}", k), L"");
        const std::string car = std::format("Car{}", k);
        s.setHidden(car, true);
        s.setText(car, L"");
        s.setMultiline(car, true);
        m_cars[static_cast<size_t>(k - 1)] = -1;
    }
    for (int k = 1; k <= 3; ++k) {
        s.setHidden(std::format("Crewmate{}", k), true);
        s.setHidden(std::format("Border{}", k), true);
    }
    for (int k = 1; k <= onlineTeam() && k <= 3; ++k) {
        const int32_t slot = p.raceMembers[static_cast<size_t>(k - 1)];
        if (slot < 0 || slot >= 4) continue;
        const auto number = static_cast<size_t>(career.members[static_cast<size_t>(slot)]);
        if (number >= career.crewmates.size()) continue;
        const Crewmate& mate = career.crewmates[number];
        s.setText(std::format("Driver{}", k),
                  oj::widenSigned(std::string_view(mate.shortName.data(), mate.shortName.size())));
        s.setHidden(std::format("Crewmate{}", k), false);
        s.setTexture(std::format("Crewmate{}", k), std::format("ui_crewmember_{:02}", mate.number));
        s.setHidden(std::format("Border{}", k), false);
    }
    m_shown = p.raceChoices[0];
    markers(s);
    careerCashTexts(s, career, host.language().index, texts);
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
    m_chosen = 0;
    m_shownValue = -1.0f;
    m_repairAnswer = -1;
}

// 0x004D5F90: selection1 .. 3 hidden; the first free slot of the team shows its selection<n>.
void OnlineRaceSetup::markers(ui::Screen& s) {
    for (int k = 1; k <= 3; ++k) s.setHidden(std::format("selection{}", k), true);
    for (int k = 0; k < onlineTeam() && k < 3; ++k) {
        if (m_cars[static_cast<size_t>(k)] == -1) {
            s.setHidden(std::format("selection{}", k + 1), false);
            return;
        }
    }
}

// 0x004D5100: the car's stats (999: none): bhp its rating with its thousands marked (0x004A3AF0), drive its drive's
// text.
void OnlineRaceSetup::stats(NetworkHost& host, ui::Screen& s, int32_t index) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (index == 999) return;
    Career& career = *data->career;
    if (index < 0 || static_cast<size_t>(index) >= career.garage.size()) return;
    const Car& car = career.garage[static_cast<size_t>(index)];
    s.setHidden("bhp", false);
    s.setText("bhp", toWide(groupThousands(x87::chop(std::bit_cast<float>(car.rating)), host.language().index)));
    s.setText("drive", texts.at(kTextDrives + static_cast<size_t>(driveOf(car, data->tables))));
}

// 0x004D5980: the car's items disabled; in the first free slot (+0x38 up) Car<n> shown with its display name (" (P)"
// heavily tuned); with every slot taken the last slot's car given back (its items enabled) and the car takes it. The
// markers (0x004D5F90) and the car sent to the session (0x004D5D90: 0x004EA2E0 with the slot).
void OnlineRaceSetup::take(NetworkHost& host, ui::Screen& s, int32_t index) {
    GameData* data = host.gameData();
    Career& career = *data->career;
    if (index < 0 || static_cast<size_t>(index) >= career.garage.size()) return;
    enableCarItems(s, static_cast<float>(index), false);
    const Car& car = career.garage[static_cast<size_t>(index)];
    std::wstring text = car.def != nullptr ? widenSigned(car.def->displayName) : std::wstring();
    if (heavilyTuned(car, data->catalogue)) text += L" (P)";
    const int team = std::clamp(onlineTeam(), 0, 3);
    int slot = -1;
    for (int k = 0; k < team; ++k) {
        if (m_cars[static_cast<size_t>(k)] == -1) {
            slot = k;
            ++m_chosen;
            break;
        }
    }
    if (slot == -1) {
        slot = team - 1;
        if (slot < 0) return;
        enableCarItems(s, static_cast<float>(m_cars[static_cast<size_t>(slot)]), true);
    }
    m_cars[static_cast<size_t>(slot)] = index;
    const std::string element = std::format("Car{}", slot + 1);
    s.setHidden(element, false);
    s.setText(element, text);
    markers(s);
    if (net::NetGame* net = net::netGame(); net != nullptr && car.def != nullptr) {
        net->session.sendCar(net::CarConfig::of(car, car.def->name), slot);
    }
}

// 0x004D5820: the car given back: its slot -1 (+0x38 down) and Car<n> emptied; its items enabled; the markers.
void OnlineRaceSetup::giveBack(ui::Screen& s, int32_t index) {
    for (int k = 0; k < 3; ++k) {
        if (m_cars[static_cast<size_t>(k)] != index) continue;
        m_cars[static_cast<size_t>(k)] = -1;
        --m_chosen;
        s.setText(std::format("Car{}", k + 1), L"");
    }
    enableCarItems(s, static_cast<float>(index), true);
    markers(s);
}

// 0x004D52D0. The car view's frame and FADEBOX's alpha 1 - its +0x4C. Unless the network's checks (0x0046E0F0 with
// the stage 8) left: the highlighted car other than the one shown (+0x10) shown (0x0045A080: 2.64, 1, fading; not GO)
// with its stats (0x004D5100); the repair answered yes (+0x28): its cost paid, the money not below 0, noted on the car
// (0x0048F270), Cash and Date, the car repaired (0x0048EB20), its item's text without " (D)" and the car taken
// (0x004D5980). SELECT (CLICK): GO (0x004D5ED0); a car in the team given back (0x004D5820); a damaged car's repair
// "Repair this car for" its cost: over the money "INSUFFICIENT CASH" (type 1), else "ARE YOU SURE?" (type 3 into +0x28);
// else the car taken and, filling the team, GO highlighted.
void OnlineRaceSetup::update(NetworkHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    CareerProfile& p = career.profile;
    if (m_view) {
        m_view->frame(host.frameTime(), gameRandom());
        if (const int fade = s.find("FADEBOX"); fade >= 0) s.setAlpha(fade, 1.0f - m_view->shown());
    }
    if (net::netGame() != nullptr && host.netKeys(event, net::SessionStage::Cars, true)) return;
    const size_t items = s.menuSize("CarMenu");
    const size_t item = s.selectedMenuItem("CarMenu");
    const float value = items != 0 ? s.menuItemValue("CarMenu", item) : 0.0f;
    if (items != 0 && value != m_shownValue) {
        m_shown = x87::chop(value);
        m_shownValue = value;
        const auto index = static_cast<uint32_t>(m_shown);
        if (value != kItemGo && m_view && index < career.garage.size()) {
            m_view->show(career.garage[index], &career.garageTail, index, 2.64f, 1.0f, true, host.viewLoading());
        }
        stats(host, s, m_shown);
    }
    if (m_repairAnswer == 1 && items != 0) {
        const auto index = static_cast<size_t>(x87::chop(value));
        if (index < career.garage.size()) {
            Car& car = career.garage[index];
            pay(career, x87::chop(repairCost(car, data->catalogue)));
            if (p.money < 0) p.money = 0;
            const float cost = repairCost(car, data->catalogue);
            car.spent = std::bit_cast<uint32_t>(std::bit_cast<float>(car.spent) + cost);
            const auto spent = static_cast<float>(static_cast<int32_t>(p.moneySpent));
            p.moneySpent = static_cast<uint32_t>(x87::chop(spent + cost));
            careerCashTexts(s, career, host.language().index, texts);
            repairCar(car);
            std::wstring text = car.def != nullptr ? widenSigned(car.def->displayName) : std::wstring();
            if (heavilyTuned(car, data->catalogue)) text += L" (P)";
            s.setMenuItemText("CarMenu", item, text);
            take(host, s, static_cast<int32_t>(index));
        }
        m_repairAnswer = -1;
    }
    if (event != input::kSelect || items == 0) return;
    host.sound("CLICK", 1.0f, 0xF);
    if (x87::chop(value) == 999) {
        go(host);
        return;
    }
    const auto index = x87::chop(value);
    if (std::find(m_cars.begin(), m_cars.end(), index) != m_cars.end()) {
        giveBack(s, index);
        return;
    }
    if (index < 0 || static_cast<size_t>(index) >= career.garage.size()) return;
    const Car& car = career.garage[static_cast<size_t>(index)];
    if (gridDamaged(car) || partsWorn(car)) {
        const float cost = repairCost(car, data->catalogue);
        const std::wstring text = texts.at(kTextRepairFor) + L" " +
                                  toWide(moneyText(x87::chop(cost), host.language().index)) + L"\n";
        if (cost <= static_cast<float>(p.money)) {
            host.message(MessageType::YesNo, text + texts.at(kTextAreYouSure), false, &m_repairAnswer, true);
        } else {
            host.message(MessageType::Continue, text + L"\n" + texts.at(kTextInsufficientCash), false, nullptr);
        }
        return;
    }
    const bool last = m_chosen == onlineTeam() - 1;
    take(host, s, index);
    if (last) s.selectMenuItem("CarMenu", s.menuSize("CarMenu") - 1, true);
}

// 0x004D5ED0: GO: the profile's race cars (+0x600, count +0x60C) the team's when it is complete; careerload and the
// career's screen (+0xA4) 8.
void OnlineRaceSetup::go(NetworkHost& host) {
    GameData* data = host.gameData();
    CareerProfile& p = data->career->profile;
    p.raceCarCount = 0;
    if (m_chosen != onlineTeam()) return;
    for (size_t k = 0; k < 3; ++k) {
        if (m_cars[k] == -1) continue;
        p.raceCars[static_cast<size_t>(p.raceCarCount)] = m_cars[k];
        ++p.raceCarCount;
    }
    host.request("careerload", SwitchMode::Replace);
    data->careerScreen = CareerScreen::OnlineGridWalk;
}

// 0x004D5270: "Menu"'s highlight kept (its +0x13C, a menu); the car view freed (0x0045AE90, 0x0045A340).
void OnlineRaceSetup::leave() {
    if (m_view) m_view->clear();
    m_view.reset();
}

}  // namespace oj::frontend
