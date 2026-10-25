// A challenge's results.

#include "Game/Race/Hud/ChallengeResults.h"
#include "Game/Race/Hud/HudInternal.h"

#include "Core/Data/GameTexts.h"
#include "Core/Input/Input.h"
#include "Core/Network/OnlineRecords.h"

namespace oj {

using namespace hud;

namespace {

}  // namespace

ChallengeResults::ChallengeResults(ui::WindowManager& windows, const TextTable& texts)
    : m_windows(windows), m_texts(texts) {}

// 0x00450B60: the speech paused ([G+0x4C]'s +0x0C set, its player's slots), the 3D world's voices paused (its manager,
// +0x1C, +0x20 with 1) and the sound system stepped ([G+0x44]; resultsQuiet); each player's messages emptied; the
// controller let go (0x00407FB0); the challenge's outcome 0 becomes 1, in the arcade winning the race (0x0042B350;
// RaceState::challengeResult, asked before the window comes in). MSG1: CHALLENGE FAILED for the outcomes 2 .. 5, else
// CHALLENGE COMPLETED; MSG2 by the outcome: 2 OUT OF TIME, 3 CAR DAMAGED, 4 YOU ARE TOO SLOW, 5 TARGET SPEED NOT
// REACHED, else for a speed challenge (type 2) or without the first player's car WELL DONE, else its best lap
// (0x0044FBD0). The menu back to its items (0x005AECF0), its first selected (its element +0x184 for the item's
// text: none in this window); RESTART taken out online and in the career outside a challenge (types 0 .. 2) not met (an
// outcome other than 1); with the outcome 1 QUIT selected.
bool ChallengeResults::enter(const HudRace& race, RaceHud* hud) {
    m_screen = m_windows.open("hud_challenge_results");
    if (!m_screen->loaded()) {
        m_screen.reset();
        return false;
    }
    resultsQuiet();
    ui::Screen& s = *m_screen;
    if (hud != nullptr) {
        for (size_t i = 0; i < race.players.size(); ++i) hud->messages(i).clear();
    }
    const ChallengeOutcome outcome = race.challenge;
    const auto line = [&](size_t index) -> const std::wstring& { return m_texts.at(index); };
    if (const int msg1 = s.find("msg1"); msg1 >= 0) {
        s.setText(msg1, line(outcome >= ChallengeOutcome::OutOfTime && outcome <= ChallengeOutcome::SpeedNotReached
                                 ? kTextChallengeFailed
                                 : kTextChallengeCompleted));
    }
    if (const int msg2 = s.find("msg2"); msg2 >= 0) {
        const RaceCar* car = race.players.empty() ? nullptr : race.players[0].car;
        switch (outcome) {
        case ChallengeOutcome::OutOfTime: s.setText(msg2, line(kTextWellDone + 2)); break;
        case ChallengeOutcome::CarDamaged: s.setText(msg2, line(kTextWellDone + 1)); break;
        case ChallengeOutcome::TooSlow: s.setText(msg2, line(kTextWellDone + 3)); break;
        case ChallengeOutcome::SpeedNotReached: s.setText(msg2, line(kTextWellDone + 4)); break;
        default:
            if (race.type == RaceType::Speed || car == nullptr || car->competitor == nullptr) {
                s.setText(msg2, line(kTextWellDone));
            } else {
                s.setText(msg2, widen(hudTime(car->competitor->bestLap)));
            }
        }
    }
    if (const int menu = s.find("Menu"); menu >= 0) {
        const std::string& m = s.nameOf(menu);
        s.restoreMenu(m);
        s.selectMenuItem(m, 0);
        if (race.networked) s.removeMenuItem(m, "restart");
        if (race.mode == RaceMode::Career &&
            (race.type < RaceType::TimedLap || race.type > RaceType::Speed || outcome == ChallengeOutcome::Met))
            s.removeMenuItem(m, "restart");
        if (outcome == ChallengeOutcome::Met) {
            for (size_t i = 0; i < s.menuSize(m); ++i) {
                if (_stricmp(s.menuItemCommand(m, i).c_str(), "quit") == 0) {
                    s.selectMenuItem(m, i);
                    break;
                }
            }
        }
    }
    m_quit = 0;
    m_restart = 0;
    m_answer = 0;
    return true;
}

// 0x00450F80: a quit (+0x0C) with the answer (+0x14) 1 stops the replay's recording ([G+0x9C]) and asks
// for closedown; a restart (+0x10) with it has the race's respect worked out (0x004BCD90) and asks for restart_race;
// the flag and the answer cleared. SELECT (CLICK): QUIT quits at once, RESTART asks "RESTART RACE?" / "ARE YOU SURE?",
// REPLAY asks for start_replay. Stock problem: QUIT makes the text "QUIT RACE?" / "ARE YOU SURE?" and drops it, asking
// nothing. The fixed build asks it, as RESTART its own.
RaceWindowFrame ChallengeResults::frame(const HudRace& race, uint32_t uiEvent) {
    RaceWindowFrame out;
    if (m_screen == nullptr) return out;
    ui::Screen& s = *m_screen;
    if (uiEvent != input::kNone) {
        s.finishTransitions();
        s.menuEvent(uiEvent);
    }
    if (m_quit != 0) {
        if (m_answer == 1) {
            out.window = "closedown";
            out.replayOff = true;  // 0x00450FB2
        }
        m_quit = 0;
        m_answer = 0;
    }
    if (m_restart != 0) {
        if (m_answer == 1 && out.window.empty()) {
            raceRespectOf(race, false, false);
            out.window = "restart_race";
            out.stopRecord = true;  // 0x00451029
        }
        m_restart = 0;
        m_answer = 0;
    }
    if (uiEvent != input::kSelect) return out;
    if (m_screen->sound) m_screen->sound("CLICK");  // 0x00451092
    const int menu = s.find("Menu");
    if (menu < 0) return out;
    const std::string& m = s.nameOf(menu);
    const std::string command = s.menuItemCommand(m, s.selectedMenuItem(m));
    if (_stricmp(command.c_str(), "quit") == 0) {
        m_quit = 1;
        out.question = m_texts.at(kTextQuitRace) + L"\n" + m_texts.at(kTextAreYouSure);
        out.answer = &m_answer;
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

void ChallengeResults::draw(float seconds, float dt) {
    if (m_screen == nullptr) return;
    m_screen->update(dt);
    m_screen->draw(seconds, dt);
}

}  // namespace oj
