// hud_pause.

#include "Game/Race/Hud/RacePause.h"
#include "Game/Race/Hud/HudInternal.h"

#include "Core/Data/GameTexts.h"
#include "Core/Input/Input.h"
#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Network/NetSession.h"
#include "Core/Network/OnlineRecords.h"

#include <algorithm>

namespace oj {

using namespace hud;

namespace {

}  // namespace

RacePause::RacePause(ui::WindowManager& windows, const TextTable& texts)
    : m_windows(windows), m_texts(texts) {}

// 0x0044E920 with one player: the pause made sure of (the race state's +0x20, as 0x004263E0); the race's respect
// worked out as for the last place (0x004BCD90 with its first flag); BackMenu (none in the window) and the menu back
// to their items (0x005AECF0) with their first item selected; FRIENDS taken out without the friends' object ([0x0075EF7C]; +0x24 whether it stays),
// PLAYERS without the matchmaking, RESTART on the network and in the career (but in a challenge, types 0 .. 2, with
// an outcome other than 1, and in a cruise of a crew's challenge), each with BackMenu's first item; PLAYER hidden with
// one player; every COUNTDOWNBOX hidden.
bool RacePause::enter(const HudRace& race) {
    m_screen = m_windows.open("hud_pause");
    if (!m_screen->loaded()) {
        m_screen.reset();
        return false;
    }
    ui::Screen& s = *m_screen;
    raceRespectOf(race, true, false);
    if (const int menu = s.find("Menu"); menu >= 0) {
        const std::string& m = s.nameOf(menu);
        if (s.find("BackMenu") >= 0) {
            s.restoreMenu("BackMenu");
            s.selectMenuItem("BackMenu", 0);
        }
        s.restoreMenu(m);
        s.selectMenuItem(m, 0);
        const auto remove = [&](const char* command) {
            s.removeMenuItem(m, command);
            if (s.find("BackMenu") >= 0 && s.menuSize("BackMenu") != 0) s.removeMenuItemAt("BackMenu", 0);
        };
        if (net::onlineFriends() == nullptr) {
            remove("friends");
            m_friends = 0;
        } else {
            m_friends = 1;
        }
        if (net::matchmaking() == nullptr) remove("players");
        if (race.networked) remove("restart");
        if (race.mode == RaceMode::Career) {
            const RaceType type = race.type;
            // 0x0044EB23: a cruise keeps RESTART only for a crew's challenge (the current event's +0x05, 0x004A64E0).
            const CalendarEvent* event = type == RaceType::Cruise && race.career != nullptr
                                             ? currentEvent(const_cast<Career*>(race.career)->calendar)
                                             : nullptr;
            const bool cruiseOff = type == RaceType::Cruise && (event == nullptr || event->challenge == 0);
            if (type < RaceType::TimedLap || type == RaceType::Solo || type > RaceType::Cruise ||
                (type <= RaceType::Speed && race.challenge == ChallengeOutcome::Met) || cruiseOff) {
                remove("restart");
            }
        }
    }
    // 0x0044ECD7: a client of a network game reads "Leave Session" (2008) on QUIT.
    if (const int menu = s.find("Menu"); menu >= 0 && race.networked && !race.host) {
        const std::string& m = s.nameOf(menu);
        for (size_t i = 0; i < s.menuSize(m); ++i) {
            if (_stricmp(s.menuItemCommand(m, i).c_str(), "quit") == 0) {
                s.setMenuItemText(m, i, m_texts.at(kTextLeaveSessionItem));
                break;
            }
        }
    }
    // 0x0044EBDB: with more than one player and a player who paused, PLAYER "Player" (167), " " and " <its number>"
    // (Stock problem: two spaces); the second player's pause takes FRIENDS and RESTART out of Menu and BackMenu's first
    // item twice. Else PLAYER hidden.
    if (const int player = s.find("player"); player >= 0) {
        if (race.players.size() > 1 && race.pausing >= 0) {
            s.setHidden(player, false);
            if (race.pausing == 1) {
                if (const int menu = s.find("Menu"); menu >= 0) {
                    const std::string m = s.nameOf(menu);
                    s.removeMenuItem(m, "friends");
                    s.removeMenuItem(m, "restart");
                }
                if (s.find("BackMenu") >= 0) {
                    for (int k = 0; k < 2; ++k) {
                        if (s.menuSize("BackMenu") != 0) s.removeMenuItemAt("BackMenu", 0);
                    }
                }
            }
            s.setText(player, m_texts.at(kTextPlayer) + L" " + std::format(L" {}", race.pausing == 1 ? 2 : 1));
        } else {
            s.setHidden(player, true);
        }
    }
    for (int e = 0; e < s.size(); ++e) {
        if (_stricmp(s.nameOf(e).c_str(), "COUNTDOWNBOX") == 0) s.setHidden(e, true);
    }
    return true;
}

// 0x0044EEA0 offline with one player (the window manager first ends the transitions and gives the menu UP and DOWN):
// the cruise's quiet time (+0x14) 0.1 each frame; quitting (+0x20) asks for closedown; a restart (+0x18) once the
// lap's records were looked at (+0x1C) asks for restart_race (a sprint's racers' points cleared and its heat -1; the
// player who paused, [G+0x34]+0x0C's +0x10, back to 0, the only one here); the records looked at (lookAtRecords) set
// quitting unless restarting. A "yes" to QUIT (+0x0C, the answer +0x2C) has an arcade championship's result
// (0x0042B290 with 100) and the career's event crew's +0x2C8 set (an event with +0x02) and the records looked at
// again, the replay's recording stopped ([G+0x9C] +0x828 0, vt+0x2C) and on the network a client or a pink slip
// leaving the session (0x004EA4E0 with 1; online 0x004E2740), the host taking everyone back to the lobby (0x004EA850);
// a "yes" to RESTART (+0x10) sets the restart and the records looked at again. (The menus' removals once online, +0x24
// with [0x0075EF7C], do not happen off line.) Then the event: BACK goes on; SELECT (CLICK): CONTINUE goes on, QUIT
// asks "QUIT RACE?" (type 9: with "ARE YOU SURE?"; the career with the race cars chosen "QUIT RACE AND FINISH
// LAST?", with the profile's +0x61C "QUIT RACE AND LOSE BET?", else as type 9; off the career "QUIT RACE?"), each but
// the short ones with "ALL GAME PROGRESS WILL BE LOST." and "ARE YOU SURE?"; RESTART asks "RESTART RACE?" / "ARE YOU
// SURE?".
RaceWindowFrame RacePause::frame(const HudRace& race, uint32_t uiEvent, uint32_t secondEvent) {
    RaceWindowFrame out;
    if (m_screen == nullptr) return out;
    if (race.cruise != nullptr) race.cruise->pause = 0.1f;
    ui::Screen& s = *m_screen;
    if (uiEvent != input::kNone) s.finishTransitions();
    // 0x005B1207: Menu and BackMenu take the first player's event, else the second's (their +0x1A0, set by the
    // enter).
    if (const uint32_t menuEvent = uiEvent != input::kNone ? uiEvent : secondEvent; menuEvent != input::kNone) {
        s.menuEvent(menuEvent);
    }
    // 0x0044EEC1: the friends' object gone: FRIENDS out (and BackMenu's first item).
    if (m_friends != 0 && net::onlineFriends() == nullptr) {
        if (const int menu = s.find("Menu"); menu >= 0) s.removeMenuItem(s.nameOf(menu), "friends");
        if (s.find("BackMenu") >= 0 && s.menuSize("BackMenu") != 0) s.removeMenuItemAt("BackMenu", 0);
        m_friends = 0;
    }
    if (m_quit != 0) {
        out.window = "closedown";
        return out;
    }
    if (m_restart != 0 && m_settled != 0) {
        // A sprint starts again from its first heat: the points (+0x138) of the session's racers' cars cleared and the
        // heat -1 (restart_race counts it). Stock problem: the heats' time (+0x134) is kept.
        if (race.type == RaceType::Sprint && race.race != nullptr) {
            const uint32_t racers = race.career != nullptr ? race.career->session.count : 0;
            for (const RaceCar* car : race.cars) {
                if (car != nullptr && car->competitor != nullptr && car->record >= 0 &&
                    static_cast<uint32_t>(car->record) < racers) {
                    car->competitor->points = 0.0f;
                }
            }
            race.race->rules().heat = -1;
        }
        out.window = "restart_race";
        m_settled = 1;
        return out;
    }
    if (m_settled == 0) {
        out.highScores = lookAtRecords(race, false);
        if (m_restart == 0) m_quit = 1;
        m_settled = 1;
        return out;
    }
    if (m_quitAsked != 0) {
        if (m_answer == 1) {
            // An arcade championship's result (0x0042B290 with 100).
            if (race.mode == RaceMode::Arcade && race.race != nullptr)
                const_cast<RaceState*>(race.race)->arcadeResult(100);
            out.replayOff = true;  // 0x0044F3E1
            // On the network the host takes everyone back to the lobby but in a pink slip ([0x00728D00]); otherwise the
            // session is left.
            if (race.networked) out.netEnd = race.host && net::onlineHosted().career.pinkslip == 0 ? 2 : 1;
            if (race.mode == RaceMode::Career && race.career != nullptr) {
                Career& career = *const_cast<Career*>(race.career);
                const CalendarEvent* event = currentEvent(career.calendar);
                if (event != nullptr && event->hostedByPlayer != 0) {
                    career.crews[static_cast<size_t>(event->crew)].hostLost = 1;
                }
            }
            m_settled = 0;
            return out;
        }
        m_quitAsked = 0;
        m_answer = 0;
    }
    if (m_restartAsked != 0) {
        if (m_answer == 1) {
            m_settled = 0;
            m_restart = 1;
            out.stopRecord = true;  // 0x0044F490
        } else {
            m_restartAsked = 0;
        }
        m_answer = 0;
    }
    // 0x0044F0B5: each player's event in turn (the window's +0x88, +0x8C; [G+0x6C]+0x58 players): BACK the race goes
    // on (0x0044ED90, the views' window 0x004584C0 with 1) with that player's controller on DRIVER1 again; SELECT on
    // Menu's item (CLICK) as below, CONTINUE making every player's controller DRIVER1 again.
    const std::array<uint32_t, 2> events = {uiEvent, secondEvent};
    const size_t players = std::clamp<size_t>(race.players.size(), 1, 2);
    for (size_t player = 0; player < players; ++player) {
        if (events[player] == input::kBack) {
            out.goOn = true;
            out.player = static_cast<int>(player);
            return out;
        }
        if (events[player] == input::kSelect) selected(race, out, static_cast<int32_t>(player));
    }
    return out;
}

void RacePause::selected(const HudRace& race, RaceWindowFrame& out, int32_t player) {
    ui::Screen& s = *m_screen;
    const int menu = s.find("Menu");
    if (menu < 0) return;
    const std::string& m = s.nameOf(menu);
    if (s.sound) s.sound("CLICK");  // 0x0044F51C
    const std::string command = s.menuItemCommand(m, s.selectedMenuItem(m));
    const auto line = [&](size_t index) -> const std::wstring& { return m_texts.at(index); };
    if (_stricmp(command.c_str(), "quit") == 0) {
        std::wstring text;
        if (race.type == RaceType::TestTrack) {
            text = line(kTextQuitRace) + L"\n" + line(kTextAreYouSure);
        } else if (race.mode != RaceMode::Career && race.networked && net::matchmaking() != nullptr &&
                   net::onlineHosted().mode == net::OnlineKind::Career && net::onlineHosted().career.pinkslip != 0 &&
                   race.net != nullptr) {
            // 0x0044F5B1: an online career's pink slip: "QUIT RACE?" .. once a player left the race (0x004EBCF0),
            // else "Quitting will result in a race forfeit and loss of your car." and "ARE YOU SURE?".
            text = (race.net->playerLeftRace() ? line(kTextQuitRace) : line(kTextForfeit)) + L"\n" +
                   line(kTextAreYouSure);
        } else if (race.mode != RaceMode::Career && race.networked && !race.host) {
            // 0x0044F530: a client of a network game "Leave game session?".
            text = line(kTextLeaveSession) + L"\n" + line(kTextAreYouSure);
        } else if (race.mode == RaceMode::Career) {
            const int32_t bet = race.career != nullptr ? race.career->profile.watching : 0;
            if (race.raceCarsChosen || bet != 0) {
                text = line(race.raceCarsChosen ? kTextQuitFinishLast : kTextQuitLoseBet) + L"\n" +
                       line(kTextProgressLost) + L"\n" + line(kTextAreYouSure);
            } else {
                text = line(kTextQuitRace) + L"\n" + line(kTextAreYouSure);
            }
        } else {
            text = line(kTextQuitRace) + L"\n" + line(kTextProgressLost) + L"\n" + line(kTextAreYouSure);
        }
        m_quitAsked = 1;
        out.question = text;
        out.answer = &m_answer;
    } else if (_stricmp(command.c_str(), "continue") == 0) {
        out.goOn = true;
        out.player = -1;
    } else if (_stricmp(command.c_str(), "restart") == 0) {
        m_restartPlayer = player;
        m_restartAsked = 1;
        out.question = line(kTextRestartRace) + L"\n" + line(kTextAreYouSure);
        out.answer = &m_answer;
    } else if (_stricmp(command.c_str(), "options") == 0) {
        // 0x0044F7E6: opts_ingame_bg in place of the windows (mode 2).
        out.window = "opts_ingame_bg";
    } else if (_stricmp(command.c_str(), "friends") == 0 || _stricmp(command.c_str(), "players") == 0) {
        // 0x0044F718: online_ingame_bg (mode 2) with online_friends or online_players ([0x0072182C]).
        out.window = "online_ingame_bg";
        out.onlineWindow = _stricmp(command.c_str(), "friends") == 0 ? "online_friends" : "online_players";
    } else {
        log::info("race: the pause's {} does nothing", command);  // no item of hud_pause.ui
    }
}

void RacePause::draw(float seconds, float dt) {
    if (m_screen == nullptr) return;
    m_screen->update(dt);
    m_screen->draw(seconds, dt);
}

}  // namespace oj
