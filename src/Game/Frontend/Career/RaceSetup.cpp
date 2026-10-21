// CMSRaceSetup: the cars and drivers of a race.

#include "Game/Frontend/Career/RaceSetup.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/Garage/CarStats.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>

namespace oj::frontend {

namespace {

// A crew's leave: a crew of more than one member in a round not a cruise nor a challenge, and not a pink slip.
bool crewRace(const Career& career, const CalendarEvent& e, const EventRound& round) {
    return career.profile.memberCount != 1 && round.type != EventKind::Showoff && round.type != EventKind::Challenge &&
           e.pinkSlip == 0;
}

}  // namespace

// The first menu item of the value enabled or not (0x005AFA60 and the item's +0x00).
void enableCarItems(ui::Screen& s, float value, bool enabled) {
    for (size_t i = 0; i < s.menuSize("carmenu"); ++i) {
        if (s.menuItemValue("carmenu", i) == value) s.setMenuItemEnabledAt("carmenu", i, enabled);
    }
}

// 0x004D7470: carmenu emptied (its +0x100 set) and an item for each car listed for the meeting (the profile's +0x514,
// count +0x5F0), its value the garage index: the model's display name, " (D)" when damaged, " (P)" with more than
// three prototype parts (0x004A8F50); then "GO" (value 999) disabled and the cars' items enabled.
void RaceSetupWindow::cars(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Career& career = *data->career;
    const CareerProfile& p = career.profile;
    s.clearMenu("carmenu");
    const int32_t listed = p.raceChoiceCount;
    for (int32_t k = 0; k < listed && k < 55; ++k) {
        const int32_t index = p.raceChoices[static_cast<size_t>(k)];
        if (index < 0 || static_cast<size_t>(index) >= career.garage.size()) continue;
        const Car& car = career.garage[static_cast<size_t>(index)];
        std::wstring text = car.def != nullptr ? widenSigned(car.def->displayName) : std::wstring();
        if (gridDamaged(car) || partsWorn(car)) text += L" (D)";
        if (heavilyTuned(car, data->catalogue)) text += L" (P)";
        s.addMenuItem("carmenu", text, static_cast<float>(index));
    }
    s.addMenuItem("carmenu", texts.at(kTextGo), kItemGo);
    for (size_t i = 0; i < s.menuSize("carmenu"); ++i) {
        if (s.menuItemValue("carmenu", i) == kItemGo) {
            s.setMenuItemEnabledAt("carmenu", i, false);
            break;
        }
    }
    for (int32_t k = 0; k < listed && static_cast<size_t>(k) < s.menuSize("carmenu"); ++k) {
        s.setMenuItemEnabledAt("carmenu", static_cast<size_t>(k), true);
    }
}

// 0x004D8990: selection1 .. 3 hidden; the first free slot of the team shows its selection<n>.
void RaceSetupWindow::markers(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    Career& career = *data->career;
    for (int k = 1; k <= 3; ++k) s.setHidden(std::format("selection{}", k), true);
    const CalendarEvent* e = currentEvent(career.calendar);
    if (e == nullptr) return;
    const int team = roundOf(career, *e)->teamSize;
    for (int k = 0; k < team && k < 3; ++k) {
        if (m_cars[static_cast<size_t>(k)] == -1) {
            s.setHidden(std::format("selection{}", k + 1), false);
            return;
        }
    }
}

// 0x004D7660: with GO highlighted nothing; else bhp and drive shown, STATS shown, bhp the highlighted car's rating
// (cut, thousands marked) and drive its drive's line (1830 + 0x0048CE20).
void RaceSetupWindow::stats(CareerHost& host, ui::Screen& s, bool go) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (go) return;
    Career& career = *data->career;
    s.setHidden("bhp", false);
    s.setHidden("drive", false);
    s.showContaining("STATS", false);
    const auto index = static_cast<size_t>(x87::chop(s.menuItemValue("carmenu", s.selectedMenuItem("carmenu"))));
    if (index >= career.garage.size()) return;
    const Car& car = career.garage[index];
    s.setText("bhp", toWide(groupThousands(x87::chop(std::bit_cast<float>(car.rating)), host.language().index)));
    s.setText("drive", texts.at(kTextDrives + static_cast<size_t>(driveOf(car, data->tables))));
}

// 0x004D8410: the car taken into the team: its items disabled; in the first free slot (+0x38, the count +0x44 up)
// Car<n>icon and Car<n> shown with the model's display name; with every slot taken the last slot's car is given back
// (its items enabled) and the car takes that slot. The markers again (0x004D8990).
void RaceSetupWindow::take(CareerHost& host, ui::Screen& s, int32_t index) {
    GameData* data = host.gameData();
    Career& career = *data->career;
    const CalendarEvent* e = currentEvent(career.calendar);
    if (e == nullptr) return;
    const int team = roundOf(career, *e)->teamSize;
    enableCarItems(s, static_cast<float>(index), false);
    const auto name = [&](int32_t car) {
        const auto k = static_cast<size_t>(car);
        return k < career.garage.size() && career.garage[k].def != nullptr
                   ? widenSigned(career.garage[k].def->displayName) : std::wstring();
    };
    for (int k = 0; k < team && k < 3; ++k) {
        if (m_cars[static_cast<size_t>(k)] != -1) continue;
        m_cars[static_cast<size_t>(k)] = index;
        ++m_chosen;
        s.setHidden(std::format("Car{}icon", k + 1), false);
        s.setHidden(std::format("Car{}", k + 1), false);
        s.setText(std::format("Car{}", k + 1), name(index));
        markers(host, s);
        return;
    }
    // Stock problem: a team size of 0 reads before the cars (FADEBOX's pointer) and one past 3 after them (the count);
    // here within the three.
    const auto last = static_cast<size_t>(std::clamp(team, 1, 3) - 1);
    enableCarItems(s, static_cast<float>(m_cars[last]), true);
    m_cars[last] = index;
    s.setHidden(std::format("Car{}icon", last + 1), false);
    s.setHidden(std::format("Car{}", last + 1), false);
    s.setText(std::format("Car{}", last + 1), name(index));
    markers(host, s);
}

// 0x004D8270: the car given back: its slot freed (the count down), Car<n>icon and Car<n> hidden and emptied; its items
// enabled; the markers again.
void RaceSetupWindow::giveBack(CareerHost& host, ui::Screen& s, int32_t index) {
    for (size_t k = 0; k < 3; ++k) {
        if (m_cars[k] != index) continue;
        m_cars[k] = -1;
        --m_chosen;
        s.setHidden(std::format("Car{}icon", k + 1), true);
        s.setHidden(std::format("Car{}", k + 1), true);
        s.setText(std::format("Car{}", k + 1), L"");
    }
    enableCarItems(s, static_cast<float>(index), true);
    markers(host, s);
}

// 0x004D8810: GO: the profile's race cars (+0x600, count +0x60C) the team's when it is complete; a round the player's
// crew is not in yet takes it (+0x0D): a race or a sprint with room (crews x team under 6, a sprint 4) one crew more
// (+0x16) and the player's drivers (+0x18), else the first crew 1 .. 8 racing gives its place. One driver (+0x18 1):
// the race set up (0x004B8570) and started (0x004A3CC0, "loading") and the bet's share (0x004BC2E0); else
// CMSGridWalk. An incomplete team: "no".
void RaceSetupWindow::go(CareerHost& host) {
    GameData* data = host.gameData();
    Career& career = *data->career;
    CareerProfile& p = career.profile;
    CalendarEvent* e = currentEvent(career.calendar);
    if (e == nullptr) return;
    auto& round = const_cast<EventRound&>(*roundOf(career, *e));
    p.raceCarCount = 0;
    if (m_chosen != round.teamSize) {
        host.sound("no");
        return;
    }
    for (size_t k = 0; k < 3; ++k) {
        if (m_cars[k] == -1) continue;
        p.raceCars[static_cast<size_t>(p.raceCarCount)] = m_cars[k];
        ++p.raceCarCount;
    }
    if (round.crews[0] == 0) {
        bool room = false;
        if (round.type != EventKind::Showoff && round.type != EventKind::Challenge) {
            const int drivers = round.teamSize * round.opponents;
            room = round.type == EventKind::Sprint ? drivers < 4 : drivers < 6;
        }
        if (room) {
            ++round.opponents;
            round.drivers = static_cast<int8_t>(round.drivers + p.raceCarCount);
        } else {
            for (size_t c = 1; c < 9; ++c) {
                if (round.crews[c] != 0) {
                    round.crews[c] = 0;
                    break;
                }
            }
        }
        round.crews[0] = 1;
    }
    if (round.drivers == 1) {
        data->setupRace(gameRandom());
        data->startRace(gameRandom(), static_cast<int32_t>(host.settingsValue(save::Settings::kAutomatic)));
        host.request("loading", SwitchMode::Replace);
        updateBetShare(career);
        return;
    }
    host.request("CMSGridWalk", SwitchMode::Replace);
}

// 0x004D69F0: carmenu, bhp and drive kept (+0x24 .. +0x2C), bhp and drive emptied; +0x0C .. +0x14 0; the car menu
// (0x004D7470); Driver1 .. 3 emptied, Car1 .. 3 hidden, emptied and over several lines; back_text "BACK" for a crew's
// race (it goes back to CMSCrewRaceSetup), else "LEAVE EVENT"; Crewmate1 .. 3, Border1 .. 3 and CrewBG1 .. 3 hidden;
// for each driver of the team Driver<n> the crewmate's short name of its member slot (the profile's +0x5F4), and
// Crewmate<n> (its ui_crewmember_<number>), Border<n> and CrewBG<n> shown; the team's slots free (+0x38 .. +0x40 -1,
// +0x44 0) and Car1Icon .. 3 hidden; +0x1C -1; the markers (0x004D8990); Cash and Date; +0x18 and +0x20 -1; the car
// view (+0x30, made in place as 0x0045A2C0: CAR3D, LOGO, ORIGIN, the zoom 0.7, the mirror image, the car's folder kept
// and the shadow on); FADEBOX (+0x34). A cruise asks once (the profile's +0x634) "VIEW SHOWOFF TUTORIAL?" into +0x20.
void RaceSetupWindow::enter(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    CareerProfile& p = career.profile;
    CalendarEvent* e = currentEvent(career.calendar);
    if (e == nullptr) return;
    const EventRound& round = *roundOf(career, *e);
    if (const int el = s.find("drive"); el >= 0) s.clearText(el);
    if (const int el = s.find("bhp"); el >= 0) s.clearText(el);
    m_respectAnswer = 0;
    m_leaving = 0;
    m_leaveAnswer = 0;
    cars(host, s);
    for (int k = 1; k <= 3; ++k) {
        s.setText(std::format("Driver{}", k), L"");
        const std::string car = std::format("Car{}", k);
        s.setHidden(car, true);
        s.setText(car, L"");
        s.setMultiline(car, true);
    }
    s.setText("back_text", texts.at(crewRace(career, *e, round) ? kTextBack : kTextLeaveEventItem));
    for (int k = 1; k <= 3; ++k) {
        s.setHidden(std::format("Crewmate{}", k), true);
        s.setHidden(std::format("Border{}", k), true);
        s.setHidden(std::format("CrewBG{}", k), true);
    }
    for (int k = 1; k <= round.teamSize && k <= 3; ++k) {
        const int32_t slot = p.raceMembers[static_cast<size_t>(k - 1)];
        // Stock problem: a member slot without a crewmate reads through its old pointer; nothing shown then.
        if (slot < 0 || slot >= 4) continue;
        const auto number = static_cast<size_t>(career.members[static_cast<size_t>(slot)]);
        if (number >= career.crewmates.size()) continue;
        const Crewmate& mate = career.crewmates[number];
        s.setText(std::format("Driver{}", k),
                  widenSigned(std::string_view(mate.shortName.data(), mate.shortName.size())));
        s.setHidden(std::format("Crewmate{}", k), false);
        s.setTexture(std::format("Crewmate{}", k), std::format("ui_crewmember_{:02}", mate.number));
        s.setHidden(std::format("Border{}", k), false);
        s.setHidden(std::format("CrewBG{}", k), false);
    }
    m_chosen = 0;
    for (size_t k = 0; k < 3; ++k) {
        m_cars[k] = -1;
        s.setHidden(std::format("Car{}Icon", k + 1), true);
    }
    m_shownCar = -1;
    markers(host, s);
    careerCashTexts(s, career, host.language().index, texts);
    m_repairAnswer = -1;
    m_tutorialAnswer = -1;
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
    if (p.showoffTutorialShown == 0 && e->type == EventKind::Showoff) {
        host.message(MessageType::YesNo, texts.at(kTextShowoffTutorial), false, &m_tutorialAnswer, true);
        p.showoffTutorialShown = 1;
    }
}

// 0x004D7840: the car view's frame (0x00459EE0) and FADEBOX's alpha 1 - its +0x4C; Cash and Date.
// Leaving a special event answered "yes" (+0x0C): the crew's respect message of kind 2 with the event's +0x02, +0x10
// set; then (+0x10) or another answered "yes" (+0x14): the event's end, the history's step, CLICK and CMSCalendar.
// BACK: a crew's race goes back to CMSCrewRaceSetup (CLICK); else (CLICK) the question as in CMSAtMeeting. The car
// highlighted (carmenu) other than the one shown (+0x1C) is shown (0x0045A080: the garage car, 2.64, 1, fading; not
// GO); without the view busy the stats (0x004D7660), else bhp and drive hidden. The tutorial answered "yes" (+0x20):
// CMSRaceSetup to come back to
// ([0x0072158C]) and vid_tutorial. The repair answered "yes" (+0x18): its cost paid (0x004B8200) and noted on the car
// (0x0048F270: its +0xA8 and the profile's +0x4E8), Cash and Date, the car repaired (0x0048EB20), +0x18 -1, its item's
// text without " (D)", the car taken (0x004D8410) and, with the team complete or every car taken, GO highlighted and
// enabled. SELECT: GO (0x004D8810); else (CLICK) a car in the team given back (GO disabled); a damaged car asks for
// its repair ("Repair this car for" and the cost; "INSUFFICIENT CASH" over the money, else "ARE YOU SURE?" into
// +0x18); else the car taken and, filling the team or the cars, GO highlighted and enabled. PGDN on a car with the view
// not busy (0x0045A880): CLICK, [0x00721F30] 1, the car ([0x00728C60]) and car_stats2purple over the window.
void RaceSetupWindow::update(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    CareerProfile& p = career.profile;
    CalendarEvent* e = currentEvent(career.calendar);
    if (e == nullptr) return;
    const EventRound& round = *roundOf(career, *e);
    if (m_view) {
        m_view->frame(host.frameTime(), gameRandom());
        if (const int fade = s.find("FADEBOX"); fade >= 0) s.setAlpha(fade, 1.0f - m_view->shown());
    }
    careerCashTexts(s, career, host.language().index, texts);
    if (m_respectAnswer == 1) {
        crewRespect(host, career, e->crew, RespectReason::BackedOut, e->hostedByPlayer, host.language().index, texts);
        m_respectAnswer = 0;
        m_leaving = 1;
        return;
    }
    if (m_leaving != 0 || m_leaveAnswer == 1) {
        endEvent(career, *e, data->tables);
        recordHistory(career);
        host.sound("CLICK");
        host.request("CMSCalendar", SwitchMode::Replace);
        return;
    }
    if (event == input::kBack) {
        if (crewRace(career, *e, round)) {
            host.sound("CLICK");
            host.request("CMSCrewRaceSetup", SwitchMode::Replace);
            return;
        }
        host.sound("CLICK");
        if (!specialEvent(*e)) {
            host.message(MessageType::YesNo, texts.at(kTextLeaveEvent) + L"\n" + texts.at(kTextEventClosed), false,
                         &m_leaveAnswer, true);
        } else {
            host.message(MessageType::YesNo, texts.at(kTextLeaveSpecial) + L"\n" + texts.at(kTextEventClosed), false,
                         &m_respectAnswer, true);
        }
    }
    const size_t items = s.menuSize("CarMenu");
    const size_t item = s.selectedMenuItem("CarMenu");
    const float value = items != 0 ? s.menuItemValue("CarMenu", item) : 0.0f;
    if (items != 0 && static_cast<float>(m_shownCar) != value) {
        m_shownCar = static_cast<int32_t>(std::nearbyint(value));
        const auto index = static_cast<uint32_t>(m_shownCar);
        if (value != kItemGo && m_view && index < career.garage.size()) {
            m_view->show(career.garage[index], &career.garageTail, index, 2.64f, 1.0f, true, host.viewLoading());
        }
    }
    if (items != 0) {
        // Without the view busy (its +0x40 and +0x3C) the stats and bhp and drive shown, also with GO highlighted;
        // else bhp and drive hidden.
        const bool busy = m_view && m_view->busy();
        if (!busy) stats(host, s, value == kItemGo);
        s.setHidden("bhp", busy);
        s.setHidden("drive", busy);
    }
    if (m_tutorialAnswer == 1) {
        host.shared().tutorialReturn = "CMSRaceSetup";
        host.request("vid_tutorial", SwitchMode::Replace);
        return;
    }
    if (m_repairAnswer == 1 && items != 0) {
        const auto index = x87::chop(value);
        Car& car = career.garage[static_cast<size_t>(index)];
        pay(career, x87::chop(repairCost(car, data->catalogue)));
        const float cost = repairCost(car, data->catalogue);
        car.spent = std::bit_cast<uint32_t>(std::bit_cast<float>(car.spent) + cost);
        const auto spent = static_cast<float>(static_cast<int32_t>(p.moneySpent));
        p.moneySpent = static_cast<uint32_t>(x87::chop(spent + cost));
        careerCashTexts(s, career, host.language().index, texts);
        repairCar(car);
        m_repairAnswer = -1;
        std::wstring text = car.def != nullptr ? widenSigned(car.def->displayName) : std::wstring();
        if (heavilyTuned(car, data->catalogue)) text += L" (P)";
        s.setMenuItemText("CarMenu", item, text);
        take(host, s, index);
        const int32_t listed = p.raceChoiceCount;
        if (m_chosen == round.teamSize || m_chosen == listed) {
            s.selectMenuItem("CarMenu", s.menuSize("CarMenu") - 1, true);
            enableCarItems(s, kItemGo, true);
        }
    }
    if (event == input::kSelect) {
        if (items == 0) return;
        if (x87::chop(value) == 999) {
            go(host);
            return;
        }
        host.sound("CLICK");
        const auto index = static_cast<int32_t>(std::nearbyint(value));
        if (std::find(m_cars.begin(), m_cars.end(), index) != m_cars.end()) {
            giveBack(host, s, index);
            enableCarItems(s, kItemGo, false);
            return;
        }
        const Car& car = career.garage[static_cast<size_t>(index)];
        if (!gridDamaged(car) && !partsWorn(car)) {
            const int32_t listed = p.raceChoiceCount;
            const bool last = m_chosen == round.teamSize - 1 || m_chosen == listed - 1;
            take(host, s, index);
            if (last) {
                s.selectMenuItem("CarMenu", s.menuSize("CarMenu") - 1, true);
                enableCarItems(s, kItemGo, true);
            }
            return;
        }
        const float cost = repairCost(car, data->catalogue);
        const std::wstring text = texts.at(kTextRepairFor) + L" " +
                                  toWide(moneyText(x87::chop(cost), host.language().index)) + L"\n";
        if (cost > static_cast<float>(p.money)) {
            host.message(MessageType::Continue, text + L"\n" + texts.at(kTextInsufficientCash), false, nullptr);
        } else {
            host.message(MessageType::YesNo, text + texts.at(kTextAreYouSure), false, &m_repairAnswer, true);
            m_repairAnswer = -1;
        }
        return;
    }
    if (event == input::kSelect2 && items != 0 && !(m_view && m_view->busy())) {
        host.sound("CLICK");
        if (x87::chop(value) == 999) return;
        host.carStatsPurple().setGarage(1);
        const auto index = static_cast<size_t>(x87::chop(value));
        g_statsCar = index < career.garage.size() ? &career.garage[index] : nullptr;
        host.request("car_stats2purple", SwitchMode::Push);
    }
}

// 0x004D77E0: the state keeps a Menu element's highlighted item (its +0x13C, when the window has one); the car's view
// freed (0x0045AE90, 0x0045A340).
void RaceSetupWindow::leave() {
    if (m_view) m_view->clear();
    m_view.reset();
}

}  // namespace oj::frontend
