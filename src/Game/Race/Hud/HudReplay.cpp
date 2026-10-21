// hud_replay.

#include "Game/Race/Hud/HudReplay.h"
#include "Game/Race/Hud/HudInternal.h"

#include "Core/Data/GameTexts.h"
#include "Core/Input/Input.h"
#include "Core/Network/OnlineRecords.h"

#include <bit>

namespace oj {

using namespace hud;

HudReplay::HudReplay(ui::WindowManager& windows, const TextTable& texts)
    : m_windows(windows), m_texts(texts) {}

// 0x0044DA80: the fade (+0x0C, [0x007209FC]) 0, the window's fields cleared (RP_INFO PAUSED, the idle time 5 s, no
// answer); CRUISEMOVE and RP_TOOLTIP emptied (their effects held as by every text set); with fewer than 2 cars the
// RP_NEXT elements hidden; outside the attract (mode 4) the ATTRACT elements hidden and the first player's controller
// put on the UI map (0x00407FB0; here the driver's keys are read only while racing), in it every RP_ element.
bool HudReplay::enter(const HudRace& race, Replay& replay) {
    m_screen = m_windows.open("hud_replay");
    if (!m_screen->loaded()) {
        m_screen.reset();
        return false;
    }
    ui::Screen& s = *m_screen;
    replay.fade = 0.0f;
    m_progress = 0.0f;
    m_info = 3;
    m_quit = 0;
    m_selected = 0;
    m_ejectAsked = 0;
    m_saveAsked = 0;
    m_hidden = 0;
    m_answer = -1;
    m_hideAsked = 0;
    m_idle = 5.0f;
    m_moveTime = 0.0f;
    m_moves.clear();
    if (const int e = s.find("CRUISEMOVE"); e >= 0) s.setText(e, L"");
    if (const int e = s.find("RP_TOOLTIP"); e >= 0) s.setText(e, L"");
    const auto hideNamed = [&](const char* part) {
        for (int i = 0; i < s.size(); ++i) {
            if (s.nameOf(i).find(part) != std::string::npos) s.setHidden(i, true);
        }
    };
    if (replay.cars < 2) hideNamed("RP_NEXT");
    hideNamed(race.mode != RaceMode::Attract ? "ATTRACT" : "RP_");
    return true;
}

// 0x0044DCA0: every RP_ element shown again, RP_NEXT hidden with fewer than 2 cars.
void HudReplay::showControls(const HudRace&, const Replay& replay) {
    ui::Screen& s = *m_screen;
    s.showContaining("RP_", false);
    m_hidden = 0;
    if (replay.cars < 2) {
        for (int i = 0; i < s.size(); ++i) {
            if (s.nameOf(i).find("RP_NEXT") != std::string::npos) s.setHidden(i, true);
        }
    }
}

// 0x0044DD00: play on (0x00426B80), CLICK, the race paused; a replay saved already (or read from its file, its
// +0x30 0x00426650) quits at once, else "Quit Replay?" (433) asked on NO with the answer at +0x20, +0x28 set.
void HudReplay::eject(Replay& replay, ReplayWindowFrame& out) {
    replay.resume();
    if (m_screen != nullptr && m_screen->sound) m_screen->sound("CLICK");
    out.unpause = true;
    out.pause = true;
    if (replay.saved != 0) {
        m_quit = 1;
        return;
    }
    out.question = m_texts.at(kTextQuitReplay);
    out.answer = &m_answer;
    out.startNo = true;  // 0x0044DDDF: edi, 1 since 0x0044DD4C
    m_ejectAsked = 1;
}

// 0x0044DE20. A quit (+0x2C) asks for closedown. BACKGROUND's alpha is the fade; past 0.95 with the play ended, or with
// the whole played, it plays on and asks for restart_replay. In the attract (mode 4) any input, or the play past 0.8
// of its length, ends it: the race paused and the quit set. RP_TOOLTIP names the selected item; the controls hidden
// or shown (+0x30). The save asked (+0x24) asks for savereplay (mode 5) and quits; the eject answered quits (1) or
// goes on (0, the race on). The meter shows the part played; with a message box over the window (+0x34 of the
// manager) the play stops: no fast forward, the normal speed, paused (0x0044E1A4; here as the note opens, Main's
// controllerWatch, the window having no frames under a message). RP_INFO by the play: PAUSED
// once it ended, SLOW-MOTION under the normal speed, FAST FORWARD, else PLAYING. A showoff's marks of the frame queue
// the trick's (339 + it, not 15) and the special move's (359 + it) names; each is shown 2 s in CRUISEMOVE with the
// WARNING sound. The events: none counts the idle time down; else it is 5 s again: NEXT (6) with 2 cars or more and
// the play not ended takes the next car (CLICK); else the controls come back, SELECT on a shown menu with its
// transitions done: play (CLICK, play on), rew (CLICK, restart_replay), stop (CLICK: pause the play, or play on once
// it ended), slow (CLICK, play on, slow), ff (CLICK, play on, fast forward at 2, the queued moves dropped), eject,
// next_car (CLICK: each player's next view; on the bumper's view the director's trackside view 11 for 10000 s), hide
// (CLICK: hide unless the play ended); BACK ejects. A new selection while fast forwarding plays on at the normal
// speed. Ended, RP_NEXT_ and RP_SEL_ (with fewer than 2 items) hidden; else unless hidden both shown; with fewer than
// 2 cars RP_NEXT_ hidden.
ReplayWindowFrame HudReplay::frame(const HudRace& race, Replay& replay, float dt, uint32_t uiEvent, bool anyInput) {
    ReplayWindowFrame out;
    if (m_screen == nullptr) return out;
    ui::Screen& s = *m_screen;
    if (uiEvent != input::kNone) {
        s.finishTransitions();
        s.menuEvent(uiEvent);
    }
    if (m_quit != 0) {
        out.window = "closedown";
        return out;
    }
    const int background = s.find("BACKGROUND");
    if (replay.fade > std::bit_cast<float>(0x3F733333u) && replay.done()) {
        replay.resume();
        out.unpause = true;
        out.window = "restart_replay";
        return out;
    }
    const float played = replay.time / replay.length;
    if (played >= 1.0f) {
        replay.resume();
        out.unpause = true;
        out.window = "restart_replay";
        return out;
    }
    if (background >= 0) {
        const auto alpha = static_cast<uint32_t>(static_cast<uint8_t>(x87::chop(replay.fade * 255.0f)));
        s.setColour(background, alpha << 24);
    }
    if (race.mode == RaceMode::Attract) {
        if (!(played < 1.0f && played <= 0.8f && !anyInput)) {
            out.release = true;  // 0x0044DFF0
            out.pause = true;
            m_quit = 1;
        }
        return out;
    }
    const std::string menu = "RP_MENU";
    if (const int tip = s.find("RP_TOOLTIP"); tip >= 0) {
        static const std::array<const char*, 8> kCommands = {"hide", "next_car", "rew", "slow",
                                                             "play", "stop", "ff",       "eject"};
        const std::string command = s.menuItemCommand(menu, s.selectedMenuItem(menu));
        for (size_t i = 0; i < kCommands.size(); ++i) {
            if (_stricmp(command.c_str(), kCommands[i]) == 0) {
                s.setText(tip, m_texts.at(1649 + i));
                break;
            }
        }
    }
    if (m_hideAsked != 0) {
        s.hideContaining("RP_", true);
        m_hidden = 1;
    } else {
        showControls(race, replay);
    }
    if (m_saveAsked != 0) {
        if (m_answer == 1) out.window = "savereplay";
        m_quit = 1;
        return out;
    }
    if (m_ejectAsked != 0) {
        if (m_answer == 1) {
            m_quit = 1;
            return out;
        }
        if (m_answer == 0) out.unpause = true;
        m_ejectAsked = 0;
        m_answer = -1;
    }
    m_progress = replay.progress();
    if (const int meter = s.find("RP_METER"); meter >= 0) {
        s.setMeter(meter, m_progress);
        s.setMeterShown(s.nameOf(meter), m_progress);
    }
    if (replay.done()) {
        m_info = 3;
    } else if (replay.timeScale < 1.0f) {
        m_info = 2;
    } else {
        m_info = replay.fast != 0 ? 1 : 0;
    }
    if (const int info = s.find("RP_INFO"); info >= 0) s.setText(info, m_texts.at(504 + static_cast<size_t>(m_info)));
    if (race.type == RaceType::Cruise && replay.fast == 0) {
        const uint8_t trick = replay.marks[0];
        const uint8_t move = replay.marks[1];
        replay.marks[0] = 0;
        replay.marks[1] = 0;
        if (trick != 0 && trick != 15) m_moves.push_back(m_texts.at(339 + static_cast<size_t>(trick)));
        if (move != 0) m_moves.push_back(m_texts.at(359 + static_cast<size_t>(move)));
    }
    const int moveText = s.find("CRUISEMOVE");
    if (m_moveTime <= 0.0f) {
        if (!m_moves.empty()) {
            if (moveText >= 0) s.setText(moveText, m_moves.front());
            m_moves.pop_front();
            m_moveTime = 2.0f;
            if (s.sound) s.sound("WARNING");  // 0x0044E41B
        }
    } else {
        m_moveTime = m_moveTime - dt;
        if (m_moveTime <= 0.0f && moveText >= 0) s.clearText(moveText);
    }
    bool selectionCheck = true;
    if (uiEvent == input::kNone) {
        if (m_idle > 0.0f) m_idle = m_idle - dt;
    } else {
        m_idle = 5.0f;
        if (uiEvent == input::kSelect2) {
            if (replay.cars >= 2 && !replay.done()) {
                if (s.sound) s.sound("CLICK");  // 0x0044E5DE
                out.nextCar = true;
            }
        } else {
            m_hideAsked = 0;
            if (uiEvent == input::kSelect) {
                if (m_hidden == 0 && s.transitionsDone()) {
                    selectionCheck = false;
                    const std::string command = s.menuItemCommand(menu, s.selectedMenuItem(menu));
                    // Each but eject with CLICK (0x0044E4F7 .. 0x0044E879).
                    for (const char* c : {"play", "rew", "stop", "slow", "ff", "next_car", "hide"}) {
                        if (_stricmp(command.c_str(), c) == 0 && s.sound) s.sound("CLICK");
                    }
                    if (_stricmp(command.c_str(), "play") == 0) {
                        replay.resume();
                        out.unpause = true;
                    } else if (_stricmp(command.c_str(), "rew") == 0) {
                        out.window = "restart_replay";
                    } else if (_stricmp(command.c_str(), "stop") == 0) {
                        if (!replay.done()) {
                            replay.fast = 0;
                            replay.timeScale = 1.0f;
                            replay.state = ReplayState::Stopped;
                        } else {
                            replay.resume();
                            out.unpause = true;
                        }
                    } else if (_stricmp(command.c_str(), "slow") == 0) {
                        replay.resume();
                        replay.slow();
                        out.unpause = true;
                    } else if (_stricmp(command.c_str(), "ff") == 0) {
                        replay.resume();
                        replay.fastForward(2.0f);
                        out.unpause = true;
                        m_moves.clear();
                    } else if (_stricmp(command.c_str(), "eject") == 0) {
                        eject(replay, out);
                    } else if (_stricmp(command.c_str(), "next_car") == 0) {
                        if (!replay.done()) out.changeView = true;
                    } else if (_stricmp(command.c_str(), "hide") == 0) {
                        if (!replay.done()) m_hideAsked = 1;
                    }
                }
            } else if (uiEvent == input::kBack) {
                selectionCheck = false;
                eject(replay, out);
            }
        }
    }
    if (selectionCheck) {
        const size_t selected = s.selectedMenuItem(menu);
        if (selected != m_selected && replay.fast != 0) {
            replay.resume();
            out.unpause = true;
        }
        m_selected = selected;
    }
    const auto hideAll = [&](const char* part) {
        for (int i = 0; i < s.size(); ++i) {
            if (s.nameOf(i).find(part) != std::string::npos) s.setHidden(i, true);
        }
    };
    if (replay.done()) {
        hideAll("RP_NEXT_");
        if (s.menuSize(menu) < 2) hideAll("RP_SEL_");
    } else if (m_hideAsked == 0) {
        s.showContaining("RP_NEXT_", false);
        s.showContaining("RP_SEL_", false);
    }
    if (replay.cars < 2) hideAll("RP_NEXT_");
    return out;
}

// 0x0044DC20: the clock's +0x20 and +0x24 cleared; after a quit the race no longer a replay's (+0x828 0); with the
// sound the 3D world's pitch back to 1 (its manager's +0x30; nothing else sets it).
void HudReplay::leave(Replay& replay) {
    if (m_quit != 0) replay.playing = 0;
    if (audio::World* world = gameSounds().world) world->manager().setPitch(1.0f);
}

void HudReplay::draw(float seconds, float dt) {
    if (m_screen == nullptr) return;
    m_screen->update(dt);
    m_screen->draw(seconds, dt);
}

}  // namespace oj
