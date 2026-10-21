// A showoff's results.

#include "Game/Race/Hud/CruiseResults.h"
#include "Game/Race/Hud/HudInternal.h"

#include "Core/Career/Money.h"
#include "Core/Data/GameTexts.h"
#include "Core/Input/Input.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Network/OnlineRecords.h"

namespace oj {

using namespace hud;

void CruiseResults::netBoard(const std::array<std::wstring, 3>& lines, const std::array<bool, 3>& shown) {
    if (m_screen == nullptr) return;
    for (size_t i = 0; i < 3; ++i) {
        const int e = m_screen->find("mplay" + std::to_string(i + 1));
        if (e < 0 || !shown[i]) continue;
        m_screen->setHidden(e, false);
        m_screen->setText(e, std::to_wstring(i + 1) + m_texts.at(kTextOrdinals + i) + L" " + lines[i]);
    }
}

CruiseResults::CruiseResults(ui::WindowManager& windows, const TextTable& texts)
    : m_windows(windows), m_texts(texts) {}

// 0x00451270: the speech and the 3D world's voices paused (resultsQuiet); the career's respect in counter
// (careerCounter); each player's messages emptied; the controller let go; score the cruise's score with its thousands
// marked; +0x10 set in the career before its race cars are chosen; +0x0C when the save system does not save; the menu
// back to its items with its first selected; online RESTART and REPLAY out with the frame's buttons hidden; in
// the career RESTART out unless the event is a crew's challenge (+0x05) whose target (+0x2E0 of the challenge) the
// score has not reached; +0x14 .. +0x1C cleared; the mplay lines hidden off the network (0x004B09D0; the online score
// on the network 0x004B0A50, Main's netCruiseBoard); QUIT selected.
bool CruiseResults::enter(const HudRace& race, RaceHud* hud) {
    m_screen = m_windows.open("hud_cruise_results");
    if (!m_screen->loaded()) {
        m_screen.reset();
        return false;
    }
    resultsQuiet();
    ui::Screen& s = *m_screen;
    careerCounter(s, race);
    if (hud != nullptr) {
        for (size_t i = 0; i < race.players.size(); ++i) hud->messages(i).clear();
    }
    const int32_t score = race.cruise != nullptr ? race.cruise->score() : 0;
    if (const int e = s.find("score"); e >= 0) s.setText(e, widen(groupThousands(score, m_windows.language().index)));
    m_recordsDone = race.mode == RaceMode::Career && !race.raceCarsChosen ? 1 : 0;
    m_notSaving = race.saving ? 0 : 1;
    if (const int menu = s.find("Menu"); menu >= 0) {
        const std::string& m = s.nameOf(menu);
        s.restoreMenu(m);
        s.selectMenuItem(m, 0);
        if (race.networked) {
            // 0x0045163F: on the network the menu itself and the frame's buttons hidden (the round's end takes the
            // window away).
            s.removeMenuItem(m, "restart");
            s.removeMenuItem(m, "replay");
            s.setHidden(menu, true);
            for (const char* name : {"FRAME_BOTTOMPANEL", "FRAME_ANILINE3", "FRAME_SEL_BUT", "FRAME_SEL_TEXT"}) {
                if (const int e = s.find(name); e >= 0) s.setHidden(e, true);
            }
        }
        if (race.mode == RaceMode::Career && race.career != nullptr) {
            Career& career = *const_cast<Career*>(race.career);
            const CalendarEvent* e = currentEvent(career.calendar);
            bool keep = false;
            if (e != nullptr && e->challenge != 0 && e->crew >= 0 &&
                static_cast<size_t>(e->crew) < career.crews.size() && e->challengeIndex >= 0 && e->challengeIndex < 3) {
                const Crew& crew = career.crews[static_cast<size_t>(e->crew)];
                keep = crew.challenges[static_cast<size_t>(e->challengeIndex)].value > score;
            }
            if (!keep) s.removeMenuItem(m, "restart");
        }
        for (size_t i = 0; i < s.menuSize(m); ++i) {
            if (_stricmp(s.menuItemCommand(m, i).c_str(), "quit") == 0) {
                s.selectMenuItem(m, i);
                break;
            }
        }
    }
    for (const char* name : {"mplay1", "mplay2", "mplay3"}) {
        if (const int e = s.find(name); e >= 0 && !race.networked) s.setHidden(e, true);
    }
    // 0x004517E5: an online career's showoff ends its race on the statistics (0x004ED120; the stock's reporter is
    // there online).
    if (net::matchmaking() != nullptr && net::onlineHosted().mode == net::OnlineKind::Career &&
        net::stats::reporter() != nullptr) {
        net::stats::reportRaceLeft();
    }
    m_quit = 0;
    m_restart = 0;
    m_answer = 0;
    return true;
}

// 0x00451830: the first frame (+0x10 0) gives the score to the online world record (with the statistics, 0x00451894)
// and looks at the route's record with it: a better one kept with the driver's name and the car's model (0x0040A220) and newhighscore_cms asked over the window
// (mode 5). A quit (+0x14) answered 1 (+0x1C) stops the replay's recording, tells the network and asks for closedown; a
// restart (+0x18) answered 1 stops the replay (0x00426F60) and asks for restart_race. SELECT off the network (CLICK):
// QUIT quits at once (its question made and dropped, a stock problem), REPLAY asks for start_replay, RESTART asks
// "RESTART RACE?" / "ARE YOU SURE?".
RaceWindowFrame CruiseResults::frame(const HudRace& race, uint32_t uiEvent) {
    RaceWindowFrame out;
    if (m_screen == nullptr) return out;
    ui::Screen& s = *m_screen;
    if (uiEvent != input::kNone) {
        s.finishTransitions();
        s.menuEvent(uiEvent);
    }
    if (m_recordsDone == 0) {
        m_recordsDone = 1;
        const auto score = static_cast<float>(race.cruise != nullptr ? race.cruise->score() : 0);
        net::stats::showoffResult(race.routeIndex, score);
        if (race.highScores != nullptr && race.tables != nullptr &&
            race.highScores->better(race.routeIndex, score, *race.tables)) {
            const HudCar* p = race.players.empty() ? nullptr : &race.players[0];
            const int32_t model =
                p != nullptr && p->record != nullptr && p->record->def != nullptr ? p->record->def->id : 0;
            race.highScores->keep(race.routeIndex, race.driverName, score, model, *race.tables);
            log::info("race: a new record of route {}: {}", race.routeIndex, score);
            out.highScores = true;
            return out;
        }
    }
    if (m_notSaving == 0) m_notSaving = 1;
    if (m_quit != 0) {
        if (m_answer == 1) out.window = "closedown";
        m_quit = 0;
        m_answer = 0;
    }
    if (m_restart != 0) {
        if (m_answer == 1 && out.window.empty()) {
            out.window = "restart_race";
            out.stopRecord = true;  // 0x00451A48
        }
        m_restart = 0;
        m_answer = 0;
    }
    if (uiEvent != input::kSelect || race.networked) return out;
    if (m_screen->sound) m_screen->sound("CLICK");  // 0x00451AAB
    const int menu = s.find("Menu");
    if (menu < 0) return out;
    const std::string& m = s.nameOf(menu);
    const std::string command = s.menuItemCommand(m, s.selectedMenuItem(m));
    if (_stricmp(command.c_str(), "quit") == 0) {
        m_quit = 1;
        m_answer = 1;
    } else if (_stricmp(command.c_str(), "replay") == 0) {
        if (out.window.empty()) {
            out.window = "start_replay";
            out.stopRecord = true;  // 0x00451157, 0x00451B70
        }
    } else if (_stricmp(command.c_str(), "restart") == 0) {
        m_restart = 1;
        out.question = m_texts.at(kTextRestartRace) + L"\n" + m_texts.at(kTextAreYouSure);
        out.answer = &m_answer;
    }
    return out;
}

void CruiseResults::draw(float seconds, float dt) {
    if (m_screen == nullptr) return;
    m_screen->update(dt);
    m_screen->draw(seconds, dt);
}

}  // namespace oj
