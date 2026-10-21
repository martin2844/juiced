// CMSCrew: the player's crew.

#include "Game/Frontend/Career/CrewWindow.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

namespace oj::frontend {

namespace {

std::string g_crewLastButton;  // [0x007A2D4C]: the highlighted button the crew window saw last

}  // namespace

// The crewmate of a member place (1 .. 4; the profile's +0x500 + 4 x (place - 1)); -1 past the members.
int CrewWindow::mateOfPlace(CareerHost& host, int place) {
    GameData* data = host.gameData();
    const Career& career = *data->career;
    if (place > career.profile.memberCount || place < 1 || place > 4) return -1;
    const auto number = career.members[static_cast<size_t>(place - 1)];
    return number >= 0 && static_cast<size_t>(number) < career.crewmates.size() ? number : -1;
}

// 0x004C6070: Cash and Date; the buttons 1 .. 4 "-" and crewpic1 .. 4 hidden; for each member the button of its place
// its crewmate's short name (over several lines) and crewpic<n> shown with its ui_crewmember_<number>; every
// Selection2, Selection3 and Selection4 hidden.
void CrewWindow::list(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Career& career = *data->career;
    careerCashTexts(s, career, host.language().index, texts);
    for (int n = 1; n <= 4; ++n) {
        s.setText(std::format("{}", n), L"-");
        s.setHidden(std::format("crewpic{}", n), true);
    }
    for (int32_t k = 0; k < career.profile.memberCount && k < 4; ++k) {
        const int mate = mateOfPlace(host, k + 1);
        // Stock problem: a member slot without a crewmate reads through its pointer; nothing shown then.
        if (mate < 0) continue;
        const Crewmate& m = career.crewmates[static_cast<size_t>(mate)];
        const std::string button = std::format("{}", k + 1);
        s.setText(button, widenSigned(std::string_view(m.shortName.data(), m.shortName.size())));
        s.setMultiline(button, true);
        const std::string pic = std::format("crewpic{}", k + 1);
        s.setHidden(pic, false);
        s.setTexture(pic, std::format("ui_crewmember_{:02}", m.number));
    }
    hideAll(s, "Selection2");
    hideAll(s, "Selection3");
    hideAll(s, "Selection4");
}

// 0x004C64C0: every element with SELECTION in its name hidden, then selection<n> of the highlighted button shown;
// Skill1 .. 4Meter and skill1 .. 4 hidden. Without a crewmate (+0x0C) skill1 .. 3Meter 0 and RacesValue, WinsValue,
// TimeValue, Per_WinsValue "-". Else Skill1Meter its driving ability (+0x94) and Skill3Meter its recover (+0x9C), each
// x 0.01 with skill1 / skill3 shown (0 and hidden for crewmate 0); RacesValue its races (+0xB0), WinsValue its wins
// (+0xB4), Per_WinsValue wins / races x 100 ("%.1f%%", 0 without races), TimeValue the months since it joined (the
// calendar's month less its +0xB8) and "Months".
void CrewWindow::showStats(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Career& career = *data->career;
    const int mate = m_mate;
    s.hideContaining("SELECTION");
    s.show(std::format("selection{}", std::atoi(s.currentButton().c_str())), false);
    for (int n = 1; n <= 4; ++n) {
        s.setHidden(std::format("Skill{}Meter", n), true);
        s.setHidden(std::format("skill{}", n), true);
    }
    if (mate < 0) {
        for (int n = 1; n <= 3; ++n) s.setMeter(std::format("skill{}Meter", n), 0.0f);
        for (const char* name : {"RacesValue", "WinsValue", "TimeValue", "Per_WinsValue"}) s.setText(name, L"-");
        return;
    }
    const Crewmate& m = career.crewmates[static_cast<size_t>(mate)];
    const std::array<std::pair<int, float>, 2> skills = {std::pair{1, m.drivingAbility}, std::pair{3, m.recover}};
    for (const auto& [n, value] : skills) {
        const std::string meter = std::format("Skill{}Meter", n);
        if (m.number == 0) {
            s.setMeter(meter, 0.0f);
            continue;
        }
        s.setHidden(meter, false);
        s.setMeter(meter, value * 0.01f);
        s.setHidden(std::format("skill{}", n), false);
    }
    s.setText("RacesValue", toWide(std::format("{}", m.races)));
    s.setText("WinsValue", toWide(std::format("{}", m.wins)));
    float wins = 0.0f;
    if (m.races != 0) wins = static_cast<float>(m.wins) / static_cast<float>(m.races) * 100.0f;
    s.setText("Per_WinsValue", toWide(std::format("{:.1f}%", wins)));
    s.setText("TimeValue", toWide(std::format("{} ", career.calendar.month - m.joinDay)) +
                               texts.at(kTextMonthsWord));
}

// 0x004C5F60: the button 1 highlighted; its crewmate (+0x0C, none past the members); the stats (0x004C64C0); +0x10
// -1; the list (0x004C6070); with more than one member every element with MSG_ in its name hidden, else MSG_TEXT1 ..
// 3 over several lines.
void CrewWindow::enter(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    s.setCurrentButton("1");
    m_mate = mateOfPlace(host, std::atoi(s.currentButton().c_str()));
    showStats(host, s);
    m_unused = -1;
    list(host, s);
    if (career.profile.memberCount > 1) {
        s.hideContaining("MSG_");
    } else {
        for (const char* name : {"MSG_TEXT1", "MSG_TEXT2", "MSG_TEXT3"}) s.setMultiline(name, true);
    }
}

// 0x004C63D0: the phone's frame (0x004B60F0); BACK (CLICK) CMSMainMenu; a new highlighted button
// ([0x007A2D4C]) takes its crewmate (none past the members) and the stats again.
void CrewWindow::update(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    if (data == nullptr || !data->career) return;
    host.phoneFrame(s);
    if (event == input::kBack) {
        host.sound("CLICK");
        host.request("CMSMainMenu", SwitchMode::Replace);
    }
    if (s.currentButton() == g_crewLastButton) return;
    g_crewLastButton = s.currentButton();
    m_mate = mateOfPlace(host, std::atoi(g_crewLastButton.c_str()));
    showStats(host, s);
}

}  // namespace oj::frontend
