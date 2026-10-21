// The crew race's setup (CMSCrewRaceSetup).

#include "Game/Frontend/Career/CrewRaceSetup.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>

namespace oj::frontend {

// 0x004D8AC0: +0x0C .. +0x14 0; Cash and Date. One member, a cruise or a challenge: the first racer member 0 (the
// profile's +0x5F4), the profile's +0x614 1 and CMSRaceSetup. Else back_text "LEAVE EVENT"; +0x24 0; the buttons
// driver1value .. 4 and selection, border, crewpic and driver 1 .. 4 hidden; for each member of the crew
// driver<n>value its crewmate's short name, border<n> shown and crewpic<n> its ui_crewmember_<number>; the slots
// (+0x18 .. +0x20) -1; Type the team size's line (491 + size); Choose "SELECT DRIVERS: " and the drivers left; every
// 0, 2, 3 and 4 hidden and 1 shown; the profile's +0x614 0; the button driver1value highlighted.
void CrewRaceSetup::enter(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    CareerProfile& p = career.profile;
    CalendarEvent* e = currentEvent(career.calendar);
    if (e == nullptr) return;
    const EventRound& round = *roundOf(career, *e);
    m_respectAnswer = 0;
    m_leaving = 0;
    m_leaveAnswer = 0;
    careerCashTexts(s, career, host.language().index, texts);
    if (p.memberCount == 1 || round.type == EventKind::Showoff || round.type == EventKind::Challenge) {
        p.raceMembers[0] = 0;
        p.raceCarsChosen = 1;
        host.request("CMSRaceSetup", SwitchMode::Replace);
        return;
    }
    s.setText("back_text", texts.at(kTextLeaveEventItem));
    m_chosen = 0;
    for (int n = 1; n <= 4; ++n) {
        s.setHidden(std::format("driver{}value", n), true);
        s.setHidden(std::format("selection{}", n), true);
        s.setHidden(std::format("border{}", n), true);
        s.setHidden(std::format("crewpic{}", n), true);
        s.setHidden(std::format("driver{}", n), true);
    }
    for (int32_t k = 0; k < p.memberCount && k < 4; ++k) {
        const auto number = static_cast<size_t>(career.members[static_cast<size_t>(k)]);
        // Stock problem: a member slot without a crewmate reads through its pointer; nothing shown then.
        if (number >= career.crewmates.size()) continue;
        const Crewmate& mate = career.crewmates[number];
        const std::string value = std::format("driver{}value", k + 1);
        s.setText(value, widenSigned(std::string_view(mate.shortName.data(), mate.shortName.size())));
        s.setHidden(value, false);
        s.setHidden(std::format("border{}", k + 1), false);
        s.setTexture(std::format("crewpic{}", k + 1), std::format("ui_crewmember_{:02}", mate.number));
        s.setHidden(std::format("crewpic{}", k + 1), false);
    }
    m_members.fill(-1);
    s.setText("Type", texts.at(kTextTeamSizes + static_cast<size_t>(std::max<int>(round.teamSize, 0))));
    s.setText("Choose", texts.at(kTextSelectDrivers) + toWide(std::format("{}", round.teamSize - m_chosen)));
    hideAll(s, "0");
    s.show("1", false);
    hideAll(s, "2");
    hideAll(s, "3");
    hideAll(s, "4");
    p.raceCarsChosen = 0;
    s.setCurrentButton("driver1value");
}

// 0x004D90F0: every 0 .. 4 hidden, then the one of the highlighted button's seventh character (1 .. 4, else 0) shown.
// Leaving answered as in CMSRaceSetup (+0x0C the respect message, +0x10 / +0x14 the event's end and CMSCalendar);
// BACK asks as there (into +0x14 or +0x0C). The button finished shown once the team is complete. SELECT on finished
// (0x004D9640): with the team complete (CLICK) the members into the profile's +0x5F4 (member 0 sets +0x614) and
// CMSRaceSetup, else "NO"; on a driver its member taken (0x004D9860: the first free slot, selection<n> shown; a
// complete team "NO") or given back (0x004D9790: selection<n> hidden), each with CLICK; a complete team shows and
// highlights Finished; Choose the drivers left.
void CrewRaceSetup::update(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    CareerProfile& p = career.profile;
    CalendarEvent* e = currentEvent(career.calendar);
    if (e == nullptr) return;
    const std::string button = s.currentButton();
    for (const char* name : {"0", "1", "2", "3", "4"}) hideAll(s, name);
    {
        const char c = button.size() > 6 ? button[6] : '\0';
        static constexpr const char* kDigits[] = {"1", "2", "3", "4"};
        s.show(c >= '1' && c <= '4' ? kDigits[c - '1'] : "0", false);
    }
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
        host.sound("CLICK");
        if (!specialEvent(*e)) {
            host.message(MessageType::YesNo, texts.at(kTextLeaveEvent) + L"\n" + texts.at(kTextEventClosed), false,
                         &m_leaveAnswer, true);
        } else {
            host.message(MessageType::YesNo, texts.at(kTextLeaveSpecial) + L"\n" + texts.at(kTextEventClosed), false,
                         &m_respectAnswer, true);
        }
    }
    const EventRound& round = *roundOf(career, *e);
    const int team = std::clamp<int>(round.teamSize, 0, 3);
    s.setHidden("finished", round.teamSize != m_chosen);
    if (event != input::kSelect) return;
    if (_stricmp(button.c_str(), "finished") == 0) {
        if (m_chosen != round.teamSize) {
            host.sound("NO");
            return;
        }
        host.sound("CLICK");
        for (int k = 0; k < team; ++k) {
            p.raceMembers[static_cast<size_t>(k)] = m_members[static_cast<size_t>(k)];
            if (m_members[static_cast<size_t>(k)] == 0) p.raceCarsChosen = 1;
        }
        host.request("CMSRaceSetup", SwitchMode::Replace);
        return;
    }
    const int member = (button.size() > 6 ? button[6] : '\0') - '1';
    bool taken = false;
    for (int k = 0; k < team; ++k) taken = taken || m_members[static_cast<size_t>(k)] == member;
    if (taken) {
        host.sound("CLICK");
        for (int k = 0; k < team; ++k) {
            if (m_members[static_cast<size_t>(k)] != member) continue;
            m_members[static_cast<size_t>(k)] = -1;
            --m_chosen;
            s.setHidden(std::format("selection{}", member + 1), true);
        }
    } else if (m_chosen == round.teamSize) {
        host.sound("NO");
    } else {
        host.sound("CLICK");
        for (int k = 0; k < team; ++k) {
            if (m_members[static_cast<size_t>(k)] != -1) continue;
            m_members[static_cast<size_t>(k)] = member;
            ++m_chosen;
            s.setHidden(std::format("selection{}", member + 1), false);
            break;
        }
    }
    if (round.teamSize == m_chosen) {
        s.setHidden("Finished", false);
        s.setCurrentButton("Finished");
    }
    s.setText("Choose", texts.at(kTextSelectDrivers) + toWide(std::format("{}", round.teamSize - m_chosen)));
}

}  // namespace oj::frontend
