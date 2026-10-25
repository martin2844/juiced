// CMSAtMeeting: a meeting's events.

#include "Game/Frontend/Career/AtMeeting.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/Career/Calendar.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace oj::frontend {

namespace {

// The menu items' values (the item's +0x24).
constexpr float kItemEnter = 2.0f;
constexpr float kItemWatch = 3.0f;

}  // namespace

// 0x004C0B60: the event's details: every element named extra1, laps, type, prize, sponsored, route, routetxt,
// lapstitle, typetitle and prizetitle hidden; lapstitle, typetitle and prizetitle shown; Prize the round's first prize
// (" NA" for none); route "ROUTE:"; routetxt the route: for a circuit or a challenge its number (the route slot + 1,
// past 3 the slot - 3 and "Reversed"), "Showoff", "Point-to-point" (with "Reversed" past the first slot) or "Sprint".
void AtMeeting::details(CareerHost& host, ui::Screen& s) {
    const TextTable& texts = host.gameTexts();
    const CalendarEvent& e = *m_event;
    for (const char* name : {"extra1", "laps", "type", "prize", "sponsored", "route", "routetxt", "lapstitle",
                             "typetitle", "prizetitle"}) {
        hideAll(s, name);
    }
    s.show("lapstitle", false);
    s.show("typetitle", false);
    s.show("prizetitle", false);
    if (const int prize = s.find("Prize"); prize >= 0) {
        const int32_t first = e.round.prize[0];
        s.setText(prize, first == 0 ? L" " + texts.at(kTextNotAvailable)
                                    : L" " + toWide(moneyText(first, host.language().index)));
        s.setHidden(prize, false);
    }
    if (const int route = s.find("route"); route >= 0) {
        s.setText(route, texts.at(kTextRoute) + L":");
        s.setHidden(route, false);
    }
    if (const int text = s.find("routetxt"); text >= 0) {
        std::wstring t = L" ";
        const EventKind type = e.round.type;
        const int slot = e.round.routeIndex;
        if (type == EventKind::Circuit || type == EventKind::Challenge) {
            if (slot > 3) {
                t += toWide(std::format("{} ", slot - 3)) + texts.at(kTextReversed);
            } else {
                t += toWide(std::format("{}", slot + 1));
            }
        }
        if (type == EventKind::Showoff) t += texts.at(kTextShowoff);
        if (type == EventKind::PointToPoint) {
            t += texts.at(kTextPointToPoint);
            if (slot >= 1) t += L" " + texts.at(kTextReversed);
        }
        if (type == EventKind::Sprint) t += texts.at(kTextSprintRoute);
        s.setText(text, t);
        s.setHidden(text, false);
    }
}

// 0x004C1060: the race's set up: the current event's crew with its +0x2D0 over 0 lowers it by one (not below 0) when
// rand() / 32767 is above 0.75; a crew of more than one member in a round not a cruise nor a challenge, and not a
// pink slip, goes to CMSCrewRaceSetup; else the first racer is member 0 (the profile's +0x5F4), the profile's +0x614
// is set and CMSRaceSetup follows.
void AtMeeting::toRaceSetup(CareerHost& host) {
    GameData* data = host.gameData();
    Career& career = *data->career;
    CareerProfile& p = career.profile;
    CalendarEvent* e = currentEvent(career.calendar);
    if (e == nullptr) return;
    const EventRound& round = *roundOf(career, *e);
    Crew& crew = career.crews[static_cast<size_t>(e->crew)];
    if (crew.pinkSlipsWon != 0) {
        const float r = static_cast<float>(gameRandom().next()) * 3.0518509e-05f;
        if (r > 0.75f) {
            --crew.pinkSlipsWon;
            if (crew.pinkSlipsWon < 0) crew.pinkSlipsWon = 0;
        }
    }
    if (p.memberCount != 1 && round.type != EventKind::Showoff && round.type != EventKind::Challenge &&
        e->pinkSlip == 0) {
        host.request("CMSCrewRaceSetup", SwitchMode::Replace);
        return;
    }
    p.raceMembers[0] = 0;
    p.raceCarsChosen = 1;
    host.request("CMSRaceSetup", SwitchMode::Replace);
}

// The race entered (0x004C0810 and the menu's ENTER item): the session to be set up again (+0x264), the profile's
// +0x614 0, +0x618 1 and +0x61C 0, the bets cleared (0x004B1620), the event attended and its fee paid (0x004B7F00).
void AtMeeting::enterRace(CareerHost& host) {
    GameData* data = host.gameData();
    Career& career = *data->career;
    CareerProfile& p = career.profile;
    career.session.needsSetup = 1;
    p.raceCarsChosen = 0;
    p.racing = 1;
    p.watching = 0;
    clearBets(career.bets);
    if (career.meetingEvent != nullptr) attendEvent(career, *career.meetingEvent, data->catalogue);
}

// 0x004C01E0: the player's crew's text lines its name (the settings' +0x90, made wide with signed characters); the
// career's money counters (+0x58) cleared (0x004B5D60); the profile's +0x634 0; the advisor's last advice the one given
// when its event is this one, else 13, and no advice (13, no event); Cash and Date; the event (+0x10) the one chosen
// (the profile's +0x610); RaceMenu emptied; CrewLogo the event's crew's logo of the flyers and CD_Leader its leader's
// picture (by the crew's id); Crew the crew's name in capitals (117 + crew); Location the location of the first circuit
// of the crew of +0x0D; the details (0x004C0B60); +0x14 .. +0x20 0; the crew's +0x2D4 and +0x2D8 0; the route's map
// (0x0045B6B0; place 0 at the pitch -0.4, 0x0045BE00; the first round's route added, 0x0045B720; a cruise then looks at
// its location's text, 0x0045BEC0: a stock problem in other languages, as in the arcade). The menu: "ENTER CHALLENGE",
// "ENTER SHOWOFF", "ENTER SPRINT" or "ENTER RACE" (by the type) when the player may race it (0x004B8060) with the fee
// or for the player's own event; "WATCH SPRINT" or "WATCH RACE" for a race or a sprint that is not special. RaceMenu
// shown; the profile's +0x630 0; the flyer (0x004B2E20).
void AtMeeting::enter(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career || data->career->meetingEvent == nullptr) return;
    Career& career = *data->career;
    CareerProfile& p = career.profile;
    CalendarEvent& e = *career.meetingEvent;
    const save::Settings settings = host.driverSettings() ? *host.driverSettings() : save::Settings::defaults(host.language().index);
    host.setCrewNameTexts(widenSigned(settings.crewName()));
    career.stats = CareerStats{};
    p.showoffTutorialShown = 0;
    career.advisor.lastAdvice = career.adviceEvent == &e ? career.advisor.advice : 13;
    career.adviceEvent = nullptr;
    career.advisor.advice = 13;
    careerCashTexts(s, career, host.language().index, texts);
    m_event = &e;
    const Crew& crew = career.crews[static_cast<size_t>(e.crew)];
    s.clearMenu("RaceMenu");
    const auto id = static_cast<size_t>(crew.id);
    if (id < career.flyers.crews.size()) s.setTexture("CrewLogo", career.flyers.crews[id].logo);
    if (id < std::size(kLeaderPictures)) s.setTexture("CD_Leader", kLeaderPictures[id]);
    s.setText("Crew", texts.at(kTextCrewNamesUpper + static_cast<size_t>(std::max<int>(e.crew, 0))));
    // Stock problem: a +0x0D outside 0 .. 8 reads past the crews; no location then.
    if (e.location >= 0 && e.location < 9) {
        const Crew& hostCrew = career.crews[static_cast<size_t>(e.location)];
        const auto route = static_cast<size_t>(hostCrew.circuitRoute[0]);
        const std::vector<ExeTables::Route>& routes = data->tables.routes;
        const int location = route < routes.size() ? routes[route].location : 0;
        s.setText("Location", texts.at(kTextLocations + static_cast<size_t>(std::max(location, 0))));
    }
    details(host, s);
    m_respectAnswer = 0;
    m_leaving = 0;
    m_leaveAnswer = 0;
    m_feeAnswer = 0;
    career.crews[static_cast<size_t>(e.crew)].unused = 0;
    career.crews[static_cast<size_t>(e.crew)].meterBlinks = 0;
    m_map.start(s);
    if (m_map.view3d(s) != nullptr) m_map.look(host, s, 0, -0.4f);
    const std::vector<ExeTables::Route>& routes = data->tables.routes;
    if (static_cast<size_t>(e.round.route) < routes.size()) m_map.route(host, s, routes[e.round.route].name, true);
    if (mayRace(career, e, data->catalogue) && (p.money >= e.entranceFee || e.crew == 0)) {
        size_t line = kTextEnterRace;
        if (e.type == EventKind::Challenge) line = kTextEnterChallenge;
        if (e.type == EventKind::Showoff) line = kTextEnterShowoff;
        if (e.type == EventKind::Sprint) line = kTextEnterSprint;
        s.addMenuItem("RaceMenu", texts.at(line), kItemEnter);
    }
    if (!(e.hostedByPlayer != 0 || e.pinkSlip != 0 || e.type == EventKind::Showoff || e.type == EventKind::Challenge)) {
        s.addMenuItem("RaceMenu", texts.at(e.type == EventKind::Sprint ? kTextWatchSprint : kTextWatchRace),
                      kItemWatch);
    }
    if (e.type == EventKind::Showoff && e.location >= 0 && e.location < 9) {
        const auto route = static_cast<size_t>(career.crews[static_cast<size_t>(e.location)].circuitRoute[0]);
        const int location = route < routes.size() ? routes[route].location : 0;
        // The fixed build: the location's English name (as the arcade's map), the same place in every language.
        m_map.lookAt(host, s, englishLocation(host, std::max(location, 0)));
    }
    s.setHidden("RaceMenu", false);
    p.outOfRace = 0;
    eventFlyer(host, s, career, e);
}

// 0x004C06A0: the route's map's frame (0x0045BC40). Leaving a special event answered "yes" (+0x14): the
// crew's respect message of kind 2 (0x004BC7B0 with the event's +0x02) and +0x18 set; then (+0x18) or leaving another
// event answered "yes" (+0x1C): the event's end (0x004A6870), the history's step (0x004BD8A0), CLICK and CMSCalendar.
// The fee answered "yes" (+0x20): the race entered and its set up (0x004C1060). BACK (CLICK) asks "Are you sure you
// want to leave the race event?" (or for a special event "Leave event and lose respect?") with "The event will be
// closed and you will not be able to return.". SELECT (CLICK) on ENTER, when the player may race it: with a fee for
// another crew's event "Enter race event? Fee:" and the fee, else the race entered, RaceMenu hidden and its set up;
// on WATCH: the session to be set up again, the profile's +0x614 0, +0x618 0, +0x61C 1 and CMSGridWalk.
void AtMeeting::update(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career || m_event == nullptr) return;
    m_map.frame(host, s);
    Career& career = *data->career;
    CareerProfile& p = career.profile;
    CalendarEvent& e = *m_event;
    if (m_respectAnswer == 1) {
        crewRespect(host, career, e.crew, RespectReason::BackedOut, e.hostedByPlayer, host.language().index, texts);
        m_respectAnswer = 0;
        m_leaving = 1;
        return;
    }
    if (m_leaving != 0 || m_leaveAnswer == 1) {
        endEvent(career, e, data->tables);
        recordHistory(career);
        host.sound("CLICK");
        host.request("CMSCalendar", SwitchMode::Replace);
        return;
    }
    if (m_feeAnswer == 1) {
        enterRace(host);
        toRaceSetup(host);
    }
    if (event == input::kBack) {
        host.sound("CLICK");
        if (!specialEvent(e)) {
            host.message(MessageType::YesNo, texts.at(kTextLeaveEvent) + L"\n" + texts.at(kTextEventClosed), false,
                         &m_leaveAnswer, true);
        } else {
            host.message(MessageType::YesNo, texts.at(kTextLeaveSpecial) + L"\n" + texts.at(kTextEventClosed), false,
                         &m_respectAnswer, true);
        }
    }
    if (event != input::kSelect || s.menuSize("RaceMenu") == 0) return;
    host.sound("CLICK");
    const float value = s.menuItemValue("RaceMenu", s.selectedMenuItem("RaceMenu"));
    if (value == kItemEnter) {
        if (!mayRace(career, *m_event, data->catalogue)) return;
        const CalendarEvent* today = currentEvent(career.calendar);
        if (today != nullptr && today->entranceFee != 0 && today->crew != 0) {
            host.message(MessageType::YesNo,
                         texts.at(kTextEnterFee) + toWide(moneyText(today->entranceFee, host.language().index)), false,
                         &m_feeAnswer, true);
        } else {
            enterRace(host);
            s.setHidden("RaceMenu", true);
            toRaceSetup(host);
        }
        return;
    }
    if (value == kItemWatch) {
        career.session.needsSetup = 1;
        p.raceCarsChosen = 0;
        p.racing = 0;
        p.watching = 1;
        host.request("CMSGridWalk", SwitchMode::Replace);
    }
}

// 0x004C0650: the state keeps the selected item of a Menu element (its +0x13C, when the window has one) and the route's
// map is freed.
void AtMeeting::leave() {
    m_map = CityMap{};
}

}  // namespace oj::frontend
