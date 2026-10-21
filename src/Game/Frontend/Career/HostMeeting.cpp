// Hosting a meeting: its setup, its race and its summary.

#include "Game/Frontend/Career/HostMeeting.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>

namespace oj::frontend {

namespace {

// A hosted race's kind stepped left or right: PointToPoint, Circuit, Sprint, kept at the ends.
EventKind stepKind(EventKind kind, int32_t step) {
    return static_cast<EventKind>(std::clamp(static_cast<int32_t>(kind) + step,
                                             static_cast<int32_t>(EventKind::PointToPoint),
                                             static_cast<int32_t>(EventKind::Sprint)));
}

constexpr int32_t kHostCosts[9] = {1000, 2000, 4000, 8000, 10000, 12000, 15000, 20000, 30000};  // 0x00718FB4
constexpr uint32_t kHostWhite = 0xFFFFFFFF;
constexpr uint32_t kHostRed = 0xFFBC381D;
constexpr uint32_t kHostRed2 = 0xFFBA381D;
constexpr int32_t kHostLaps[11] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};  // 0x00719044
constexpr int32_t kHostPrizes[7][3] = {{3000, 2000, 1000},   {7000, 3000, 2000},   {12000, 5000, 3000},
                                       {20000, 8000, 5000},  {25000, 10000, 5000}, {30000, 12000, 6000},
                                       {40000, 15000, 7000}};  // 0x00718FD8
constexpr uint32_t kHostGrey = 0xFF646432;

}  // namespace

// 0x004CD580: an event of the setup (+0x0D the location, +0x10 the type, +0x14 5000, +0x6C the class; a region
// (+0x60) or a make (+0x64) by the criteria; its prize the rounds' and CMSHostMeetingSetupRace's from 0x00718FD8, not
// read further) and for crews 1 .. 8 whether each comes (0x004A9480) into +0x40 ..; the crews coming.
int32_t HostMeeting::attendance(CareerHost& host) {
    GameData* data = host.gameData();
    const Career& career = *data->career;
    CalendarEvent e{};
    e.challengeIndex = -1;  // A hosted meeting is not a crew challenge.
    e.location = static_cast<int8_t>(m_setup.location);
    e.type = m_setup.type;
    e.entranceFee = 5000;
    e.carClass = m_setup.carClass;
    if (m_setup.criteriaType == CarCriteria::Region) e.region = m_setup.region;
    if (m_setup.criteriaType == CarCriteria::Make) e.make = m_setup.make;
    int32_t coming = 0;
    for (size_t k = 0; k < 8; ++k) {
        m_setup.attend[k] = 0;
        if (hostAttends(career, career.crews[k + 1], e, data->catalogue)) {
            ++coming;
            m_setup.attend[k] = 1;
        }
    }
    return coming;
}

// 0x004CAE60: [0x0075EF70] 0; TypeText the type's line (140 + type); LocationText the location's crew (108 + it) and "
// (<the track of its first circuit route>)" (575 + the route's location; the map looks at the location by its English
// name, 0x0045BEC0), white with the crew's fourth step (+0x60) and respect (+0x70) of 1000 or more, else red and reason
// 1; CriteriaText "None" (164) in 0xFF646432, or the region (188 + it) or the make (193 + it) with the opponents' cars
// of it, the class and the type (0x004A9820 on crew 1) none: reason 5; with reason 5 CriteriaText, CriteriaTypeText
// (184 + the type) and BHPText red; BHPText the class (604 + it) and " (<low> - <high>" (0x0047FCA0) "bhp)"; CostValue
// the location's cost (0x00718FB4) into +0x34 and [0x00728C6C]; CashValue the money, red and reason 2 under the cost;
// Attendance the crews coming (0x004CD580), red under 2.
void HostMeeting::setupTexts(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    const Career& career = *data->career;
    const int language = host.language().index;
    m_reason = 0;
    const auto colour = [&](const char* name, uint32_t argb) {
        if (const int e = s.find(name); e >= 0) s.setFontColour(e, argb);
    };
    s.setText("TypeText", texts.at(kTextRoundTypes + static_cast<size_t>(m_setup.type)));
    const Crew& crew = career.crews[static_cast<size_t>(std::clamp(m_setup.location, 0, 8))];
    const auto route = static_cast<size_t>(crew.circuitRoute[0]);
    const auto& routes = data->tables.routes;
    const size_t track = route < routes.size() ? static_cast<size_t>(routes[route].location) : 0;
    s.setText("LocationText", texts.at(kTextCrewNames + static_cast<size_t>(m_setup.location)) + L" (" +
                                  texts.at(kTextLocations + track) + L")");
    m_setup.map.lookAt(host, s, englishLocation(host, static_cast<int32_t>(track)));
    if (crew.stage[3] != 0 && !(static_cast<float>(crew.respect) < 1000.0f)) {
        colour("LocationText", kHostWhite);
    } else {
        colour("LocationText", kHostRed);
        m_reason = 1;
    }
    if (m_setup.criteriaType == CarCriteria::None) {
        s.setText("CriteriaText", texts.at(kTextNone));
        colour("CriteriaText", 0xFF646432);
    } else if (m_setup.criteriaType == CarCriteria::Region || m_setup.criteriaType == CarCriteria::Make) {
        const bool byRegion = m_setup.criteriaType == CarCriteria::Region;
        s.setText("CriteriaText", texts.at(byRegion ? kTextRegions + static_cast<size_t>(m_setup.region)
                                                          : kTextMakes + static_cast<size_t>(m_setup.make)));
        colour("CriteriaText", kHostWhite);
        const int cars = hostCars(career, career.crews[1], byRegion ? m_setup.region : 0, byRegion ? 0 : m_setup.make,
                                  m_setup.carClass, m_setup.type);
        if (cars < 1) m_reason = 5;
    }
    if (m_reason == 5) colour("CriteriaText", kHostRed2);
    s.setText("CriteriaTypeText", texts.at(kTextCriteriaTypes + static_cast<size_t>(m_setup.criteriaType)));
    colour("CriteriaTypeText", m_reason == 5 ? kHostRed2 : kHostWhite);
    int low = 0, high = 0;
    classRatings(m_setup.carClass, low, high);
    s.setText("BHPText", texts.at(kTextClasses + static_cast<size_t>(m_setup.carClass)) +
                             toWide(std::format(" ({} - {}", low, high)) + texts.at(kTextBhp) + L")");
    colour("BHPText", m_reason == 5 ? kHostRed2 : kHostWhite);
    m_setup.cost = kHostCosts[std::clamp(m_setup.location, 0, 8)];
    s.setText("CostValue", toWide(moneyText(m_setup.cost, language)));
    m_cost = m_setup.cost;
    s.setText("CashValue", toWide(moneyText(career.profile.money, language)));
    if (career.profile.money < m_cost) {
        m_reason = 2;
        colour("CashValue", kHostRed);
    } else {
        colour("CashValue", kHostWhite);
    }
    const int32_t coming = attendance(host);
    s.setText("Attendance", toWide(std::format("{}", coming)));
    colour("Attendance", coming < 2 ? kHostRed : kHostWhite);
}

// 0x004CA9A0: with +0x38 (the calendar's) the defaults: type 3, location 0, region 1, make 1, class 7, +0x30 1, cost 0,
// CMSHostMeetingSetupRace's +0x20 0 and no rounds ([0x00728C70]); the meeting's map (0x0045B6B0; place 0 at the pitch
// -0.4, 0x0045BE00); the texts (0x004CAE60); +0x0C -1; Cash and Date; the advisor's +0x60 (no advice to host) set.
void HostMeeting::enterSetup(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    if (m_setup.defaults != 0) {
        m_setup.type = EventKind::PointToPoint;
        m_setup.location = 0;
        m_setup.region = 1;
        m_setup.make = 1;
        m_setup.carClass = 7;
        m_setup.unused = 1;
        m_setup.cost = 0;
        m_race.prize = 0;
        m_rounds = 0;
        m_roundList.clear();
    }
    m_setup.map.start(s);
    if (m_setup.map.view3d(s) != nullptr) m_setup.map.look(host, s, 0, -0.4f);
    setupTexts(host, s);
    m_setup.answer = -1;
    careerCashTexts(s, career, host.language().index, texts);
    career.advisor.hosted = 1;
}

// 0x004CAA90: the state keeps a Menu element's highlighted item; the map freed.
void HostMeeting::leaveSetup() {
    m_setup.map = CityMap{};
}

// 0x004CAAE0: the map's frame (0x0045BC40). A "yes" (+0x0C 1): CLICK, CMSMainMenu. BACK (CLICK):
// "Cancel event setup?" (1827) and "ARE YOU SURE?" (type 3, on NO) into +0x0C. SELECT (CLICK): with no reason
// CMSHostMeetingSetupRace, else a note of it (type 1: reason 5 line 1996, else 476 + reason). LEFT or RIGHT (CLICK) on
// the highlighted button: Type (3 .. 5), Location (0 .. 8), CriteriaType (0 .. 2), Criteria (by the criteria: the
// region 1 .. 4 or the make 1 .. 17; with none nothing), carclass (the other way, 0 .. 7), each with the texts again.
void HostMeeting::updateSetup(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    m_setup.map.frame(host, s);
    if (m_setup.answer == 1) {
        host.sound("CLICK");
        host.request("CMSMainMenu", SwitchMode::Replace);
        return;
    }
    if (event == input::kBack) {
        host.sound("CLICK");
        host.message(MessageType::YesNo, texts.at(kTextCancelSetup) + L"\n" + texts.at(kTextAreYouSure), false,
                     &m_setup.answer, true);
        return;
    }
    if (event == input::kSelect) {
        host.sound("CLICK");
        if (m_reason == 0) {
            host.request("CMSHostMeetingSetupRace", SwitchMode::Replace);
        } else {
            host.message(MessageType::Continue,
                         texts.at(m_reason == 5 ? kTextNoCarsToHost : kTextNoHost + static_cast<size_t>(m_reason)),
                         false, nullptr);
        }
        return;
    }
    if (event != input::kLeft && event != input::kRight) return;
    host.sound("CLICK");
    const int32_t step = event == input::kLeft ? -1 : 1;
    const std::string current = s.currentButton();
    const auto is1 = [&](const char* name) { return _stricmp(current.c_str(), name) == 0; };
    if (is1("Type")) {
        m_setup.type = stepKind(m_setup.type, step);
        setupTexts(host, s);
    }
    if (is1("Location")) {
        m_setup.location += step;
        if (m_setup.location < 0) m_setup.location = 0;
        if (m_setup.location == 9) m_setup.location = 8;
        setupTexts(host, s);
    }
    if (is1("CriteriaType")) {
        m_setup.criteriaType = static_cast<CarCriteria>(std::clamp(static_cast<int32_t>(m_setup.criteriaType) + step,
                                                                   static_cast<int32_t>(CarCriteria::None),
                                                                   static_cast<int32_t>(CarCriteria::Make)));
        setupTexts(host, s);
    }
    if (is1("Criteria")) {
        if (m_setup.criteriaType == CarCriteria::Region) {
            m_setup.region += step;
            if (m_setup.region < 1) m_setup.region = 1;
            if (m_setup.region == 5) m_setup.region = 4;
            setupTexts(host, s);
        } else if (m_setup.criteriaType == CarCriteria::Make) {
            m_setup.make += step;
            if (m_setup.make < 1) m_setup.make = 1;
            if (m_setup.make == 0x12) m_setup.make = 0x11;
            setupTexts(host, s);
        }
    }
    if (is1("carclass")) {
        m_setup.carClass -= step;
        if (m_setup.carClass < 0) m_setup.carClass = 0;
        if (m_setup.carClass >= 8) m_setup.carClass = 7;
        setupTexts(host, s);
    }
}

// 0x004CBA80: [0x0075EF70] 0. TypeText the type's line (140 + type). RouteText: a point to point "1", or "1 (Reversed)"
// (127) past route 0; other types the route + 1, or past 3 "<route - 3> (Reversed)"; white for types 3 and 4, else
// grey 0xFF646432; the route's distance: circuits the location crew's circuit route and km (+0x1C4, +0x1E4), sprints
// +0x24C, +0x250, point to points +0x22C, +0x234, by the route (its model added to the map, 0x0045B720). LapsText:
// point to points and sprints "1 (<km>km)" in grey, else "<laps> (<laps x km>km)" (0x00719044) in white; with the
// settings' miles (+0x94) "m" and the distance x 0.625. NumCrewText: a sprint makes the team 1 and grey, else white;
// the team's line (491 + it). prizeMoneyText1 .. 3: "1st $", "2nd $", "3rd $" (37 ..) and the prize's places
// (0x00718FD8, thousands marked). CostValue the setup's cost and the rounds' +0x14 ([0x00728C6C]); CashValue the
// money, under the cost red and reason 2; Attendance the crews coming (0x004CD580), red under 2.
void HostMeeting::raceTexts(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    const Career& career = *data->career;
    const int language = host.language().index;
    m_reason = 0;
    const auto colour = [&](const char* name, uint32_t argb) {
        if (const int e = s.find(name); e >= 0) s.setFontColour(e, argb);
    };
    s.setText("TypeText", texts.at(kTextRoundTypes + static_cast<size_t>(m_race.type)));
    std::wstring route;
    if (m_race.type == EventKind::PointToPoint) {
        route = m_race.route > 0 ? L"1 (" + texts.at(kTextReversed) + L")" : L"1";
    } else if (m_race.route > 3) {
        route = toWide(std::format("{} (", m_race.route - 3)) + texts.at(kTextReversed) + L")";
    } else {
        route = toWide(std::format("{}", m_race.route + 1));
    }
    s.setText("RouteText", route);
    const Crew& crew = career.crews[static_cast<size_t>(std::clamp(m_setup.location, 0, 8))];
    const auto at = static_cast<size_t>(std::max(m_race.route, 0));
    float km = 0.0f;
    if (m_race.type == EventKind::Circuit) {
        km = at < crew.circuitKm.size() ? crew.circuitKm[at] : 0.0f;
    } else if (m_race.type == EventKind::Sprint) {
        km = crew.sprintKm;  // Stock problem: the stock code reads +0x250 + 4 x the route.
    } else {
        km = at < crew.p2pKm.size() ? crew.p2pKm[at] : 0.0f;
    }
    // Stock problem: a route past the type's list reads the fields after it; no route's model then.
    int32_t mapRoute = -1;
    if (m_race.type == EventKind::Circuit) {
        if (at < crew.circuitRoute.size()) mapRoute = crew.circuitRoute[at];
    } else if (m_race.type == EventKind::Sprint) {
        if (at == 0) mapRoute = crew.sprintRoute;
    } else if (at < crew.p2pRoute.size()) {
        mapRoute = crew.p2pRoute[at];
    }
    const auto& routes = data->tables.routes;
    if (mapRoute >= 0 && static_cast<size_t>(mapRoute) < routes.size()) {
        m_race.map.route(host, s, routes[static_cast<size_t>(mapRoute)].name, true);
    }
    colour("RouteText",
           m_race.type == EventKind::Circuit || m_race.type == EventKind::PointToPoint ? kHostWhite : kHostGrey);
    const bool miles = host.optionSettings().u32(save::Settings::kMph) != 0;
    if (m_race.type == EventKind::Sprint || m_race.type == EventKind::PointToPoint) {
        s.setText("LapsText", toWide(miles ? std::format("1 ({:.1f}m)", static_cast<double>(km * 0.625f))
                                           : std::format("1 ({:.1f}km)", static_cast<double>(km))));
        colour("LapsText", kHostGrey);
    } else {
        const int32_t laps = kHostLaps[std::clamp(m_race.laps, 0, 10)];
        const float length = static_cast<float>(laps) * km;
        s.setText("LapsText", toWide(miles ? std::format("{} ({:.1f}m)", laps, static_cast<double>(length * 0.625f))
                                           : std::format("{} ({:.1f}km)", laps, static_cast<double>(length))));
        colour("LapsText", kHostWhite);
    }
    if (m_race.type == EventKind::Sprint) {
        m_race.crew = 1;
        colour("NumCrewText", kHostGrey);
    } else {
        colour("NumCrewText", kHostWhite);
    }
    s.setText("NumCrewText", texts.at(kTextTeamSizes + static_cast<size_t>(m_race.crew)));
    const auto& prizes = kHostPrizes[std::clamp(m_race.prize, 0, 6)];
    for (size_t k = 0; k < 3; ++k) {
        s.setText(std::format("prizeMoneyText{}", k + 1),
                  toWide(std::format("{}", k + 1)) + texts.at(kTextOrdinals + k) + L" $" +
                      toWide(groupThousands(prizes[k], language)));
    }
    m_cost = m_setup.cost;
    for (const HostRound& r : m_roundList) m_cost += r.cost;
    s.setText("CostValue", toWide(moneyText(m_cost, language)));
    s.setText("CashValue", toWide(moneyText(career.profile.money, language)));
    if (career.profile.money < m_cost) {
        m_reason = 2;
        colour("CashValue", kHostRed);
    } else {
        colour("CashValue", kHostWhite);
    }
    const int32_t coming = attendance(host);
    s.setText("Attendance", toWide(std::format("{}", coming)));
    colour("Attendance", coming < 2 ? kHostRed : kHostWhite);
}

// 0x004CB550: the type the setup's (+0x14), route 0, laps 0, team 1, prize the setup's location (+0x18); the map
// (0x0045B6B0) looking at the location crew's first circuit's location by its English name (0x0045BEC0); the texts
// (0x004CBA80); +0x0C -1; Cash and Date.
void HostMeeting::enterRace(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    m_race.type = m_setup.type;
    m_race.route = 0;
    m_race.laps = 0;
    m_race.crew = 1;
    m_race.prize = m_setup.location;
    m_race.map.start(s);
    if (m_race.map.view3d(s) != nullptr) {
        const Crew& crew = data->career->crews[static_cast<size_t>(std::clamp(m_setup.location, 0, 8))];
        const auto route = static_cast<size_t>(crew.circuitRoute[0]);
        const auto& routes = data->tables.routes;
        m_race.map.lookAt(host, s, englishLocation(host, route < routes.size() ? routes[route].location : 0));
    }
    raceTexts(host, s);
    m_race.answer = -1;
    careerCashTexts(s, *data->career, host.language().index, texts);
}

// 0x004CB690: the map's frame (0x0045BC40). A "yes" (+0x0C 1): CMSMainMenu. BACK (CLICK): "Cancel event setup?" and
// "ARE YOU SURE?" (type 3, on NO) into +0x0C. SELECT (CLICK): with no reason the round kept ([0x00728C74 ..]: type,
// route, laps, team, prize, 0) and CMSHostMeetingSummary; else a note (type 1, 476 + the reason). LEFT or RIGHT (CLICK)
// on the highlighted button: Type (only for a setup of type 5: 3 .. 5, the route 0), Route (0 up to the location
// crew's routes of the type: point to points +0x23C, circuits +0x228, sprints +0x254), Laps (0 .. 10), Crewnum (1 ..
// 3), prize (0 .. 6), each with the texts again.
void HostMeeting::updateRace(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    const Career& career = *data->career;
    m_race.map.frame(host, s);
    if (m_race.answer == 1) {
        host.request("CMSMainMenu", SwitchMode::Replace);
        return;
    }
    if (event == input::kBack) {
        host.sound("CLICK");
        host.message(MessageType::YesNo, texts.at(kTextCancelSetup) + L"\n" + texts.at(kTextAreYouSure), false,
                     &m_race.answer, true);
        return;
    }
    if (event == input::kSelect) {
        host.sound("CLICK");
        if (m_reason == 0) {
            m_roundList.push_back(HostRound{m_race.type, m_race.route, m_race.laps, m_race.crew, m_race.prize, 0});
            m_rounds = static_cast<int32_t>(m_roundList.size());
            host.request("CMSHostMeetingSummary", SwitchMode::Replace);
        } else {
            host.message(MessageType::Continue, texts.at(kTextNoHost + static_cast<size_t>(m_reason)), false, nullptr);
        }
        return;
    }
    if (event != input::kLeft && event != input::kRight) return;
    host.sound("CLICK");
    const int32_t step = event == input::kLeft ? -1 : 1;
    const std::string current = s.currentButton();
    const auto is1 = [&](const char* name) { return _stricmp(current.c_str(), name) == 0; };
    if (is1("Type")) {
        if (m_setup.type == EventKind::Sprint) {
            m_race.type = stepKind(m_race.type, step);
            m_race.route = 0;
        }
        raceTexts(host, s);
    }
    if (is1("Route")) {
        const Crew& crew = career.crews[static_cast<size_t>(std::clamp(m_setup.location, 0, 8))];
        m_race.route += step;
        if (m_race.route < 0) m_race.route = 0;
        int32_t most = -1;
        if (m_race.type == EventKind::PointToPoint) most = crew.p2pCount;
        if (m_race.type == EventKind::Circuit) most = crew.circuitCount;
        if (m_race.type == EventKind::Sprint) most = crew.sprintCount;
        if (most >= 0 && m_race.route == most) m_race.route = most - 1;
        raceTexts(host, s);
    }
    if (is1("Laps")) {
        m_race.laps += step;
        if (m_race.laps < 0) m_race.laps = 0;
        if (m_race.laps == 11) m_race.laps = 10;
        raceTexts(host, s);
    }
    if (is1("Crewnum")) {
        m_race.crew = std::clamp(m_race.crew + step, 1, 3);
        raceTexts(host, s);
    }
    if (is1("prize")) {
        m_race.prize += step;
        if (m_race.prize < 0) m_race.prize = 0;
        if (m_race.prize == 7) m_race.prize = 6;
        raceTexts(host, s);
    }
}

// 0x004CC820: [0x0075EF70] 0. Race1Type, Race1Laps, Race1Route, Race1NumCrew, Race11st .. Race13rd "-"; location the
// track of the location crew's first circuit route (575 + its location; the map looks at it by its English name,
// 0x0045BEC0);
// event the setup's type (140 + it). For each round n kept: Race<n>Type its type's line, Race<n>Laps "-" for point to
// points and sprints, else its laps (0x00719044), Race<n>Route as in the race's set up without "Reversed" ("1", "1 R",
// "<n> R", "<n>"), Race<n>NumCrew its team, Race<n>1st .. 3rd the prize's places in thousands "$<n>k". CostValue the
// setup's cost ([0x00728C6C]); CashValue the money, under the cost red and reason 2; Attendance the crews coming
// (0x004CD580), red under 2.
void HostMeeting::summaryTexts(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    const Career& career = *data->career;
    const int language = host.language().index;
    m_reason = 0;
    const auto colour = [&](const char* name, uint32_t argb) {
        if (const int e = s.find(name); e >= 0) s.setFontColour(e, argb);
    };
    for (const char* name : {"Race1Type", "Race1Laps", "Race1Route", "Race1NumCrew", "Race11st", "Race12nd",
                             "Race13rd"}) {
        s.setText(name, L"-");
    }
    const Crew& crew = career.crews[static_cast<size_t>(std::clamp(m_setup.location, 0, 8))];
    const auto route = static_cast<size_t>(crew.circuitRoute[0]);
    const auto& routes = data->tables.routes;
    const size_t track = route < routes.size() ? static_cast<size_t>(routes[route].location) : 0;
    m_summary.map.lookAt(host, s, englishLocation(host, static_cast<int32_t>(track)));
    s.setText("location", texts.at(kTextLocations + track));
    s.setText("event", texts.at(kTextRoundTypes + static_cast<size_t>(m_setup.type)));
    for (size_t i = 0; i < m_roundList.size(); ++i) {
        const HostRound& r = m_roundList[i];
        const auto name = [&](const char* what) { return std::format("Race{}{}", i + 1, what); };
        s.setText(name("Type"), texts.at(kTextRoundTypes + static_cast<size_t>(r.type)));
        if (r.type == EventKind::Sprint || r.type == EventKind::PointToPoint) {
            s.setText(name("Laps"), L"-");
        } else {
            s.setText(name("Laps"), toWide(std::format("{}", kHostLaps[std::clamp(r.laps, 0, 10)])));
        }
        std::string text;
        if (r.type == EventKind::PointToPoint) {
            text = r.route > 0 ? "1 R" : "1";
        } else {
            text = r.route > 3 ? std::format("{} R", r.route - 3) : std::format("{}", r.route + 1);
        }
        s.setText(name("Route"), toWide(text));
        s.setText(name("NumCrew"), toWide(std::format("{}", r.crew)));
        const auto& prizes = kHostPrizes[std::clamp(r.prize, 0, 6)];
        const char* const places[3] = {"1st", "2nd", "3rd"};
        for (size_t k = 0; k < 3; ++k) {
            s.setText(name(places[k]), toWide("$" + groupThousands(prizes[k] / 1000, language) + "k"));
        }
    }
    m_cost = m_setup.cost;
    s.setText("CostValue", toWide(moneyText(m_cost, language)));
    s.setText("CashValue", toWide(moneyText(career.profile.money, language)));
    if (career.profile.money < m_cost) {
        m_reason = 2;
        colour("CashValue", kHostRed);
    } else {
        colour("CashValue", kHostWhite);
    }
    const int32_t coming = attendance(host);
    s.setText("Attendance", toWide(std::format("{}", coming)));
    colour("Attendance", coming < 2 ? kHostRed : kHostWhite);
}

// 0x004CD6F0: the hosted event. Its day of the month and month the calendar's +0xF8C and +0xF90, the cost paid (as
// 0x004B8200), its type, location (+0x0D), crew 0, fee 5000, class and region or make; for each round kept (at +0x1C,
// 0x38 bytes; one fits the event) used, its type, the prize's places (+0x2C ..; their sum into the event's +0x18) and
// by the type: a point to point the location crew's route and km of the route, team (+0x17) the round's, laps 1,
// +0x0C 6; a circuit the same with the laps of 0x00719044 and the km times them; a sprint the crew's sprint (+0x24C,
// +0x250 + 4 x the route), team 1, laps 1, +0x0C 3; +0x54 the rounds. For each round: +0x16 and +0x18 0, +0x08 1 for
// a team over one, the crews (+0x0D) cleared; the crews 0 .. 8 shuffled by 50 swaps of rand() % 9 and rand() % 9,
// then in that order, while the round has under 6 racers (+0x18) and fewer crews than +0x0C, each crew the setup has
// coming (+0x3C + 4 x crew; also its +0x56) takes a place (+0x18 by the team, +0x16 one more, its +0x0D flag). The
// time of day and weather (0x004A4410), the profile's +0x620 the type; the event into the calendar's first free slot
// (used); the event of that day chosen ([0x00728C50], the profile's +0x610; [0x00728C5C], [0x00728C58] its day and
// month), both note lists emptied, the days passed (0x004A6530) to it and the meeting (0x004C27A0).
void HostMeeting::makeEvent(CareerHost& host) {
    GameData* data = host.gameData();
    Career& career = *data->career;
    Calendar& calendar = career.calendar;
    CareerProfile& p = career.profile;
    CalendarEvent e{};
    e.challengeIndex = -1;  // A hosted meeting is not a crew challenge.
    e.dayOfMonth = static_cast<int8_t>(calendar.chosenDayOfMonth);
    e.month = calendar.chosenMonth;
    pay(career, m_setup.cost);
    e.type = m_setup.type;
    e.location = static_cast<int8_t>(m_setup.location);
    e.crew = 0;
    e.entranceFee = 5000;
    e.carClass = m_setup.carClass;
    if (m_setup.criteriaType == CarCriteria::Region) e.region = m_setup.region;
    if (m_setup.criteriaType == CarCriteria::Make) e.make = m_setup.make;
    e.roundCount = static_cast<int8_t>(m_roundList.size());
    const Crew& crew = career.crews[static_cast<size_t>(std::clamp(m_setup.location, 0, 8))];
    if (!m_roundList.empty()) {
        // Stock problem: past the first round the rounds are written over the event's fields after its round.
        const HostRound& h = m_roundList.front();
        EventRound& r = e.round;
        const auto& prizes = kHostPrizes[std::clamp(h.prize, 0, 6)];
        r.prize = {prizes[0], prizes[1], prizes[2]};
        r.type = h.type;
        r.ended = 0;
        r.used = 1;
        e.prizeTotal += prizes[0] + prizes[1] + prizes[2];
        const auto at = static_cast<size_t>(std::max(h.route, 0));
        if (h.type == EventKind::Sprint) {
            r.routeIndex = h.route;
            r.route = crew.sprintRoute;  // Stock problem: +0x24C + 4 x the route (one sprint route here).
            r.mostCrews = 3;
            r.teamSize = 1;
            r.laps = 1;
            r.km = crew.sprintKm;
        } else if (h.type == EventKind::Circuit) {
            r.teamSize = static_cast<int8_t>(h.crew);
            r.route = at < crew.circuitRoute.size() ? crew.circuitRoute[at] : 0;
            const int32_t laps = kHostLaps[std::clamp(h.laps, 0, 10)];
            r.laps = static_cast<int8_t>(laps);
            r.mostCrews = 6;
            r.routeIndex = h.route;
            r.km = static_cast<float>(laps) * (at < crew.circuitKm.size() ? crew.circuitKm[at] : 0.0f);
        } else if (h.type == EventKind::PointToPoint) {
            r.teamSize = static_cast<int8_t>(h.crew);
            r.routeIndex = h.route;
            r.route = at < crew.p2pRoute.size() ? crew.p2pRoute[at] : 0;
            r.km = at < crew.p2pKm.size() ? crew.p2pKm[at] : 0.0f;
            r.mostCrews = 6;
            r.laps = 1;
        }
        r.opponents = 0;
        r.teams = r.teamSize > 1 ? 1 : 0;
        r.drivers = 0;
        r.crews.fill(0);
        std::array<int32_t, 9> order{0, 1, 2, 3, 4, 5, 6, 7, 8};
        CrtRandom& random = gameRandom();
        for (int n = 0; n < 50; ++n) {
            const auto a = static_cast<size_t>(static_cast<uint32_t>(random.next()) % 9);
            const auto b = static_cast<size_t>(static_cast<uint32_t>(random.next()) % 9);
            std::swap(order[a], order[b]);
        }
        const auto coming = [&](int32_t k) {
            return k == 0 ? 0 : m_setup.attend[static_cast<size_t>(k - 1)];  // +0x3C + 4 x crew
        };
        for (size_t j = 0; j < 9; ++j) {
            const int32_t k = order[j];
            e.attend[static_cast<size_t>(k)] = static_cast<uint8_t>(coming(k));
            if (r.drivers >= 6 || r.opponents >= r.mostCrews) break;
            if (coming(k) != 0) {
                r.drivers = static_cast<int8_t>(r.drivers + r.teamSize);
                ++r.opponents;
                r.crews[static_cast<size_t>(k)] = 1;
            } else {
                r.crews[static_cast<size_t>(k)] = 0;
            }
        }
    }
    eventConditions(e, gameRandom());
    p.lastHostedType = static_cast<int32_t>(e.type);
    for (CalendarEvent& slot : calendar.events) {
        if (slot.used != 0) continue;
        slot = e;
        slot.used = 1;
        break;
    }
    CalendarEvent* chosen = nullptr;
    for (CalendarEvent& slot : calendar.events) {
        if (slot.used != 0 && slot.dayOfMonth == e.dayOfMonth && slot.month == e.month) {
            chosen = &slot;
            break;
        }
    }
    career.meetingEvent = chosen;
    g_selectedDay = e.dayOfMonth;
    g_selectedMonth = e.month;
    clearNotes(career, career.meeting);
    clearNotes(career, career.respectChanges);
    // Stock problem: without the event (a full calendar) the stock code reads through a null pointer.
    if (chosen == nullptr) return;
    while (!(calendar.dayOfMonth == chosen->dayOfMonth && calendar.month == chosen->month)) {
        data->nextDay(gameRandom());
    }
    host.goToMeeting();
}

// 0x004CC3A0: the map (0x0045B6B0; place 0 at the pitch -0.4, 0x0045BE00; the route of the race window's type and
// route, 0x0045B720, as its texts); the texts (0x004CC820); +0x0C and +0x10 -1; Cash and Date.
void HostMeeting::enterSummary(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    m_summary.map.start(s);
    if (m_summary.map.view3d(s) != nullptr) m_summary.map.look(host, s, 0, -0.4f);
    {
        const Crew& crew = data->career->crews[static_cast<size_t>(std::clamp(m_setup.location, 0, 8))];
        const auto at = static_cast<size_t>(std::max(m_race.route, 0));
        // Stock problem: as in the race window, a route past the type's list reads the fields after it.
        int32_t mapRoute = -1;
        if (m_race.type == EventKind::Circuit) {
            if (at < crew.circuitRoute.size()) mapRoute = crew.circuitRoute[at];
        } else if (m_race.type == EventKind::Sprint) {
            if (at == 0) mapRoute = crew.sprintRoute;
        } else if (at < crew.p2pRoute.size()) {
            mapRoute = crew.p2pRoute[at];
        }
        const auto& routes = data->tables.routes;
        if (mapRoute >= 0 && static_cast<size_t>(mapRoute) < routes.size()) {
            m_summary.map.route(host, s, routes[static_cast<size_t>(mapRoute)].name, true);
        }
    }
    summaryTexts(host, s);
    m_summary.answer = -1;
    m_summary.confirm = -1;
    careerCashTexts(s, *data->career, host.language().index, texts);
}

// 0x004CC500: the map's frame (0x0045BC40). A "yes" to cancel (+0x0C 1): CMSMainMenu. BACK (CLICK): "Cancel event
// setup?" and "ARE YOU SURE?" (type 3, on NO) into +0x0C. SELECT on menu's item of value 1 with no round (CLICK):
// CMSHostMeetingSetupRace. A "yes" to create (+0x10 1): the event made (0x004CD6F0). SELECT on the item
// of value 2 (CLICK): a reason shows its note (476 + it); no round: reason 3 and "You need at least one race" (479);
// else the setup's +0x38 set and "Create event. Cost: " (1826), the cost and "ARE YOU SURE?" (type 3, on NO) into
// +0x10.
void HostMeeting::updateSummary(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    m_summary.map.frame(host, s);
    const float item = s.menuSize("menu") != 0 ? s.menuItemValue("menu", s.selectedMenuItem("menu")) : 0.0f;
    if (m_summary.answer == 1) {
        host.request("CMSMainMenu", SwitchMode::Replace);
        return;
    }
    if (event == input::kBack) {
        host.sound("CLICK");
        host.message(MessageType::YesNo, texts.at(kTextCancelSetup) + L"\n" + texts.at(kTextAreYouSure), false,
                     &m_summary.answer, true);
    } else if (event == input::kSelect && m_rounds < 1 && item == 1.0f) {
        host.sound("CLICK");
        host.request("CMSHostMeetingSetupRace", SwitchMode::Replace);
    }
    if (m_summary.confirm == 1) {
        makeEvent(host);
        return;
    }
    if (event != input::kSelect || item != 2.0f) return;
    host.sound("CLICK");
    if (m_reason != 0) {
        host.message(MessageType::Continue, texts.at(kTextNoHost + static_cast<size_t>(m_reason)), false, nullptr);
    } else if (m_rounds < 1) {
        m_reason = 3;
        host.message(MessageType::Continue, texts.at(kTextNeedRace), false, nullptr);
    } else {
        m_setup.defaults = 1;
        host.message(MessageType::YesNo,
                     texts.at(kTextCreateEvent) + toWide(moneyText(m_cost, host.language().index)) + L"\n" +
                         texts.at(kTextAreYouSure),
                     false, &m_summary.confirm, true);
    }
}

}  // namespace oj::frontend
