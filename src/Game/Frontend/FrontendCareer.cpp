// The front end's side of the career: the help pages and the career's next window after a race (what the
// career's windows ask through CareerHost).

#include "Game/Frontend/Frontend.h"
#include "Game/Frontend/FrontendInternal.h"

#include "Core/Data/GameTexts.h"
#include <iterator>
#include <algorithm>
#include <cmath>

namespace oj {

using namespace frontend;

namespace {

// A help page (the table at 0x00715D48, 0x4C bytes each): +0x04 the window, +0x24 the note's line (none: +0x28 the
// window opened over it), +0x48 shown only once a crew of 1 .. 8 has reached its fourth step (+0x60).
struct HelpPage {
    const char* window;
    size_t line;
    const char* over;
    bool hostRights;
};

constexpr HelpPage kHelpPages[] = {
    {"CMSCalendar", 0, "CMSCalendarHelp", false}, {"CMSCarMenu", 628, "", false},
    {"CMSRespectOverview", 629, "", false},       {"CMSCrew", 630, "", false},
    {"CMSMobilePhone", 631, "", false},           {"CMSGridwalk", 632, "", false},
    {"CMSRaceSetup", 633, "", false},             {"CMSCrewRaceSetup", 634, "", false},
    {"CMSCalendar", 635, "", true},               {"CMSAtMeeting", 637, "", false},
    {"instant_selcar", 638, "", false},           {"tuningmenu", 1549, "", false},
    {"stylingmenu", 1549, "", false},
};

}  // namespace

// 0x004850E0: the window's first help page not shown yet (any with `force`) whose condition holds: its note (type 1)
// or the window over this one (mode 5); the page is then shown (its +0x00).
void Frontend::helpPage(const std::string& window, bool force) {
    if (m_data == nullptr) return;
    std::vector<uint32_t>& shown = m_data->help.shown;
    if (shown.size() < std::size(kHelpPages)) shown.resize(std::size(kHelpPages), 0);
    for (size_t i = 0; i < std::size(kHelpPages); ++i) {
        const HelpPage& page = kHelpPages[i];
        if (shown[i] != 0 && !force) continue;
        if (_stricmp(window.c_str(), page.window) != 0) continue;
        if (page.hostRights) {
            bool any = false;
            if (m_data->career) {
                for (size_t c = 1; c < 9; ++c) any = any || m_data->career->crews[c].stage[3] != 0;
            }
            if (!any) continue;
        }
        if (page.line != 0) {
            message(ui::MessageType::Continue, m_gameTexts.at(page.line), false, nullptr);
        } else {
            request(page.over, SwitchMode::Push);
        }
        shown[i] = 1;
        return;
    }
}

// The first event's dealer (0x004BAEE0 with the profile's +0x00, also characterPostRace's SELECT).
void Frontend::firstEventDealer() {
    Career& career = *m_data->career;
    Dealer& dealer = career.dealer;
    career.profile.moneySaved = static_cast<uint32_t>(career.profile.money);
    dealer.mode = DealerMode::Offers;
    dealer.listCount = 0;
    dealer.deal = 0;
    dealer.chosen = 0;
    dealerOffersList(dealer);
    dealer.listCars = dealer.showroom;
    request("CMSCarDealership", SwitchMode::Replace);
}

// 0x004BAEE0: CMSMeetingSummary while the career's +0x38 list has entries, CMSRespectChanges while the +0x3C list
// has; else the history's step (0x004BD8A0); with the profile's +0x00 the profile's +0x4E4
// the money, the dealer's +0x00 1, +0x08, +0x27D4, +0x27DC 0, its offers listed (0x004B4620), +0x27D8 its +0x27E8,
// and CMSCarDealership; a first time (the profile's +0x4E0 0, then set) CMSMainMenu; with the race cars chosen (or
// the profile's +0x618) the dealer's +0x00, +0x08, +0x27D4, +0x27DC cleared, the garage listed (0x004B4440, +0x27D8
// the garage) and the player's race car chosen (0x004BB150, 0x004B52F0), its +0x0C 0, the profile's +0x4E4 the money,
// garageload (the career's +0xA4 0); else CMSMainMenu.
void Frontend::afterRaceResults() {
    Career& career = *m_data->career;
    CareerProfile& profile = career.profile;
    Dealer& dealer = career.dealer;
    if (career.meeting.count != 0) {
        request("CMSMeetingSummary", SwitchMode::Replace);
        return;
    }
    if (career.respectChanges.count != 0) {
        request("CMSRespectChanges", SwitchMode::Replace);
        return;
    }
    recordHistory(career);
    if (profile.introduction != 0) {
        firstEventDealer();
    } else if (profile.raceCarKept == 0) {
        profile.raceCarKept = 1;
        request("CMSMainMenu", SwitchMode::Replace);
    } else if (profile.raceCarsChosen != 0 || profile.racing != 0) {
        dealer.mode = DealerMode::Garage;
        dealer.listCount = 0;
        dealer.deal = 0;
        dealer.chosen = 0;
        dealerGarageList(career, m_data->tables, m_data->catalogue);
        dealerChoose(career, playerRaceCar(career.session));
        dealer.fromStats = 0;
        profile.moneySaved = static_cast<uint32_t>(profile.money);
        request("garageload", SwitchMode::Replace);
        m_data->careerScreen = CareerScreen::CarSel;  // 0x004BB132: careerload after the garage's load goes to carsel
    } else {
        request("CMSMainMenu", SwitchMode::Replace);
    }
}

// A crew's respect change over the window (CMSRespectMsg, its record set up first).
void Frontend::respectMessage(int crew, float respect, float change, const std::wstring& comment) {
    m_respect.open(*this, crew, respect, change, comment);
}

// 0x004C27A0: going to the meeting of the event chosen (the profile's +0x610): both message lists emptied, the days
// passed (0x004A6530) until the event's day, the garage cars the player may race in it listed in the profile's
// +0x514 (count +0x5F0; for an attend only reason every car, else those of its territory, make, model, prototype
// parts and class or any class 8) by their rating, the greatest first; cmsAtMeeting.
void Frontend::goToMeeting() {
    Career& career = *m_data->career;
    CareerProfile& p = career.profile;
    clearNotes(career, career.meeting);
    clearNotes(career, career.respectChanges);
    const CalendarEvent* e = career.meetingEvent;
    if (e == nullptr) return;
    while (career.calendar.dayOfMonth != e->dayOfMonth || career.calendar.month != e->month) {
        m_data->nextDay(gameRandom());
    }
    std::vector<int32_t> list;
    for (size_t k = 0; k < career.garage.size(); ++k) {
        const Car& car = career.garage[k];
        int reason = 0;
        mayEnter(career, *e, reason, m_data->catalogue);
        bool fits = reason == 0 || reason == 1 || reason == 7;
        if (!fits && car.def != nullptr) {
            fits = (e->region == 0 || car.def->region == e->region) &&
                   (e->make == 0 || car.def->make == e->make) && (e->model == 0 || car.def->id == e->model) &&
                   (e->extreme == 0 || heavilyTuned(car, m_data->catalogue)) &&
                   (carClass(car) == e->carClass || e->carClass == 8);
        }
        // Stock problem: past 55 cars the list runs over its count (+0x5F0) and on; not kept.
        if (fits && list.size() < 55) list.push_back(static_cast<int32_t>(k));
    }
    const auto rating = [&](int32_t k) { return std::bit_cast<float>(career.garage[static_cast<size_t>(k)].rating); };
    for (size_t i = 0; i + 1 < list.size(); ++i) {
        if (rating(list[i]) < rating(list[i + 1])) {
            std::swap(list[i], list[i + 1]);
            i = static_cast<size_t>(-1);
        }
    }
    p.raceChoiceCount = static_cast<int32_t>(list.size());
    for (size_t i = 0; i < list.size(); ++i) p.raceChoices[i] = list[i];
    request("cmsAtMeeting", SwitchMode::Replace);
}

// The text lines of the player's crew (108 "Player's crew" and 117 "PLAYER'S CREW") take its name: when the settings
// are read (0x00409D20's end), when a new driver's crew is named (0x00409F30) and when CMSAtMeeting opens.
void Frontend::setCrewNameTexts(const std::wstring& name) {
    m_gameTexts.set(kTextCrewNames, name);
    m_gameTexts.set(kTextCrewNamesUpper, name);
}

ui::Screen* Frontend::phoneScreen() {
    for (Popup& p : m_popups) {
        if (is(p.state, "CMSMobilePhone")) return p.screen.get();
    }
    return nullptr;
}

// 0x004B60F0 in the career's windows: the phone's frame (the window it saw last is this front end's own).
void Frontend::phoneFrame(ui::Screen& screen) {
    m_mobilePhone.frame(*this, screen, m_phoneScreen);
}

}  // namespace oj
